import os
import logging
import threading
import asyncio
import time
import nest_asyncio

# --- MONKEYPATCH LIGHTRAG WORKER POOL ---
# LightRAG 1.4.15 introduced a PriorityQueue WorkerPool that heavily relies on asyncio.wait_for
# which causes "Timeout should be used inside a task" bugs on Python 3.14 Windows.
# We monkeypatch the decorator here to bypass the worker pool and just execute the LLM/Embedding calls inline.
import lightrag.utils
def _bypass_priority_limit_async_func_call(*args, **kwargs):
    def decorator(func):
        async def wrapper(*func_args, **func_kwargs):
            return await func(*func_args, **func_kwargs)
        return wrapper
    return decorator
lightrag.utils.priority_limit_async_func_call = _bypass_priority_limit_async_func_call
# ----------------------------------------

# Apply nest_asyncio to allow asyncio.run() within threaded environments if needed
nest_asyncio.apply()

log = logging.getLogger(__name__)

# Global instances
_lightrag_instance = None
_current_provider = None
_current_model = None

def _get_working_dir() -> str:
    from unreal_mcp_agent import _db_path
    if _db_path:
        base = os.path.dirname(_db_path)
    else:
        base = os.path.join(os.path.dirname(__file__), "..", "Saved", "UnrealMCP")
    wd = os.path.join(base, "GraphStorage")
    os.makedirs(wd, exist_ok=True)
    return wd

def get_lightrag(provider: str, api_key: str, model: str):
    global _lightrag_instance, _current_provider, _current_model
    from lightrag import LightRAG
    
    # Check if we can reuse the existing instance
    if _lightrag_instance and provider == _current_provider and model == _current_model:
        if provider.lower() == "openai":
            os.environ["OPENAI_API_KEY"] = api_key
        elif provider.lower() == "anthropic":
            os.environ["ANTHROPIC_API_KEY"] = api_key
        elif provider.lower() == "google":
            os.environ["GEMINI_API_KEY"] = api_key
        return _lightrag_instance

    wd = _get_working_dir()
    log.info(f"Initializing LightRAG v1.4+ in {wd} for provider={provider}, model={model}")

    prov_lower = provider.lower()
    llm_model_name = model
    
    if prov_lower == "openai":
        os.environ["OPENAI_API_KEY"] = api_key
    elif prov_lower == "anthropic":
        os.environ["ANTHROPIC_API_KEY"] = api_key
    elif prov_lower == "google":
        os.environ["GEMINI_API_KEY"] = api_key
        if not llm_model_name.startswith("gemini"):
            llm_model_name = f"gemini/{model}"

    from lightrag.utils import wrap_embedding_func_with_attrs
    
    if prov_lower == "openai":
        from lightrag.llm.openai import openai_complete_if_cache, openai_embed
        llm_model_func = openai_complete_if_cache
        embedding_func = openai_embed

    elif prov_lower == "anthropic":
        from lightrag.llm.anthropic import anthropic_complete_if_cache
        from lightrag.llm.openai import openai_embed
        llm_model_func = anthropic_complete_if_cache
        embedding_func = openai_embed
        log.info("Anthropic provider selected: Using OpenAI for embeddings. Ensure OPENAI_API_KEY is set.")

    elif prov_lower == "google":
        from lightrag.llm.google import gemini_complete_if_cache, gemini_embed
        llm_model_func = gemini_complete_if_cache
        embedding_func = gemini_embed

    elif prov_lower == "ollama":
        from lightrag.llm.ollama import ollama_model_if_cache, ollama_embed
        llm_model_func = ollama_model_if_cache
        embedding_func = ollama_embed
        
    else:
        from lightrag.llm.openai import openai_complete_if_cache, openai_embed
        llm_model_func = openai_complete_if_cache
        embedding_func = openai_embed

    try:
        _lightrag_instance = LightRAG(
            working_dir=wd,
            llm_model_func=llm_model_func,
            llm_model_name=model,
            embedding_func=embedding_func,
            llm_model_kwargs={"api_key": api_key}
        )
        # Initialize storages in the background loop safely
        run_async(_lightrag_instance.initialize_storages())
        
        _current_provider = provider
        _current_model = model
    except Exception as e:
        log.error(f"Failed to initialize LightRAG: {e}")
        _lightrag_instance = None
        
    return _lightrag_instance

_bg_loop = None
_bg_thread = None

def _start_bg_loop():
    global _bg_loop
    async def run_forever():
        global _bg_loop
        _bg_loop = asyncio.get_running_loop()
        # Keep the loop alive forever
        while True:
            await asyncio.sleep(3600)
    
    # asyncio.run automatically initializes 3.12+ contextvars required for asyncio.timeout
    asyncio.run(run_forever())

def _ensure_bg_loop():
    global _bg_loop, _bg_thread
    if _bg_loop is None or not getattr(_bg_loop, 'is_running', lambda: False)():
        _bg_thread = threading.Thread(target=_start_bg_loop, daemon=True)
        _bg_thread.start()
        while _bg_loop is None or not _bg_loop.is_running():
            time.sleep(0.01)

def run_async(coro):
    """Executes a coroutine safely inside the background event loop."""
    _ensure_bg_loop()
    future = asyncio.run_coroutine_threadsafe(coro, _bg_loop)
    return future.result()


def insert_knowledge(provider: str, api_key: str, model: str, text: str) -> str:
    """Inserts a fact or graph text into LightRAG using the background event loop."""
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
    """Queries LightRAG semantic memory using the background event loop."""
    rag = get_lightrag(provider, api_key, model)
    if not rag:
        return "Error: Could not initialize RAG engine."
    
    try:
        from lightrag import QueryParam
        return run_async(rag.aquery(question, param=QueryParam(mode=mode)))
    except Exception as e:
        log.error(f"LightRAG query error: {e}")
        return f"Warning: Failed to query knowledge. {e}"

