import time
import socket
import json
import sys

UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557

def send_command(command: str, params: dict):
    payload = {"command": command, "type": command, "params": params}
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(10.0)
        s.connect((UNREAL_HOST, UNREAL_PORT))
        
        request_data = json.dumps(payload) + "\n"
        s.sendall(request_data.encode('utf-8'))
        
        response = s.recv(8192)
        if response:
            return json.loads(response.decode('utf-8'))
        return None

def test_simulation():
    print("--- Test 3: Run Deterministic Simulation (Phase 2) ---")
    
    asset_path = "/Game/AI/Tests/BT_AgentTest"
    max_time = 5.0
    
    print(f"Starting simulation on {asset_path} for {max_time} seconds...")
    start_resp = send_command("start_deterministic_simulation", {"asset_path": asset_path, "max_time_limit": max_time})
    
    if not start_resp or not start_resp.get("status") == "success":
        print(f"FAIL: Could not start simulation. Response: {start_resp}")
        return
        
    print("Simulation started. Polling for results...")
    
    # Poll loop
    completed = False
    for i in range(15):
        time.sleep(1.0)
        poll_resp = send_command("get_simulation_result", {})
        
        if not poll_resp or not poll_resp.get("status") == "success":
            print(f"FAIL: Polling failed or returned error: {poll_resp}")
            return
            
        result = poll_resp.get("result", {})
        status = result.get("status")
        
        if status == "completed":
            print("\n✅ Simulation Completed!")
            metrics = result.get("metrics", {})
            print(f"Metrics Received:")
            print(f" - Time Simulated:   {metrics.get('time_simulated', 0):.2f}s")
            print(f" - Distance Traveled: {metrics.get('distance_traveled', 0):.2f} units")
            print(f" - Time Stuck:       {metrics.get('time_stuck', 0):.2f}s")
            
            if metrics.get('time_simulated', 0) > 4.5:
                print("\n✅ PASS: Simulation ran for the requested duration.")
            else:
                print("\n❌ FAIL: Simulation ended too early.")
                
            completed = True
            break
        elif status == "running":
            print(f"   ... still running (tick {i})")
        else:
            print(f"Unknown status: {status}")
            return
            
    if not completed:
        print("❌ FAIL: Simulation timed out!")

if __name__ == "__main__":
    test_simulation()
