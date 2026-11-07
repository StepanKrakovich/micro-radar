Import("env")

import json
from pathlib import Path


credentials_path = Path(env.subst("$PROJECT_DIR")) / "credentials.json"
try:
    with credentials_path.open(encoding="utf-8") as credentials_file:
        credentials = json.load(credentials_file)
except FileNotFoundError as error:
    raise RuntimeError("Missing credentials.json with OpenSky clientId and clientSecret") from error
except (OSError, json.JSONDecodeError) as error:
    raise RuntimeError("Could not read credentials.json as valid JSON") from error

client_id = credentials.get("clientId")
client_secret = credentials.get("clientSecret")
if not isinstance(client_id, str) or not client_id.strip():
    raise RuntimeError("credentials.json must contain a non-empty clientId")
if not isinstance(client_secret, str) or not client_secret.strip():
    raise RuntimeError("credentials.json must contain a non-empty clientSecret")

env.Append(CPPDEFINES=[
    ("OPENSKY_CLIENT_ID", env.StringifyMacro(client_id)),
    ("OPENSKY_CLIENT_SECRET", env.StringifyMacro(client_secret)),
])
