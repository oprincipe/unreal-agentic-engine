import socket
import json
import time

HOST = '127.0.0.1'
PORT = 55557

def send(command_type, params):
    payload = {'type': command_type, 'params': params}
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((HOST, PORT))
        s.sendall(json.dumps(payload).encode('utf-8'))
        data = s.recv(65536)
        s.close()
        response = json.loads(data.decode('utf-8'))
        status = response.get('status', 'unknown')
        result = response.get('result', response.get('error', ''))
        return status, result
    except Exception as e:
        return 'exception', str(e)

tests = [
    ('read_blueprint_struct',     'struct_path',     '/Game/HorrorEngine/Blueprints/HE_GameplaySettings'),
    ('read_blueprint_struct',     'struct_path',     '/Game/HorrorEngine/Blueprints/HE_VideoSettings'),
    ('read_input_action',         'asset_path',      '/Game/HorrorEngine/Input/HE_Use'),
    ('read_input_action',         'asset_path',      '/Game/HorrorEngine/Input/HE_Move'),
    ('read_input_mapping_context','asset_path',      '/Game/HorrorEngine/Input/IMC_HorrorEngine'),
    ('read_widget_variables',     'blueprint_path',  '/Game/HorrorEngine/Blueprints/Widgets/MainHUD/MainHUD_WB'),
    ('read_widget_variables',     'blueprint_path',  '/Game/HorrorEngine/Blueprints/Widgets/MainHUD/Inventory_WB'),
    ('read_savegame_blueprint',   'blueprint_path',  '/Game/HorrorEngine/Blueprints/Core/HE_SaveGame'),
    ('read_material',             'asset_path',      '/Game/HorrorEngine/Materials/M_Master'),
    ('read_material',             'asset_path',      '/Game/HorrorEngine/Materials/M_Master_Color'),
    ('read_blueprint_macros',     'blueprint_path',  '/Game/HorrorEngine/Blueprints/Core/HE_Macros'),
    ('read_sound_class',          'asset_path',      '/Game/HorrorEngine/Audio/_SoundSettings/SC_MasterSound'),
    ('read_sound_class',          'asset_path',      '/Game/HorrorEngine/Audio/_SoundSettings/ATT_General'),
]

print("=" * 70)
print(f"{'TOOL':<30} {'STATUS':<10} {'SUMMARY'}")
print("=" * 70)

for cmd, param_key, path in tests:
    status, result = send(cmd, {param_key: path})
    asset_name = path.split('/')[-1]

    if status == 'success':
        # Build a short summary per tool type
        if 'properties' in result:
            summary = f"{len(result['properties'])} properties"
        elif 'variables' in result:
            summary = f"{len(result['variables'])} variables"
        elif 'mappings' in result:
            summary = f"{len(result['mappings'])} key mappings"
        elif 'functions' in result:
            summary = f"{len(result['functions'])} functions"
        elif 'macros' in result:
            summary = f"{len(result['macros'])} macros"
        elif 'scalar_parameters' in result:
            summary = (f"{len(result['scalar_parameters'])} scalars, "
                       f"{len(result['vector_parameters'])} vectors, "
                       f"{len(result['texture_parameters'])} textures")
        elif 'value_type' in result:
            summary = f"value_type={result['value_type']}, triggers={len(result['triggers'])}, modifiers={len(result['modifiers'])}"
        else:
            summary = str(result)[:60]
        print(f"  {cmd:<30} OK    [{asset_name}] {summary}")
    else:
        print(f"  {cmd:<30} FAIL  [{asset_name}] {str(result)[:60]}")

    time.sleep(0.15)

print("=" * 70)
