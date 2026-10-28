"""Create diagnostic-only QML variants. Never edits editor/qml.
Usage: python make-ui-startup-variants.py NEW_OUTPUT_DIRECTORY
Then compile variants.qrc with the Qt rcc --binary tool.
Variants are startup ablations, not functioning replacements for application UI.
"""
from pathlib import Path
import re
import shutil
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[3]
output = Path(sys.argv[1]).resolve()
if output.exists():
    raise SystemExit("Refusing to overwrite an existing experiment directory")
output.mkdir(parents=True)


def closing_brace(text, start):
    depth, index, quote = 0, start, None
    while index < len(text):
        char = text[index]
        if quote:
            if char == "\\":
                index += 2
                continue
            if char == quote:
                quote = None
        elif char in ('"', "'", "`"):
            quote = char
        elif text.startswith("//", index):
            end = text.find("\n", index)
            index = len(text) if end < 0 else end
            continue
        elif text.startswith("/*", index):
            index = text.index("*/", index + 2) + 2
            continue
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index + 1
        index += 1
    raise ValueError("Unbalanced diagnostic QML input")


heavy = {"GenerationLibraryDialog", "TileMapGenDialog", "CustomMapGeneratorDialog",
         "LuaGeneratorDialog", "AiMapAssistantDialog", "TerrainGeneratorDialog",
         "GroundClusterGeneratorDialog", "DungeonGeneratorDialog", "BrushEditorDialog"}
resource = ET.Element("RCC")
for name in ("local-full", "url-generators", "url-dialogs", "no-timer-popup"):
    folder = output / name / "qml"
    shutil.copytree(root / "editor/qml", folder)
    if name.startswith("url-"):
        path = folder / "Main.qml"
        text = path.read_text(encoding="utf-8")
        pattern = r"Loader\s*\{\s*id:\s*(\w+)\s+active:\s*false\s+sourceComponent:\s*(\w+)\s*\{"
        changes = []
        for match in re.finditer(pattern, text):
            if name == "url-generators" and match[2] not in heavy:
                continue
            target = "dialogs/" + match[2] + ".qml"
            if not (folder / target).is_file():
                raise ValueError(target)
            start = text.index("sourceComponent:", match.start())
            end = closing_brace(text, match.end() - 1)
            changes.append((start, end, 'source: "' + target + '"'))
        expected = 9 if name == "url-generators" else 30
        if len(changes) != expected:
            raise ValueError("QML has changed: review experiment targets before measuring")
        for start, end, replacement in reversed(changes):
            text = text[:start] + replacement + text[end:]
        path.write_text(text, encoding="utf-8")
        print(name, len(changes), "deferred declarations")
    elif name == "no-timer-popup":
        path = folder / "components/WorkTimerStatus.qml"
        text = path.read_text(encoding="utf-8")
        cut = text.index("\n    Popup {\n        id: panel")
        path.write_text(text[:cut] + "\n    QtObject { id: panel; function open() {} }\n}\n", encoding="utf-8")
    group = ET.SubElement(resource, "qresource", prefix="/audit/" + name)
    for path in sorted(folder.rglob("*")):
        if path.is_file():
            ET.SubElement(group, "file", alias=path.relative_to(folder).as_posix()).text = str(path)
ET.ElementTree(resource).write(output / "variants.qrc", encoding="utf-8", xml_declaration=True)
