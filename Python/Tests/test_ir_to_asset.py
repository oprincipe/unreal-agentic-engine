import json
import socket
import sys
import os

# Import the schema validator to ensure we are sending compliant JSON
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from unreal_agentic_ir_schema import validate_ir_json

UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557

def test_ir_generation():
    print("--- Test 1: Generate Valid IR JSON ---")
    
    # 1. Define the IR according to the Pydantic Schema rules
    ir_payload = {
        "ir_version": "1.0.0",
        "asset_path": "/Game/AI/Tests/BT_AgentTest",
        "blackboard_path": "/Game/AI/Tests/BB_AgentTest",
        "blackboard_keys": [
            {"name": "TargetActor", "type": "Object", "base_class": "/Script/Engine.Actor"},
            {"name": "Destination", "type": "Vector"},
            {"name": "WaitTime", "type": "Float"}
        ],
        "tree_root": {
            "type": "Root",
            "blackboard_asset": "/Game/AI/Tests/BB_AgentTest",
            "child": {
                "type": "Sequence",
                "children": [
                    {
                        "type": "Task",
                        "task_class": "UBTTask_Wait", # Native task
                        "properties": {}
                    },
                    {
                        "type": "Task",
                        "task_class": "UBTTask_MoveTo", # Native task
                        "properties": {}
                    }
                ]
            }
        }
    }

    # 2. Validate via our new Pydantic schema
    try:
        validated_data = validate_ir_json(ir_payload)
        print("PASS: IR Validation Passed. Schema constraints respected.")
    except Exception as e:
        print(f"FAIL: IR Validation Failed! Error: {e}")
        return

    print("\n--- Test 2: Send IR Payload to Unreal Engine TCP Server ---")
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(5)
            s.connect((UNREAL_HOST, UNREAL_PORT))
            print("Connected to Unreal Engine.")

            # 3. Construct tcp payload required by UnrealAgenticEngine
            tcp_payload = {
                "command": "render_ai_ir_to_asset",
                "type": "render_ai_ir_to_asset",
                "params": {
                    "ir_data": ir_payload
                }
            }

            request_data = json.dumps(tcp_payload) + "\n"
            s.sendall(request_data.encode('utf-8'))
            print("Payload sent. Waiting for response...")

            # 4. Await response
            response = s.recv(8192)
            if response:
                decoded = response.decode('utf-8')
                print(f"Raw Response: {decoded}")
                resp_json = json.loads(decoded)
                if resp_json.get("status") == "success":
                    print("PASS: Unreal Engine successfully atomicaly processed the IR and created the assets!")
                else:
                    print(f"FAIL: Unreal Engine returned an error: {resp_json.get('error') or resp_json}")
            else:
                print("FAIL: No response received.")

    except ConnectionRefusedError:
        print("FAIL: Cannot connect to Unreal Engine. Is the project open and the MCP Server plugin running?")
    except Exception as e:
        print(f"FAIL: Unexpected error during TCP transmit: {e}")

if __name__ == "__main__":
    test_ir_generation()
