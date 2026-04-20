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
    time.sleep(1)
    
    # 1. Start PIE
    send_command("start_play_in_editor", {})

    # 2. Wait
    time.sleep(2)

    # 3. Read logs
    send_command("get_editor_logs", {"clear_after_read": True})

    # 4. Stop PIE
    send_command("stop_play_in_editor", {})
