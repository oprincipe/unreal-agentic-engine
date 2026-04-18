import socket
import json
import time

HOST = '127.0.0.1'
PORT = 55557

blueprints = [
    'Battery', 'Camera', 'CameraBattery', 'Cameroid', 'CameroidPaperPack',
    'CameroidPhotoDeveloped', 'CameroidPhotoStatic', 'CustomInspectable',
    'Diary', 'Flashlight', 'Glowstick', 'Key', 'Keycard', 'Lighter',
    'LighterFluid', 'Painkiller', 'Paper', 'Pistol', 'PistolBullets',
    'Torch', 'TorchOil', 'VHS'
]

results = {}
for bp in blueprints:
    payload = {
        'type': 'read_blueprint_functions',
        'params': {
            'blueprint_path': f'/Game/HorrorEngine/Blueprints/Assets/Inventory/{bp}'
        }
    }
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((HOST, PORT))
        s.sendall(json.dumps(payload).encode('utf-8'))
        data = s.recv(65536)
        response = json.loads(data.decode('utf-8'))
        if response.get('status') == 'success':
            funcs = response['result'].get('functions', [])
            results[bp] = [f['name'] for f in funcs]
        else:
            results[bp] = 'ERROR: ' + response.get('error', 'unknown')
        s.close()
        time.sleep(0.1)
    except Exception as e:
        results[bp] = 'EXCEPTION: ' + str(e)

print(json.dumps(results, indent=2))
