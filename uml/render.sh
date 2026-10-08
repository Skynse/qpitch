#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
JAVA_TOOL_OPTIONS="${JAVA_TOOL_OPTIONS:-} -Djava.awt.headless=true" \
    plantuml -tsvg -failfast2 architecture.puml classes.puml audio-flow.puml state-and-ui.puml

# SVG viewers do not all honor PlantUML's CSS background declaration.
# Draw a white rectangle behind the diagram to make the background opaque.
python3 - <<'PY'
from pathlib import Path
import re

for name in ('architecture', 'classes', 'audio-flow', 'state-and-ui'):
    path = Path(name + '.svg')
    svg = path.read_text()
    svg = re.sub(r'(<svg\b[^>]*>)',
                 r'\1<rect width="100%" height="100%" fill="#ffffff"/>',
                 svg, count=1)
    path.write_text(svg)
PY
