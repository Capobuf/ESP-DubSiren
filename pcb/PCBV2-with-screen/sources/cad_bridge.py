"""Execute a saved CAD script through the installed FreeCAD MCP bridge."""
import json
import sys
import xmlrpc.client
from pathlib import Path

path = Path(sys.argv[1]).resolve()
bridge = xmlrpc.client.ServerProxy('http://127.0.0.1:9875', allow_none=True)
result = bridge.execute(f"exec(compile({path.read_text(encoding='utf-8')!r}, {str(path)!r}, 'exec'), dict(globals(), __file__={str(path)!r}))")
print(json.dumps(result, indent=2))
if not result.get('success'):
    sys.exit(1)
