# Unreal MCP Agent - Getting Started

This guide explains how to leverage the Unreal MCP Agent to interact with Unreal Engine 5 via Claude or Cursor.

## What is this?
Unreal MCP Agent is an advanced Model Context Protocol (MCP) server that connects your AI directly into Unreal Engine. 
It replaces the traditional "typing" with "asking" – allowing the Agent to read your project's internals, configure assets, build scenes, and test the game.

## Available Modules

### 1. Blueprint Introspection
You can ask the AI to "read" your Blueprints. It uses specialized C++ commands to reflect and return raw JSON data about:
- **Variables & Default Values**
- **Functions & Parameters** 
- **Enums & Structs**
- **Widget Properties**

*Example:* `read_blueprint_functions(blueprint_path="/Game/MyBP")`

### 2. Scene Graph & World Manipulation
The AI can actively inspect the current loaded Pmap (Level).
- `get_actors_in_level` returns an array of all Actors with their transforms.
- `spawn_actor` spawns a class or Blueprint exactly where you want it.
- `set_actor_transform` modifies location, rotation, and scale dynamically.
- `destroy_actor` removes it from the level.

### 3. Editor Pipeline Flow
You can allow the AI to test the game for you:
- `start_play_in_editor` presses the "Play" button.
- `get_editor_logs(clear_after_read=True)` pulls recent Warnings and Errors from the Engine's memory so the AI can debug its own actions.
- `stop_play_in_editor` halts the simulation.

## How to use it in Cursor / Claude
1. Open the project where you dropped the `UnrealAgenticBridge` plugin.
2. Ensure the Plugin is enabled and your Editor is open.
3. Configure your `mcp.json` to point your MCP client to the `Python/unreal_agentic_server.py` server using `uv run`.

That's it! Now just open a prompt and type:
> "Spawn a crate in the level, start the game, and read the logs to see if it causes any warnings."
