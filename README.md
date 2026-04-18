# The Most Advanced MCP Server for Unreal Engine

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5%2B-orange.svg)](https://www.unrealengine.com/)
[![YouTube](https://img.shields.io/badge/YouTube-@flopperam-red.svg?logo=youtube)](https://youtube.com/@flopperam)
[![Discord](https://img.shields.io/badge/Discord-Join%20Server-5865F2.svg?logo=discord&logoColor=white)](https://discord.gg/3KNkke3rnH)
[![Twitter](https://img.shields.io/badge/X-@Flopperam-1DA1F2.svg?logo=x&logoColor=white)](https://twitter.com/Flopperam)
[![TikTok](https://img.shields.io/badge/TikTok-@flopperam-000000.svg?logo=tiktok&logoColor=white)](https://tiktok.com/@flopperam)

**Control Unreal Engine 5.5+ through AI with natural language. Build incredible 3D worlds and architectural masterpieces using MCP. Create entire towns, medieval castles, modern mansions, challenging mazes, and complex structures with AI-powered commands.**

> **Active Development**: This project is under very active development with consistent updates, new features, and improvements being added regularly. Join our [Discord](https://discord.gg/3KNkke3rnH) to stay updated on the latest releases!

## 🎬 See It In Action
Watch our comprehensive tutorial for complete setup and usage:
- **[Complete MCP Tutorial & Installation Guide](https://youtu.be/ct5dNJC-Hx4)** - Full walkthrough of installation, setup, and advanced usage

Check out these examples of the MCP server in action on our channel:
- **[GPT-5 vs Claude](https://youtube.com/shorts/xgoJ4d3d4-4)** - Watch Claude and GPT-5 go head-to-head building simultaneously - Claude creates a massive fortress while GPT-5 builds a sprawling city
- **[Advanced Metropolis Generation](https://youtube.com/shorts/6WkxCQXbCWk)** - Watch AI generate a full-blown metropolis with towers, streets, parks, and over 4,000 objects from a single prompt
- **[Advanced Maze & Mansion Generation](https://youtube.com/shorts/ArExYGpIZwI)** - Watch Claude generate a playable maze and complete mansion complex with wings, towers, and arches

## Featured Capabilities

### Blueprint Introspection & Analysis
```bash
# Read variables, structs, inputs, enums and functions from any Blueprint
> "Show me the variables inside the GameplaySettings Blueprint"
→ read_blueprint_functions(blueprint_path="/Game/Blueprints/BP_GameplaySettings")
```

### World Manipulation & Editor Control
```bash
# Spawn actors, control Play-in-Editor (PIE) and read engine logs
> "Place a crate at the center of the room and play the game"
→ spawn_actor(class_path="/Game/Blueprints/BP_Crate", location_x=0.0, location_y=0.0, location_z=0.0)
→ start_play_in_editor()
→ get_editor_logs() # Intercept live errors from the engine!
```

---

## Complete Tool Arsenal

| **Category** | **Tools** | **Description** |
|--------------|-----------|-----------------|
| **World Building** | `create_town`, `construct_house`, `create_tower`, `create_arch` | Build complex architectural structures and settlements |
| **Level Design** | `create_maze`, `create_obstacle_course`, `create_pyramid`, `create_wall` | Design challenging game levels and puzzles |
| **Physics & Materials** | `spawn_physics_blueprint_actor`, `set_physics_properties`, `create_material_instance` | Physics simulations and material assignments |
| **Asset Management** | `duplicate_asset`, `read_data_asset`, `read_input_action` | Manipulate and read existing project assets |
| **Blueprint Introspection** | `read_blueprint_struct`, `read_blueprint_functions`, `read_blueprint_enum` | Fully analyze internal blueprint logic and variables |
| **Actor & Scene Control** | `get_actors_in_level`, `spawn_actor`, `destroy_actor`, `set_actor_transform` | Precise control over scene objects and transforms |
| **Editor Pipeline** | `start_play_in_editor`, `stop_play_in_editor`, `get_editor_logs` | Run games & capture engine logs via AI |

---

## ⚡ Lightning-Fast Setup

### Prerequisites
- **Unreal Engine 5.5+** 
- **Python 3.12+**
- **MCP Client** (Claude Desktop, Cursor, or Windsurf)

### 1. Setup Options

**Option A: Use the Pre-Built Project (Recommended for Quick Start)**
```bash
# Clone the repository
git clone https://github.com/oprincipe/unreal-engine-mcp.git
cd unreal-engine-mcp

# Open the pre-configured project
# Double-click FlopperamUnrealMCP/FlopperamUnrealMCP.uproject
# or open it through Unreal Engine launcher
```

**Option B: Add Plugin to Your Existing Project**
```bash
# Copy the plugin to your project
cp -r UnrealMCP/ YourProject/Plugins/

# Enable in Unreal Editor
Edit → Plugins → Search "UnrealMCP" → Enable → Restart Editor
```

**Option C: Install for All Projects**
```bash
# Copy to Engine plugins folder
cp -r UnrealMCP/ "C:/Program Files/Epic Games/UE_5.5/Engine/Plugins/"
```

### 2. Launch the MCP Server

```bash
cd Python
# Basic generative tools
uv run unreal_mcp_server_advanced.py

# OR - Full Blueprint, Editor Control, and Introspection capabilities
uv run unreal_mcp_server_blueprints.py
```

### 3. Configure Your AI Client

Add this to your MCP configuration:

**Cursor**: `.cursor/mcp.json`
**Claude Desktop**: `~/.config/claude-desktop/mcp.json` 
**Windsurf**: `~/.config/windsurf/mcp.json`

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory", 
        "/path/to/unreal-engine-mcp/Python",
        "run", 
        "unreal_mcp_server_blueprints.py"
      ]
    }
  }
}
```

### Recommended AI Model

**We strongly recommend using Claude for the best experience.**

Claude has proven to be the most effective AI model for:
- Understanding complex 3D spatial relationships
- Analyzing Blueprint structures and identifying engine workflows
- Following architectural and physics constraints
- Creating coherent multi-step building processes

### Enhanced Accuracy with Rules

For improved results, especially when creating specific types of objects, provide the AI with our curated rules:

- **`.cursor/rules/`** - Contains specialized guides for different creation tasks

### 4. Start Building!

```bash
> "Create a medieval castle with towers and walls"
> "Generate a town square with fountain and buildings"
> "Make a challenging maze for players to solve"
```

---

## Architecture

```mermaid
graph TB
    A[AI Client<br/>Cursor/Claude/Windsurf] -->|MCP Protocol| B[Python Server<br/>unreal_mcp_server_advanced.py]
    B -->|TCP Socket| C[C++ Plugin<br/>UnrealMCP]
    C -->|Native API| D[Unreal Engine 5.5+<br/>Editor & Runtime]
    
    B --> E[Advanced Tools]
    E --> F[World Building]
    E --> G[Physics Simulation]  
    E --> H[Blueprint System]
    
    C --> I[Actor Management]
    C --> J[Component System]
    C --> K[Material System]
```

**Performance**: Native C++ plugin ensures minimal latency for real-time control
**Reliability**: Robust TCP communication with automatic reconnection
**Flexibility**: Full access to Unreal's actor, component, and Blueprint systems

---

## Community & Support

**Join our community and get help building amazing worlds!**

### Connect With Us
- **YouTube**: [youtube.com/@flopperam](https://youtube.com/@flopperam) - Tutorials, showcases, and development updates
- **Discord**: [discord.gg/8yr1RBv](https://discord.gg/3KNkke3rnH) - Get help, share creations, and discuss the plugin
- **Twitter/X**: [twitter.com/Flopperam](https://twitter.com/Flopperam) - Latest news and quick updates  
- **TikTok**: [tiktok.com/@flopperam](https://tiktok.com/@flopperam) - Quick tips and amazing builds

### Get Help & Share
- **Questions?** Ask in our Discord server for real-time support
- **Bug reports?** Open an issue on GitHub with reproduction steps
- **Feature ideas?** Join the discussion in our community channels

---

## License
MIT License - Build amazing things freely.
