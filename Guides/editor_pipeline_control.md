# Editor PIE & Pipeline Control Guide

A true autonomous agent needs the ability to execute its logic, run tests, and diagnose failures without human intervention. The Unreal MCP Agent exposes the Unreal Editor's Play-In-Editor (PIE) states and local logs to the AI model.

## Game Testing
The Agent can physically kick off game sessions directly within the Unreal Engine Editor using:
- **`start_play_in_editor`**: Sends a `RequestPlaySession` command to `GEditor`.
- **`stop_play_in_editor`**: Ends the PIE session and returns to Editor mode.

## Log Interception & Debugging
When the game runs, the engine typically throws warnings, runtime exceptions, blueprint errors, or standard messages. The MCP C++ module secretly hooks into the Engine's `GLog` system, buffering up to the last 500 significant events. 

- **`get_editor_logs`**: Extracts this intercepted buffer so the AI can read it. It takes a `clear_after_read` boolean parameter designed to flush the buffer after polling.

## Closing the Loop
This combination is what truly allows **Self-Healing Code**:
1. AI manipulates a Blueprint or a variable.
2. AI calls `start_play_in_editor()`.
3. AI waits briefly and calls `get_editor_logs()`.
4. The logs return an `Accessed None` exception or a `Missing Reference` error.
5. AI calls `stop_play_in_editor()`.
6. AI fixes the blueprint and tries again.
