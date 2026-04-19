import os
import logging
import asyncio
import nest_asyncio

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

    async def llm_model_func(prompt, system_prompt=None, history_messages=[], **kwargs):
        from litellm import acompletion
        messages = []
        if system_prompt:
            messages.append({"role": "system", "content": system_prompt})
        if history_messages:
            messages.extend(history_messages)
        messages.append({"role": "user", "content": prompt})
        
        merged_kwargs = {**kwargs}
        if prov_lower == "ollama":
            resp = await acompletion(
                model=f"ollama/{model}",
                messages=messages,
                api_base=api_key if api_key.startswith("http") else "http://localhost:11434",
                **merged_kwargs
            )
        else:
            resp = await acompletion(
                model=llm_model_name,
                messages=messages,
                **merged_kwargs
            )
        return resp.choices[0].message.content

    async def embedding_func(texts, **kwargs):
        from litellm import aembedding
        if prov_lower == "openai":
            emb_model = "text-embedding-3-small"
        elif prov_lower == "ollama":
            emb_model = "ollama/nomic-embed-text"
            kwargs["api_base"] = api_key if api_key.startswith("http") else "http://localhost:11434"
        elif prov_lower == "google":
            emb_model = "text-embedding-004"
        else:
            emb_model = "text-embedding-3-small"
            
        try:
            resp = await aembedding(model=emb_model, input=texts, **kwargs)
            return [d["embedding"] for d in resp["data"]]
        except Exception as e:
            log.error(f"LiteLLM aembedding failed (ensure OPENAI_API_KEY is set if using Anthropic without native local embeddings): {e}")
            raise

    try:
        _lightrag_instance = LightRAG(
            working_dir=wd,
            llm_model_func=llm_model_func,
            embedding_func=embedding_func
        )
        _current_provider = provider
        _current_model = model
    except Exception as e:
        log.error(f"Failed to initialize LightRAG: {e}")
        _lightrag_instance = None
        
    return _lightrag_instance


def insert_knowledge(provider: str, api_key: str, model: str, text: str) -> str:
    """Inserts a fact or graph text into LightRAG."""
    rag = get_lightrag(provider, api_key, model)
    if not rag:
        return "Error: Could not initialize RAG engine."
    
    try:
        rag.insert(text)
        return "Knowledge successfully encoded into Semantic Graph."
    except Exception as e:
        log.error(f"LightRAG insert error: {e}")
        return f"Warning: Failed to encode knowledge. {e}"


def query_knowledge(provider: str, api_key: str, model: str, question: str, mode: str = "hybrid") -> str:
    """Queries LightRAG semantic memory."""
    rag = get_lightrag(provider, api_key, model)
    if not rag:
        return "Error: Could not initialize RAG engine."
    
    try:
        from lightrag.utils import QueryParam
        return rag.query(question, param=QueryParam(mode=mode))
    except Exception as e:
        log.error(f"LightRAG query error: {e}")
        return f"Warning: Failed to query knowledge. {e}"

