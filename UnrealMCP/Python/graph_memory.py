import os
import json
import logging
import threading
import asyncio
import time
import sys

# --- MONKEYPATCH LIGHTRAG WORKER POOL ---
# LightRAG 1.4.15 introduced a PriorityQueue WorkerPool that uses asyncio.wait_for
# which causes "Timeout should be used inside a task" bugs on Python 3.14/Windows.
# We bypass it entirely: pop all internal scheduler kwargs before forwarding to the real LLM.
import lightrag.utils
def _bypass_priority_limit_async_func_call(*args, **kwargs):
    def decorator(func):
        async def wrapper(*func_args, **func_kwargs):
            func_kwargs.pop("_priority", None)
            func_kwargs.pop("_timeout", None)
            func_kwargs.pop("_queue_timeout", None)
            return await func(*func_args, **func_kwargs)
        return wrapper
    return decorator
lightrag.utils.priority_limit_async_func_call = _bypass_priority_limit_async_func_call
# ----------------------------------------

log = logging.getLogger(__name__)

# Global instances
_lightrag_instance = None
_current_provider = None
_current_model = None


def _get_working_dir() -> str:
    try:
        from unreal_mcp_agent import _db_path
        if _db_path:
            base = os.path.dirname(_db_path)
        else:
            raise ImportError
    except Exception:
        base = os.path.join(os.path.dirname(__file__), "..", "Saved", "UnrealMCP")
    wd = os.path.join(base, "GraphStorage")
    os.makedirs(wd, exist_ok=True)
    return wd


# ---------------------------------------------------------------------------
# Lightweight local embedding using a pre-downloaded sentence-transformers
# model.  Falls back to a zero-vector stub only if the package is missing.
# ---------------------------------------------------------------------------
_embed_model = None

def _get_embed_model():
    global _embed_model
    if _embed_model is not None:
        return _embed_model
    try:
        from sentence_transformers import SentenceTransformer
        _embed_model = SentenceTransformer("all-MiniLM-L6-v2")
        log.info("Loaded local sentence-transformers embedding model (all-MiniLM-L6-v2).")
    except Exception as e:
        log.warning(f"sentence_transformers unavailable ({e}). Using stub embeddings.")
        _embed_model = None
    return _embed_model


async def _local_embedding_func(texts: list[str]) -> "np.ndarray":
    import numpy as np
    model = _get_embed_model()
    if model is None:
        # Stub: zero vectors — storage will work but search quality is zero
        return np.zeros((len(texts), 384), dtype=np.float32)
    embeddings = model.encode(texts, normalize_embeddings=True)
    return np.array(embeddings, dtype=np.float32)


# ---------------------------------------------------------------------------
# LLM wrappers — one per supported provider.
# The critical part: LightRAG calls llm_model_func with keyword_extraction=True
# when it wants structured JSON for entity/relation extraction.
# We detect this via `response_format` in kwargs and route accordingly,
# keeping everything within the same provider (no silent cross-provider calls).
# ---------------------------------------------------------------------------

def _make_anthropic_llm_func(model: str, api_key: str):
    """Return an async func compatible with LightRAG's llm_model_func contract."""
    from lightrag.llm.anthropic import anthropic_complete_if_cache

    async def llm_model_func(prompt, system_prompt=None, history_messages=None, **kwargs):
        if history_messages is None:
            history_messages = []

        # Strip LightRAG-internal kwargs that LLM APIs don't understand
        kwargs.pop("response_format", None)
        kwargs.pop("keyword_extraction", None)
        kwargs.setdefault("max_tokens", 8192)

        return await anthropic_complete_if_cache(
            model, prompt,
            system_prompt=system_prompt,
            history_messages=history_messages,
            api_key=api_key,
            **kwargs
        )
    return llm_model_func


def _make_openai_llm_func(model: str, api_key: str):
    from lightrag.llm.openai import openai_complete_if_cache

    async def llm_model_func(prompt, system_prompt=None, history_messages=None, **kwargs):
        if history_messages is None:
            history_messages = []
        kwargs.pop("keyword_extraction", None)
        kwargs.setdefault("max_tokens", 8192)
        return await openai_complete_if_cache(
            model, prompt,
            system_prompt=system_prompt,
            history_messages=history_messages,
            api_key=api_key,
            **kwargs
        )
    return llm_model_func


def _make_google_llm_func(model: str, api_key: str):
    from lightrag.llm.google import gemini_complete_if_cache

    async def llm_model_func(prompt, system_prompt=None, history_messages=None, **kwargs):
        if history_messages is None:
            history_messages = []
        kwargs.pop("response_format", None)
        kwargs.pop("keyword_extraction", None)
        kwargs.setdefault("max_tokens", 8192)
        return await gemini_complete_if_cache(
            model, prompt,
            system_prompt=system_prompt,
            history_messages=history_messages,
            api_key=api_key,
            **kwargs
        )
    return llm_model_func


def _make_ollama_llm_func(model: str, api_key: str):
    from lightrag.llm.ollama import ollama_model_if_cache

    async def llm_model_func(prompt, system_prompt=None, history_messages=None, **kwargs):
        if history_messages is None:
            history_messages = []
        kwargs.pop("response_format", None)
        kwargs.pop("keyword_extraction", None)
        kwargs.setdefault("max_tokens", 8192)
        return await ollama_model_if_cache(
            model, prompt,
            system_prompt=system_prompt,
            history_messages=history_messages,
            **kwargs
        )
    return llm_model_func


# ---------------------------------------------------------------------------

def get_lightrag(provider: str, api_key: str, model: str):
    global _lightrag_instance, _current_provider, _current_model
    from lightrag import LightRAG
    from lightrag.utils import EmbeddingFunc

    # Reuse existing instance when nothing has changed
    if (_lightrag_instance is not None
            and provider == _current_provider
            and model == _current_model):
        return _lightrag_instance

    wd = _get_working_dir()
    log.info(f"Initializing LightRAG in {wd} | provider={provider}, model={model}")

    prov_lower = provider.lower()

    # Set env-vars expected by LightRAG internals
    if prov_lower == "openai":
        os.environ["OPENAI_API_KEY"] = api_key
        llm_func = _make_openai_llm_func(model, api_key)
    elif prov_lower == "anthropic":
        os.environ["ANTHROPIC_API_KEY"] = api_key
        llm_func = _make_anthropic_llm_func(model, api_key)
    elif prov_lower == "google":
        os.environ["GEMINI_API_KEY"] = api_key
        llm_func = _make_google_llm_func(model, api_key)
    elif prov_lower == "ollama":
        llm_func = _make_ollama_llm_func(model, api_key)
    else:
        os.environ["OPENAI_API_KEY"] = api_key
        llm_func = _make_openai_llm_func(model, api_key)

    embedding_func = EmbeddingFunc(
        embedding_dim=384,
        max_token_size=512,
        func=_local_embedding_func,
    )

    try:
        _lightrag_instance = LightRAG(
            working_dir=wd,
            llm_model_func=llm_func,
            llm_model_name=model,
            embedding_func=embedding_func,
        )
        run_async(_lightrag_instance.initialize_storages())
        _current_provider = provider
        _current_model = model
        log.info("LightRAG initialized successfully.")
    except Exception as e:
        log.error(f"Failed to initialize LightRAG: {e}")
        _lightrag_instance = None

    return _lightrag_instance


# ---------------------------------------------------------------------------
# Background asyncio loop — isolated from Unreal's main thread
# ---------------------------------------------------------------------------
_bg_loop = None
_bg_thread = None


def _start_bg_loop():
    global _bg_loop

    async def run_forever():
        global _bg_loop
        _bg_loop = asyncio.get_running_loop()
        # Ensure AnyIO/sniffio recognises this loop
        try:
            import sniffio
            sniffio.current_async_library_cvar.set("asyncio")
        except Exception:
            pass
        while True:
            await asyncio.sleep(3600)

    # Windows: force SelectorEventLoop to avoid ProactorEventLoop IOCP bugs
    # that manifest as "cannot create weak reference to NoneType" under httpx
    if sys.platform == "win32":
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

    asyncio.run(run_forever())


def _ensure_bg_loop():
    global _bg_loop, _bg_thread
    if _bg_loop is None or not _bg_loop.is_running():
        _bg_thread = threading.Thread(target=_start_bg_loop, daemon=True, name="LightRAG-BG")
        _bg_thread.start()
        while _bg_loop is None or not _bg_loop.is_running():
            time.sleep(0.01)


def run_async(coro):
    """Run a coroutine inside the dedicated background event loop."""
    _ensure_bg_loop()

    async def _wrap():
        # Re-inject sniffio context for every task boundary
        try:
            import sniffio
            sniffio.current_async_library_cvar.set("asyncio")
        except Exception:
            pass
        return await coro

    future = asyncio.run_coroutine_threadsafe(_wrap(), _bg_loop)
    return future.result()


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def insert_knowledge(provider: str, api_key: str, model: str, text: str) -> str:
    """Insert a fact into the persistent knowledge graph."""
    rag = get_lightrag(provider, api_key, model)
    if not rag:
        return "Error: Could not initialize RAG engine."
    try:
        run_async(rag.ainsert(text))
        return "Knowledge successfully encoded into Semantic Graph."
    except Exception as e:
        log.error(f"LightRAG insert error: {e}")
        return f"Warning: Failed to encode knowledge. {e}"


def query_knowledge(provider: str, api_key: str, model: str, question: str, mode: str = "hybrid") -> str:
    """Query the persistent knowledge graph."""
    rag = get_lightrag(provider, api_key, model)
    if not rag:
        return "Error: Could not initialize RAG engine."
    try:
        from lightrag import QueryParam
        return run_async(rag.aquery(question, param=QueryParam(mode=mode)))
    except Exception as e:
        log.error(f"LightRAG query error: {e}")
        return f"Warning: Failed to query knowledge. {e}"
