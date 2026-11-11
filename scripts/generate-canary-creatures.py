#!/usr/bin/env python3
"""Build a DME creatures.xml from a Canary (opentibiabr) server data folder.

Reads monster/**/*.lua and npc/**/*.lua and keeps only the name, the type and
the outfit (lookType/lookTypeEx and colours), which is all DME needs to show
and place creatures. Nothing else from the server scripts is copied.

Example:
  python scripts/generate-canary-creatures.py \
      --data canary/data-otservbr-global --output data/1511/creatures.xml
"""

from __future__ import annotations

import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path

STRING = r'"((?:[^"\\]|\\.)*)"'
CREATE = re.compile(r'Game\.create(Monster|Npc)Type\(\s*(?:' + STRING + r'|([A-Za-z_][A-Za-z0-9_]*))')
OUTFIT = re.compile(r'\.outfit\s*=\s*\{(.*?)\}', re.S)
FIELD = re.compile(r'(look[A-Za-z]+)\s*=\s*(-?\d+)')


def string_variable(text: str, name: str) -> str | None:
    match = re.search(r'\b' + re.escape(name) + r'\s*=\s*' + STRING, text)
    return match.group(1) if match else None


def parse(path: Path) -> list[dict]:
    text = path.read_text(encoding="utf-8", errors="replace")
    outfit_match = OUTFIT.search(text)
    fields = {k.lower(): int(v) for k, v in FIELD.findall(outfit_match.group(1))} if outfit_match else {}
    creatures = []
    for kind, literal, variable in CREATE.findall(text):
        name = literal or (string_variable(text, variable) if variable else None)
        if not name or name == "...":
            continue
        creatures.append({"name": name.replace('\\"', '"'), "type": kind.lower(), **fields})
    return creatures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", type=Path, required=True, help="Canary data-<datapack> folder")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    found: dict[tuple[str, str], dict] = {}
    for folder in ("monster", "npc"):
        for path in sorted((args.data / folder).rglob("*.lua")):
            for creature in parse(path):
                found.setdefault((creature["type"], creature["name"].casefold()), creature)

    root = ET.Element("creatures")
    for (_, _), creature in sorted(found.items(), key=lambda item: (item[0][0], item[0][1])):
        attributes = {"name": creature["name"], "type": creature["type"]}
        look_type = creature.get("looktype", 0)
        look_item = creature.get("looktypeex", 0)
        if look_type:
            attributes["looktype"] = str(look_type)
        elif look_item:
            attributes["lookitem"] = str(look_item)
        for source, target in (("lookhead", "lookhead"), ("lookbody", "lookbody"),
                               ("looklegs", "looklegs"), ("lookfeet", "lookfeet"),
                               ("lookaddons", "lookaddon")):
            if creature.get(source):
                attributes[target] = str(creature[source])
        ET.SubElement(root, "creature", attributes)
    ET.indent(root, space="\t")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(args.output, encoding="utf-8", xml_declaration=True)
    monsters = sum(1 for key in found if key[0] == "monster")
    print(f"Wrote {len(found)} creatures ({monsters} monsters, {len(found) - monsters} NPCs) to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
