# Scene Graph & World Manipulation Guide

Unreal MCP Agent allows Large Language Models to not only view your Engine world logically but also physically interact with it via dynamic actor spawning and transformations.

## Interacting with the Map

### 1. Surveying the Scene
By calling `get_actors_in_level`, the agent uses a heavy-duty `TActorIterator` to map out exactly what is currently loaded in the Editor. It returns:
- Actor names/labels
- Internal class type
- 3D Transform Object (Location, Rotation, Scale)

### 2. Spawning and Moving Objects
You can instruct the AI to construct levels or populate environments.
- **`spawn_actor`**: Provide a `class_path` (System Class or custom Blueprint) and initial coordinates. The server will instantiate it into the World.
- **`set_actor_transform`**: Modify existing objects by name. The tool accepts optional parameters for Location, Rotation, and Scale. If you just pass `loc_z=500.0`, it will lift the object while maintaining its original X/Y and rotation.

### 3. Cleanup
- **`destroy_actor`**: Target an actor by its current Editor Name and forcefully destroy it.

## Example Automation Flow
**User**: *"I need a line of three crates starting from [0,0,0], spaced 100 units apart on the X axis, and delete the old DefaultCube."*

**AI Execution**: 
1. `destroy_actor(actor_name="DefaultCube_1")`
2. `spawn_actor(class_path="/Game/BP_Crate", location_x=0)`
3. `spawn_actor(class_path="/Game/BP_Crate", location_x=100)`
4. `spawn_actor(class_path="/Game/BP_Crate", location_x=200)`
