import socket
import json
import time

HOST = '127.0.0.1'
PORT = 55557

def send(command_type, params):
    payload = {'type': command_type, 'params': params}
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(10.0)
        s.connect((HOST, PORT))
        s.sendall(json.dumps(payload).encode('utf-8'))
        data = s.recv(1024*1024)
        s.close()
        response = json.loads(data.decode('utf-8'))
        status = response.get('status', 'unknown')
        result = response.get('result', response.get('error', ''))
        return status, result
    except Exception as e:
        return 'exception', str(e)

print("--- Testing Scene Graph & Actor Manipulation ---")

# 1. Get Actors
status, res = send('get_actors_in_level', {})
if status == 'success':
    actors = res.get('actors', [])
    print(f"OK get_actors_in_level: Found {len(actors)} actors in the world.")
    if len(actors) > 0:
        first_a = actors[0]
        print(f"   Example: {first_a.get('name', 'Unknown')} | keys: {list(first_a.keys())}")
else:
    print(f"FAIL get_actors_in_level: {str(res)[:100]}")

# 2. Start PIE
status, res = send('start_play_in_editor', {})
if status == 'success':
    print("OK start_play_in_editor: PIE started successfully.")
else:
    print(f"FAIL start_play_in_editor: {str(res)[:100]}")

time.sleep(2) # Give it some time to start

# 3. Get Logs
status, res = send('get_editor_logs', {'clear_after_read': True})
if status == 'success':
    logs = res.get('logs', [])
    print(f"OK get_editor_logs: Retrieved {len(logs)} logs.")
    if len(logs) > 0:
        print(f"   Last log: {logs[-1][:100]}")
else:
    print(f"FAIL get_editor_logs: {str(res)[:100]}")

# 4. Stop PIE
status, res = send('stop_play_in_editor', {})
if status == 'success':
    print("OK stop_play_in_editor: PIE stopped successfully.")
else:
    print(f"FAIL stop_play_in_editor: {str(res)[:100]}")

# 5. Duplicate Asset
source = "/Game/HorrorEngine/Blueprints/HE_GameplaySettings"
dest = "/Game/HorrorEngine/Blueprints/HE_GameplaySettings_DTest"
status, res = send('duplicate_asset', {'source_path': source, 'dest_path': dest})
if status == 'success':
    print(f"OK duplicate_asset: Asset duplicated to {res.get('asset_path')}")
else:
    print(f"FAIL duplicate_asset: {str(res)[:100]}")

# 6. Create Material Instance
parent = "/Game/HorrorEngine/Materials/M_Master"
dest_mi = "/Game/HorrorEngine/Materials/MI_TestMaster"
status, res = send('create_material_instance', {'parent_path': parent, 'dest_path': dest_mi})
if status == 'success':
    print(f"OK create_material_instance: Created at {res.get('asset_path')}")
else:
    print(f"FAIL create_material_instance: {str(res)[:100]}")
