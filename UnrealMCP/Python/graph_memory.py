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
    from lightrag.llm import litellm_model, litellm_embedding
    
    # Check if we can reuse the existing instance
    if _lightrag_instance and provider == _current_provider and model == _current_model:
        # LiteLLM handles API keys via environment variables primarily, but we can set them
        if provider.lower() == "openai":
            os.environ["OPENAI_API_KEY"] = api_key
        elif provider.lower() == "anthropic":
            os.environ["ANTHROPIC_API_KEY"] = api_key
        elif provider.lower() == "google":
            os.environ["GEMINI_API_KEY"] = api_key
        return _lightrag_instance

    wd = _get_working_dir()
    log.info(f"Initializing LightRAG in {wd} for provider={provider}, model={model}")

    # Set up LiteLLM API keys based on provider
    prov_lower = provider.lower()
    llm_model_name = model
    
    if prov_lower == "openai":
        os.environ["OPENAI_API_KEY"] = api_key
        # Ensure litellm uses standard openai prefix if needed
    elif prov_lower == "anthropic":
        os.environ["ANTHROPIC_API_KEY"] = api_key
        # Anthropic models need 'anthropic/' prefix for litellm sometimes, or just the model name
    elif prov_lower == "google":
        os.environ["GEMINI_API_KEY"] = api_key
        if not llm_model_name.startswith("gemini"):
            llm_model_name = f"gemini/{model}"

    # We use LiteLLM kwargs to pass dynamic model configuration
    async def llm_model_func(prompt, **kwargs):
        # Merge kwargs defaults with specific model requirement
        merged_kwargs = {**kwargs}
        if prov_lower == "ollama":
            # For ollama, litellm requires the model name as 'ollama/...' and a custom api_base
            return await litellm_model(
                prompt,
                model=f"ollama/{model}",
                api_base=api_key if api_key.startswith("http") else "http://localhost:11434",
                **merged_kwargs
            )
        else:
            return await litellm_model(prompt, model=llm_model_name, **merged_kwargs)

    async def embedding_func(texts, **kwargs):
        # LiteLLM embedding default
        if prov_lower == "openai":
            emb_model = "text-embedding-3-small"
        elif prov_lower == "ollama":
            emb_model = "ollama/nomic-embed-text"
            kwargs["api_base"] = api_key if api_key.startswith("http") else "http://localhost:11434"
        elif prov_lower == "google":
            emb_model = "text-embedding-004"
        else:
            # Fallback or generic 
            emb_model = "text-embedding-3-small"
            # In Anthropic case we might have to use OpenAI for embeddings or another provider since Anthropic doesn't have an embedding API suitable out of the box in litellm usually, 
            # For simplicity, if working with Anthropic without OpenAI key, this might fail unless configured. We'll try passing dummy.
            pass
            
        return await litellm_embedding(texts, model=emb_model, **kwargs)

    # Note: LightRAG requires an embedding model. If the user is using Anthropic and has no OpenAI key,
    # embedding might throw an error if we default to OpenAI.
    # For a robust offline/local plugin, using Ollama embeddings is great, but users might not have it.
    
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

