"""Generate synthetic Work timer data; never reads an existing user history."""
import json
from pathlib import Path
import sys

output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
for count in (1000, 10000):
    path = output / f"history-{count}.json"
    if path.exists():
        raise SystemExit(f"Refusing to replace an existing fixture: {path}")
    sessions = [{
        "id": f"s-{i}", "task": f"Task {i % 20}", "durationMs": 60000,
        "operations": 20, "changedTiles": 10, "mapKey": "fixture",
        "mapName": "audit.otbm", "startedAt": "2026-01-01T10:00:00Z",
        "endedAt": "2026-01-01T10:01:00Z"
    } for i in range(count)]
    active = {"id": "paused", "task": "Task 0", "durationMs": 60000,
              "mapKey": "fixture", "mapName": "audit.otbm"}
    path.write_text(json.dumps({"schemaVersion": 1, "sessions": sessions,
                               "active": active}), encoding="utf-8")
