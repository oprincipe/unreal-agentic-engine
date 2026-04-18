# Blueprint Introspection Guide

Unreal MCP Agent provides powerful tools to dissect and analyze Unreal Engine Blueprints via natural language commands. By exposing internal UE C++ Reflection systems, AI agents can read the structure of your game without needing you to open the Editor and copy-paste values.

## Core Capabilities

### 1. Variables and Default Values
The agent can query any standard Blueprint or Data Asset to read its properties.
* **Tool**: `read_widget_variables`, `read_data_asset`, `read_blueprint_struct`
* **Use Case**: Check if a HUD has a text block named `TitleText`, or verify the maximum health value stored in a Data Asset.

### 2. Functions & Parameters
The API intercepts Blueprint function declarations.
* **Tool**: `read_blueprint_functions`
* **Use Case**: Give the AI the path to your Player Controller and ask: "What functions are available to move the character?" The AI will receive the function names, inputs, outputs, and pure/impure status.

### 3. Enums, Structs, and Macros
The agent can reconstruct complex data definitions.
* **Tool**: `read_blueprint_enum`, `read_blueprint_struct`, `read_blueprint_macros`
* **Use Case**: Let the AI automatically write Python/C++ equivalent models or understand State Machine enumeration states before modifying logic.

## Workflow Example 
**User**: *"Check my BP_Character. Does it have a Jump function, and what are the default variables?"*
**Agent**: 
1. `read_blueprint_functions(blueprint_path="/Game/BP_Character")`
2. `read_widget_variables(blueprint_path="/Game/BP_Character")`
3. The Agent responds naturally: *"Yes, there is a Jump() function, and the MaxSpeed variable is currently set to 600.0."*
