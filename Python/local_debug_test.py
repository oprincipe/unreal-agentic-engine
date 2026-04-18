import socket
import json
import time

HOST = "127.0.0.1"
PORT = 55557

def send_command(command_type, params):
    try:
        print(f"\n--- Testing {command_type} ---")
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((HOST, PORT))
        
        payload = {
            "type": command_type,
            "params": params
        }
        
        s.sendall(json.dumps(payload).encode('utf-8'))
        
        # Read the size first or just read all
        data = s.recv(1024 * 1024)
        if not data:
            print("Failed: No data received")
            return None
            
        result = json.loads(data.decode('utf-8'))
        print(f"Result: {json.dumps(result, indent=2)}")
        return result
    except Exception as e:
        print(f"Error during {command_type}: {e}")
        return None

if __name__ == "__main__":
    # Test 1: Spawn an actor
    res1 = send_command("spawn_actor", {
        "class_path": "/Engine/BasicShapes/Cube",
        "location": {"x": 0.0, "y": 0.0, "z": 100.0}
    })
    
    if res1 and res1.get("status") == "success":
        actor_name = res1.get("result", {}).get("name", "")
        # Test 2: Modify its transform
        if actor_name:
            send_command("set_actor_transform", {
                "actor_name": actor_name,
                "location": {"x": 500.0, "y": 0.0, "z": 200.0},
                "rotation": {"pitch": 0.0, "yaw": 45.0, "roll": 0.0}
            })
