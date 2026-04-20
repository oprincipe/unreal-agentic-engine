"""
graph_memory.py  –  NetworkX-based Knowledge Graph memory for Unreal MCP Agent.

Architecture:
  INSERT: text → LLM extracts SPO triples → stored as nodes+edges in NetworkX DiGraph → persisted to .graphml
  QUERY:  question → LLM extracts keywords → BFS on matching nodes → serialize only relevant subgraph triples
          → LLM answers using the compact triple context (~20-80 tokens vs. hundreds for raw text)

This approach is token-efficient because the context sent to the LLM during queries
is structured triples, not full text blobs.
"""

from __future__ import annotations
import json
import logging
import os
import threading

import networkx as nx

log = logging.getLogger(__name__)

_graph: nx.DiGraph | None = None
_graph_lock = threading.Lock()


# ── Storage path ───────────────────────────────────────────────────────────────

def _get_graph_path() -> str:
    # Priority 1: UNREALMCP_DB_PATH env var set by the C++ launcher
    #   e.g. C:\Users\...\AI_Mcp_Test\Saved\UnrealAgenticEngine\chat_history.db
    #   → we want C:\Users\...\AI_Mcp_Test\Saved\UnrealAgenticEngine\
    env_db = os.environ.get("UNREALMCP_DB_PATH", "")
    if env_db:
        base = os.path.dirname(env_db)
        os.makedirs(base, exist_ok=True)
        return os.path.join(base, "knowledge_graph.graphml")

    # Priority 2: _db_path set at runtime by the HTTP server
    try:
        from unreal_agentic_client import _db_path
        if _db_path:
            base = os.path.dirname(_db_path)
            os.makedirs(base, exist_ok=True)
            return os.path.join(base, "knowledge_graph.graphml")
    except Exception:
        pass

    # Fallback: next to the script
    base = os.path.join(os.path.dirname(__file__), "..", "Saved", "UnrealAgenticEngine")
    os.makedirs(base, exist_ok=True)
    return os.path.join(base, "knowledge_graph.graphml")


# ── Graph load / save ──────────────────────────────────────────────────────────

def _load_graph() -> nx.DiGraph:
    global _graph
    if _graph is not None:
        return _graph
    with _graph_lock:
        if _graph is not None:
            return _graph
        path = _get_graph_path()
        if os.path.exists(path):
            try:
                _graph = nx.read_graphml(path)
                log.info(f"Loaded knowledge graph: {_graph.number_of_nodes()} nodes, {_graph.number_of_edges()} edges")
                return _graph
            except Exception as e:
                log.warning(f"Could not load graph ({e}), starting fresh.")
        _graph = nx.DiGraph()
        return _graph


def _save_graph():
    path = _get_graph_path()
    try:
        nx.write_graphml(_graph, path)
    except Exception as e:
        log.error(f"Could not save graph: {e}")


# ── LLM helpers ────────────────────────────────────────────────────────────────

def _call_llm(provider: str, api_key: str, model: str, prompt: str) -> str:
    """Make a raw LLM call. Returns the text response."""
    prov = provider.lower()
    try:
        if "anthropic" in prov or "claude" in prov:
            import anthropic
            client = anthropic.Anthropic(api_key=api_key)
            msg = client.messages.create(
                model=model,
                max_tokens=1024,
                messages=[{"role": "user", "content": prompt}]
            )
            return msg.content[0].text.strip()

        elif "openai" in prov or "gpt" in prov:
            import openai
            client = openai.OpenAI(api_key=api_key)
            resp = client.chat.completions.create(
                model=model,
                messages=[{"role": "user", "content": prompt}],
                max_tokens=1024,
            )
            return resp.choices[0].message.content.strip()

        elif "google" in prov or "gemini" in prov:
            import google.generativeai as genai
            genai.configure(api_key=api_key)
            m = genai.GenerativeModel(model)
            return m.generate_content(prompt).text.strip()

        else:
            raise ValueError(f"Unsupported provider: {provider}")
    except Exception as e:
        log.error(f"LLM call failed ({provider}): {e}")
        raise


def _extract_triples(provider: str, api_key: str, model: str, text: str) -> list[tuple[str, str, str]]:
    """Ask the LLM to extract Subject-Predicate-Object triples from text as JSON."""
    prompt = (
        "Extract all factual Subject-Predicate-Object triples from the text below.\n"
        "Return ONLY a JSON array of objects, each with keys 'subject', 'predicate', 'object'.\n"
        "Use short, lowercase, underscore-separated values (e.g. 'blood_arena', 'is_a', 'deathmatch_map').\n"
        "If the text contains no facts, return an empty array [].\n\n"
        f"Text: \"{text}\"\n\n"
        "JSON:"
    )
    raw = _call_llm(provider, api_key, model, prompt)
    # Strip markdown code fences if present
    raw = raw.strip()
    if raw.startswith("```"):
        raw = raw.split("```")[1]
        if raw.startswith("json"):
            raw = raw[4:]
    try:
        data = json.loads(raw)
        triples = [(d["subject"], d["predicate"], d["object"]) for d in data if "subject" in d]
        return triples
    except Exception as e:
        log.warning(f"Triple extraction parse error ({e}), raw: {raw[:200]}")
        return []


def _extract_keywords(provider: str, api_key: str, model: str, question: str) -> list[str]:
    """Ask the LLM to extract search keywords from a question."""
    prompt = (
        "Extract the key entity names and concepts from this question.\n"
        "Return ONLY a JSON array of lowercase strings (e.g. [\"blood_arena\", \"arena\", \"name\"]).\n\n"
        f"Question: \"{question}\"\n\nJSON:"
    )
    raw = _call_llm(provider, api_key, model, prompt)
    raw = raw.strip()
    if raw.startswith("```"):
        raw = raw.split("```")[1]
        if raw.startswith("json"):
            raw = raw[4:]
    try:
        return [k.lower() for k in json.loads(raw) if isinstance(k, str)]
    except Exception:
        # Fallback: split words from the question
        return [w.lower().strip("?.,!") for w in question.split() if len(w) > 3]


# ── Graph retrieval ────────────────────────────────────────────────────────────

def _find_relevant_triples(graph: nx.DiGraph, keywords: list[str], depth: int = 2) -> list[tuple[str, str, str]]:
    """
    Find nodes matching any keyword (substring match), then BFS up to `depth`
    hops to collect the connected subgraph as SPO triples.
    Returns a deduplicated list of (subject, predicate, object) strings.
    """
    seed_nodes = set()
    for node in graph.nodes():
        node_str = str(node).lower()
        if any(kw in node_str for kw in keywords):
            seed_nodes.add(node)

    visited_nodes = set()
    queue = list(seed_nodes)
    for _ in range(depth):
        next_queue = []
        for n in queue:
            if n in visited_nodes:
                continue
            visited_nodes.add(n)
            next_queue.extend(graph.successors(n))
            next_queue.extend(graph.predecessors(n))
        queue = next_queue
    visited_nodes.update(queue)

    triples = []
    seen = set()
    for u, v, data in graph.edges(data=True):
        if u in visited_nodes or v in visited_nodes:
            pred = data.get("predicate", "related_to")
            key = (u, pred, v)
            if key not in seen:
                seen.add(key)
                triples.append(key)
    return triples


def _triples_to_context(triples: list[tuple[str, str, str]]) -> str:
    """Serialize triples as compact bullet points for use in LLM context."""
    return "\n".join(f"  • ({s}) --[{p}]--> ({o})" for s, p, o in triples)


# ── Public API ─────────────────────────────────────────────────────────────────

def insert_knowledge(provider: str, api_key: str, model: str, text: str) -> str:
    """
    Extract SPO triples from `text` using the LLM, add them to the knowledge
    graph, and persist to disk.
    """
    graph = _load_graph()

    try:
        triples = _extract_triples(provider, api_key, model, text)
    except Exception as e:
        return f"Error extracting triples: {e}"

    if not triples:
        return "Warning: No facts could be extracted from the provided text."

    with _graph_lock:
        for subject, predicate, obj in triples:
            if subject not in graph:
                graph.add_node(subject)
            if obj not in graph:
                graph.add_node(obj)
            graph.add_edge(subject, obj, predicate=predicate)
        _save_graph()

    summary = "; ".join(f"({s})▶[{p}]▶({o})" for s, p, o in triples)
    log.info(f"Memory: inserted {len(triples)} triple(s): {summary}")
    return f"Remembered {len(triples)} fact(s): {summary}"


def query_knowledge(provider: str, api_key: str, model: str,
                    question: str, mode: str = "hybrid") -> str:
    """
    Query the knowledge graph. Retrieves relevant triples via keyword BFS,
    then synthesises an answer using the LLM with only the compact triple context.
    """
    graph = _load_graph()

    if graph.number_of_nodes() == 0:
        return "No information stored in memory yet."

    # Step 1: extract search keywords
    try:
        keywords = _extract_keywords(provider, api_key, model, question)
    except Exception:
        # Fallback to simple word splitting
        keywords = [w.lower().strip("?.,!") for w in question.split() if len(w) > 3]

    log.info(f"Memory query keywords: {keywords}")

    # Step 2: BFS on graph
    triples = _find_relevant_triples(graph, keywords, depth=2)

    if not triples:
        # Fallback: return all triples if graph is small enough
        all_triples = [(u, d.get("predicate", "related_to"), v)
                       for u, v, d in graph.edges(data=True)]
        if len(all_triples) <= 30:
            triples = all_triples
        else:
            return "No relevant information found in memory for this question."

    # Step 3: synthesise answer with compact triple context
    context = _triples_to_context(triples)
    prompt = (
        "You have access to the following knowledge graph facts (in Subject→Predicate→Object format):\n"
        f"{context}\n\n"
        "Using ONLY the facts above, answer this question concisely.\n"
        "If the answer cannot be derived from the facts, say so.\n\n"
        f"Question: {question}"
    )

    try:
        answer = _call_llm(provider, api_key, model, prompt)
        log.info(f"Memory query answered ({len(triples)} triples used, context ~{len(context)} chars)")
        return answer
    except Exception as e:
        # Still return raw triples as fallback
        return f"Graph facts (LLM synthesis failed: {e}):\n{context}"
