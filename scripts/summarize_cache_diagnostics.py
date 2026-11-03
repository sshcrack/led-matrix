"""Publish backend error categories without copying URLs or credentials from logs."""
import json
import os
from pathlib import Path
import re

root = Path(os.environ["RUNNER_TEMP"])
log = root / "sccache-errors.log"
text = log.read_text(errors="replace") if log.exists() else ""
patterns = {
    "rate_limit": r"too many requests|rate.?limit|(?:HTTP[^\n]*|status[^\n]*)\b429\b",
    "authorization": r"unauthori[sz]ed|forbidden|permission denied",
    "capacity": r"quota|insufficient storage|storage limit|no space left",
    "timeout": r"timed out|timeout",
    "connection": r"connection reset|connection refused|dns error|TLS error",
}
summary = {name: len(re.findall(pattern, text, re.IGNORECASE)) for name, pattern in patterns.items()}
(root / "cache-diagnostics.json").write_text(json.dumps(summary, indent=2))
print("Cache backend diagnostic categories: " + json.dumps(summary))
