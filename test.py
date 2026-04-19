import sys
import traceback
sys.path.insert(0, 'C:/Users/oprincipe/Unreal Projects/AI_Mcp_Test/Plugins/UnrealMCP/Python')

try:
    import graph_memory
    print('SUCCESS')
except Exception as e:
    traceback.print_exc()
