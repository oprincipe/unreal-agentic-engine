#!/usr/bin/env python3
"""
Unreal AI MCP Agent Loop
========================
Provides an HTTP API that the Unreal Engine plugin calls when the user sends
a chat message from the native Editor chat window.

Architecture:
  Unreal C++ Widget
       │  HTTP POST /chat  (json: {message, provider, api_key, model, session_id})
       ▼
  This server (in-process thread on port 55558)
       │  calls LLM with Tool Use
       ▼
  LLM Provider (Anthropic / OpenAI / Google / Ollama)
       │  returns tool_use blocks
       ▼
  This server executes MCP tools against Unreal TCP Bridge (port 55557)
       │  returns final text reply
       ▼
  Unreal C++ Widget  (response body {reply: "..."})

Persistence:
  Chat history is stored in a SQLite db at:
  <UE Project>/Saved/UnrealMCP/chat_history.db
  The db path is passed as a query param or env var.
"""

import os
import sys
import json
import socket
import sqlite3
import logging
import importlib.util
import subprocess
import threading
import tempfile
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Optional, List, Dict, Any

# ────────────────────────────────────────
# Logging & Stdout redirection (Prevents WinError 6 on hidden launch)
# ────────────────────────────────────────
LOG_FILE = os.path.join(tempfile.gettempdir(), "unreal_mcp_agent.log")

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.StreamHandler(sys.stdout) if sys.stdout else logging.NullHandler(),
        logging.FileHandler(LOG_FILE, mode='w', encoding='utf-8')
    ],
)
log = logging.getLogger("UnrealMCPAgent")

class LoggerWriter:
    def __init__(self, level):
        self.level = level
    def write(self, message):
        if message != '\n':
            self.level(message.rstrip())
    def flush(self):
        pass

# Redirect missing or broken sys.stdout/stderr 
sys.stdout = LoggerWriter(log.info)
sys.stderr = LoggerWriter(log.error)

AGENT_PORT   = 55558
UNREAL_HOST  = "127.0.0.1"
UNREAL_PORT  = 55557

# ────────────────────────────────────────
# Auto-install dependencies
# ────────────────────────────────────────
REQUIREMENTS_FILE = os.path.join(os.path.dirname(__file__), "requirements.txt")

def ensure_dependencies():
    """Install missing packages from requirements.txt automatically."""
    if not os.path.exists(REQUIREMENTS_FILE):
        log.warning("requirements.txt not found, skipping auto-install.")
        return

    log.info("Checking Python dependencies...")
    try:
        subprocess.check_call(
            [sys.executable, "-m", "pip", "install", "-r", REQUIREMENTS_FILE, "--quiet"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        log.info("Dependencies OK.")
    except subprocess.CalledProcessError as e:
        log.error(f"pip install failed: {e}. Some providers may not work.")


# ────────────────────────────────────────
# SQLite Persistence
# ────────────────────────────────────────
def get_db_path() -> str:
    env_path = os.environ.get("UNREALMCP_DB_PATH", "")
    if env_path:
        return env_path
    saved_dir = os.path.join(os.path.dirname(__file__), "..", "Saved", "UnrealMCP")
    os.makedirs(saved_dir, exist_ok=True)
    return os.path.join(saved_dir, "chat_history.db")


def init_db(db_path: str):
    conn = sqlite3.connect(db_path)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS messages (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id TEXT    NOT NULL,
            role       TEXT    NOT NULL,
            content    TEXT    NOT NULL,
            ts         TEXT    NOT NULL
        )
    """)
    conn.commit()
    conn.close()


from datetime import datetime, timezone

def save_message(db_path: str, session_id: str, role: str, content: str):
    conn = sqlite3.connect(db_path)
    conn.execute(
        "INSERT INTO messages (session_id, role, content, ts) VALUES (?, ?, ?, ?)",
        (session_id, role, content, datetime.now(timezone.utc).isoformat()),
    )
    conn.commit()
    conn.close()


def load_session(db_path: str, session_id: str, limit: int = 40) -> List[Dict]:
    conn = sqlite3.connect(db_path)
    rows = conn.execute(
        "SELECT role, content FROM messages WHERE session_id=? ORDER BY id DESC LIMIT ?",
        (session_id, limit),
    ).fetchall()
    conn.close()
    return [{"role": r, "content": c} for r, c in reversed(rows)]


# ────────────────────────────────────────
# Unreal TCP Bridge helper
# ────────────────────────────────────────
def _recv_full(sock: socket.socket) -> bytes:
    chunks = []
    sock.settimeout(300.0)  # wait up to 5 minutes for complex commands
    while True:
        chunk = sock.recv(1024 * 1024)  # 1MB chunks for speed
        if not chunk:
            break
        chunks.append(chunk)
        data = b"".join(chunks)
        try:
            decoded = data.decode("utf-8")
            json.loads(decoded)
            return data
        except (json.JSONDecodeError, UnicodeDecodeError, ValueError):
            continue
    return b"".join(chunks)


def call_unreal(command: str, params: Dict) -> Dict:
    """Send a single command to the Unreal TCP bridge and return parsed JSON."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(300.0)
        s.connect((UNREAL_HOST, UNREAL_PORT))
        payload = json.dumps({"type": command, "params": params}).encode("utf-8")
        s.sendall(payload)
        data = _recv_full(s)
        s.close()
        return json.loads(data.decode("utf-8"))
    except Exception as e:
        log.exception(f"Error calling Unreal TCP bridge for tool {command}")
        return {"status": "error", "error": str(e)}


# ────────────────────────────────────────
# Tool definitions shared across providers
# ────────────────────────────────────────
TOOLS = [
    {
        "name": "get_actors_in_level",
        "description": "List all actors currently placed in the Unreal level.",
        "input_schema": {
            "type": "object",
            "properties": {},
        },
    },
    {
        "name": "spawn_actor",
        "description": "Spawn an actor or static mesh into the Unreal level at the given location.",
        "input_schema": {
            "type": "object",
            "properties": {
                "class_path": {"type": "string", "description": "Blueprint or mesh path e.g. /Engine/BasicShapes/Cube"},
                "location": {
                    "type": "object",
                    "properties": {
                        "x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"},
                    },
                },
            },
            "required": ["class_path"],
        },
    },
    {
        "name": "set_actor_transform",
        "description": "Move/rotate/scale an actor already in the level.",
        "input_schema": {
            "type": "object",
            "properties": {
                "actor_name": {"type": "string"},
                "location": {"type": "object", "properties": {"x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number"}}},
                "rotation": {"type": "object", "properties": {"pitch": {"type": "number"}, "yaw": {"type": "number"}, "roll": {"type": "number"}}},
            },
            "required": ["actor_name"],
        },
    },
    {
        "name": "delete_actor",
        "description": "Delete or destroy an actor from the Unreal Engine level.",
        "input_schema": {
            "type": "object",
            "properties": {
                "name": {"type": "string", "description": "The exact name of the actor to delete (e.g. 'StaticMeshActor1')"}
            },
            "required": ["name"],
        },
    },
    {
        "name": "start_play_in_editor",
        "description": "Start a Play-In-Editor (PIE) session.",
        "input_schema": {"type": "object", "properties": {}},
    },
    {
        "name": "stop_play_in_editor",
        "description": "Stop the current Play-In-Editor (PIE) session.",
        "input_schema": {"type": "object", "properties": {}},
    },
    {
        "name": "get_editor_logs",
        "description": "Retrieve recent Unreal Engine log messages for debugging.",
        "input_schema": {
            "type": "object",
            "properties": {
                "clear_after_read": {"type": "boolean", "description": "Clear buffer after reading"},
            },
        },
    },
]


# ────────────────────────────────────────
# Provider Adapters
# ────────────────────────────────────────

def _run_tool(tool_name: str, tool_input: Dict) -> str:
    result = call_unreal(tool_name, tool_input)
    return json.dumps(result)


def agent_anthropic(messages: List[Dict], api_key: str, model: str) -> str:
    import anthropic
    client = anthropic.Anthropic(api_key=api_key)
    
    ant_tools = [
        {"name": t["name"], "description": t["description"], "input_schema": t["input_schema"]}
        for t in TOOLS
    ]

    system = (
        "You are an AI Agent operating inside Unreal Engine 5. "
        "You have tools to inspect, modify and control the editor world. "
        "Always prefer using tools over guessing. Be concise and direct."
    )

    history = [{"role": m["role"], "content": m["content"]} for m in messages]

    while True:
        response = client.messages.create(
            model=model,
            max_tokens=4096,
            system=system,
            tools=ant_tools,
            messages=history,
        )

        # Add assistant message
        history.append({"role": "assistant", "content": response.content})

        if response.stop_reason == "end_turn":
            # Extract final text
            for block in response.content:
                if hasattr(block, "text"):
                    return block.text
            return ""

        if response.stop_reason == "tool_use":
            tool_results = []
            for block in response.content:
                if block.type == "tool_use":
                    log.info(f"Calling tool: {block.name} with {block.input}")
                    result_str = _run_tool(block.name, block.input)
                    tool_results.append({
                        "type": "tool_result",
                        "tool_use_id": block.id,
                        "content": result_str,
                    })

            history.append({"role": "user", "content": tool_results})
        else:
            break

    return "Agent loop ended without a response."


def agent_openai(messages: List[Dict], api_key: str, model: str) -> str:
    from openai import OpenAI
    client = OpenAI(api_key=api_key)

    oai_tools = [
        {
            "type": "function",
            "function": {
                "name": t["name"],
                "description": t["description"],
                "parameters": t["input_schema"],
            },
        }
        for t in TOOLS
    ]

    system_msg = {
        "role": "system",
        "content": (
            "You are an AI Agent operating inside Unreal Engine 5. "
            "Use your tools to inspect and modify the editor world. Be concise."
        ),
    }
    history = [system_msg] + [{"role": m["role"], "content": m["content"]} for m in messages]

    while True:
        response = client.chat.completions.create(
            model=model,
            messages=history,
            tools=oai_tools,
            tool_choice="auto",
        )
        msg = response.choices[0].message
        history.append(msg)

        if not msg.tool_calls:
            return msg.content or ""

        for tc in msg.tool_calls:
            fn_input = json.loads(tc.function.arguments)
            log.info(f"Calling tool: {tc.function.name} with {fn_input}")
            result_str = _run_tool(tc.function.name, fn_input)
            history.append({
                "role": "tool",
                "tool_call_id": tc.id,
                "content": result_str,
            })


def agent_google(messages: List[Dict], api_key: str, model: str) -> str:
    from google import genai
    from google.genai import types

    client = genai.Client(api_key=api_key)

    google_tools = [
        types.Tool(function_declarations=[
            types.FunctionDeclaration(
                name=t["name"],
                description=t["description"],
                parameters=t["input_schema"],
            )
            for t in TOOLS
        ])
    ]

    history = [
        types.Content(role=m["role"], parts=[types.Part(text=m["content"])])
        for m in messages
    ]

    while True:
        response = client.models.generate_content(
            model=model,
            contents=history,
            config=types.GenerateContentConfig(tools=google_tools),
        )
        candidate = response.candidates[0]
        history.append(candidate.content)

        calls = [p.function_call for p in candidate.content.parts if p.function_call]
        if not calls:
            texts = [p.text for p in candidate.content.parts if p.text]
            return "\n".join(texts)

        results = []
        for fc in calls:
            fn_input = dict(fc.args)
            log.info(f"Calling tool: {fc.name} with {fn_input}")
            result_str = _run_tool(fc.name, fn_input)
            results.append(types.Part(
                function_response=types.FunctionResponse(
                    name=fc.name,
                    response={"result": result_str},
                )
            ))
        history.append(types.Content(role="tool", parts=results))


def agent_ollama(messages: List[Dict], api_key: str, model: str) -> str:
    """Ollama runs locally. api_key can be empty or localhost URL override."""
    import ollama

    host = api_key if api_key.startswith("http") else "http://localhost:11434"
    client = ollama.Client(host=host)

    ollama_tools = [
        {
            "type": "function",
            "function": {
                "name": t["name"],
                "description": t["description"],
                "parameters": t["input_schema"],
            },
        }
        for t in TOOLS
    ]

    system_msg = {"role": "system", "content": "You are an AI Agent inside Unreal Engine 5. Use tools to inspect and modify the editor world."}
    history = [system_msg] + [{"role": m["role"], "content": m["content"]} for m in messages]

    while True:
        response = client.chat(model=model, messages=history, tools=ollama_tools)
        msg = response["message"]
        history.append(msg)

        calls = msg.get("tool_calls", [])
        if not calls:
            content = msg.get("content", "").strip()
            # Fallback for models that output raw JSON instead of using the API correctly
            
            # Clean up potential markdown code blocks
            clean_content = content
            if clean_content.startswith("```json"):
                clean_content = clean_content[7:]
            elif clean_content.startswith("```"):
                clean_content = clean_content[3:]
            if clean_content.endswith("```"):
                clean_content = clean_content[:-3]
            clean_content = clean_content.strip()

            if clean_content.startswith("{") and clean_content.endswith("}"):
                try:
                    parsed = json.loads(clean_content)
                    if "name" in parsed and ("arguments" in parsed or "parameters" in parsed):
                        fn_name = parsed["name"]
                        fn_args = parsed.get("arguments", parsed.get("parameters", {}))
                        if isinstance(fn_args, str):
                            try:
                                fn_args = json.loads(fn_args)
                            except:
                                fn_args = {}
                        log.info(f"Fallback parsing tool call from content: {fn_name} with {fn_args}")
                        result_str = _run_tool(fn_name, fn_args)
                        history.append({"role": "tool", "content": result_str})
                        continue
                except Exception:
                    pass
            return content

        for tc in calls:
            fn = tc["function"]
            log.info(f"Calling tool: {fn['name']} with {fn['arguments']}")
            result_str = _run_tool(fn["name"], fn["arguments"])
            history.append({"role": "tool", "content": result_str})


def agent_test_connection(provider: str, api_key: str, model: str) -> str:
    """Quick smoke-test: just list models if possible, or send a ping."""
    try:
        if provider == "Anthropic":
            import anthropic
            client = anthropic.Anthropic(api_key=api_key)
            resp = client.messages.create(model=model, max_tokens=5, messages=[{"role": "user", "content": "Hi"}])
            return f"✅ Anthropic connected. Model={model}"
        elif provider == "OpenAI":
            from openai import OpenAI
            client = OpenAI(api_key=api_key)
            client.models.retrieve(model)
            return f"✅ OpenAI connected. Model={model}"
        elif provider == "Google":
            from google import genai
            genai.Client(api_key=api_key)
            return f"✅ Google Gemini connected. Model={model}"
        elif provider == "Ollama":
            import ollama
            host = api_key if api_key.startswith("http") else "http://localhost:11434"
            client = ollama.Client(host=host)
            models = client.list()
            names = [m["name"] for m in models.get("models", [])]
            return f"✅ Ollama connected. Available models: {', '.join(names)}"
        else:
            return f"❌ Unknown provider: {provider}"
    except Exception as e:
        return f"❌ Connection failed: {e}"


def dispatch_agent(provider: str, messages: List[Dict], api_key: str, model: str) -> str:
    prov = provider.lower()
    if "anthropic" in prov:
        return agent_anthropic(messages, api_key, model)
    elif "openai" in prov:
        return agent_openai(messages, api_key, model)
    elif "google" in prov:
        return agent_google(messages, api_key, model)
    elif "ollama" in prov:
        return agent_ollama(messages, api_key, model)
    else:
        return f"Unknown provider: {provider}"


# ────────────────────────────────────────
# HTTP Server (Unreal C++ <-> Agent)
# ────────────────────────────────────────
_db_path: str = ""


class AgentHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        log.info(fmt % args)

    def send_json(self, code: int, data: Dict):
        body = json.dumps(data).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length)
        try:
            body = json.loads(raw)
        except Exception:
            self.send_json(400, {"error": "Invalid JSON"})
            return

        path = self.path.split("?")[0]

        if path == "/chat":
            session_id = body.get("session_id", "default")
            message    = body.get("message", "")
            provider   = body.get("provider", "Anthropic")
            api_key    = body.get("api_key", "")
            model      = body.get("model", "claude-3-5-sonnet-latest")

            if not message:
                self.send_json(400, {"error": "Missing 'message'"})
                return

            # Save user message
            save_message(_db_path, session_id, "user", message)

            # Load history (for context)
            history = load_session(_db_path, session_id, limit=40)

            try:
                reply = dispatch_agent(provider, history, api_key, model)
            except Exception as e:
                log.exception("Agent error")
                reply = f"Agent error: {e}"

            # Save assistant reply
            save_message(_db_path, session_id, "assistant", reply)
            self.send_json(200, {"reply": reply})

        elif path == "/test":
            provider = body.get("provider", "Anthropic")
            api_key  = body.get("api_key", "")
            model    = body.get("model", "claude-3-5-sonnet-latest")
            result   = agent_test_connection(provider, api_key, model)
            self.send_json(200, {"result": result})

        elif path == "/models/ollama":
            api_key = body.get("api_key", "")
            try:
                import ollama
                host = api_key if api_key.startswith("http") else "http://localhost:11434"
                client = ollama.Client(host=host)
                models = client.list()
                names = [m["name"] for m in models.get("models", [])]
                self.send_json(200, {"models": names})
            except Exception as e:
                self.send_json(500, {"error": str(e)})

        elif path == "/new_session":
            import uuid
            new_id = str(uuid.uuid4())
            self.send_json(200, {"session_id": new_id})

        elif path == "/history":
            session_id = body.get("session_id", "default")
            history = load_session(_db_path, session_id, limit=100)
            self.send_json(200, {"messages": history})

        else:
            self.send_json(404, {"error": "Not found"})


def run_http_server(db_path: str, port: int = AGENT_PORT):
    global _db_path
    _db_path = db_path
    init_db(db_path)
    server = ThreadingHTTPServer(("127.0.0.1", port), AgentHandler)
    log.info(f"Unreal AI Agent HTTP server listening on http://127.0.0.1:{port}")
    server.serve_forever()


# ────────────────────────────────────────
# Entry Point
# ────────────────────────────────────────
if __name__ == "__main__":
    ensure_dependencies()

    db = os.environ.get("UNREALMCP_DB_PATH") or get_db_path()
    port = int(os.environ.get("UNREALMCP_AGENT_PORT", AGENT_PORT))

    log.info("Starting Unreal AI MCP Agent...")
    log.info(f"  DB path   : {db}")
    log.info(f"  Agent port: {port}")
    log.info(f"  Unreal    : {UNREAL_HOST}:{UNREAL_PORT}")

    run_http_server(db, port)
