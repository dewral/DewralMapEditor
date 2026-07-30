#!/usr/bin/env python3
"""Convert OTAcademy/RME material data to Dewral Map Editor profiles.

The importer intentionally copies no client binaries. It converts the material
definitions understood by DME and copies the text-only item and creature
metadata used by the editor.
"""

from __future__ import annotations

import argparse
import json
import os
import tempfile
import xml.etree.ElementTree as ET
from collections import OrderedDict
from pathlib import Path
from typing import Iterable


# DME profiles and the closest material directory available in OTAcademy/RME.
# RME does not ship separate material sets for 7.72, 7.80, or 7.92. The 7.60
# set is the conservative compatible fallback: it does not introduce 8.00 IDs.
PROFILE_SOURCES: tuple[tuple[str, str], ...] = (
    ("760", "760"),
    ("772", "760"),
    ("780", "760"),
    ("792", "760"),
    ("800", "800"),
    ("810", "810"),
    ("820", "820"),
    ("840", "840"),
    ("850", "850"),
    ("854", "854"),
    ("860", "860"),
    ("870", "870"),
    ("910", "910"),
    ("920", "920"),
    ("946", "946"),
    ("954", "954"),
    ("960", "960"),
    ("986", "986"),
    ("1010", "1010"),
    ("1030", "1030"),
    ("1041", "1041"),
    ("1077", "1077"),
    ("1098", "1098"),
)

EDGE_INDEX = {
    "n": 1,
    "e": 2,
    "s": 3,
    "w": 4,
    "cnw": 5,
    "cne": 6,
    "csw": 7,
    "cse": 8,
    "dnw": 9,
    "dne": 10,
    "dse": 11,
    "dsw": 12,
}

WALL_ALIGNMENT = {
    "pole": 0,
    "south end": 1,
    "east end": 2,
    "northwest diagonal": 3,
    "corner": 3,
    "west end": 4,
    "northeast diagonal": 5,
    "horizontal": 6,
    "south t": 7,
    "north end": 8,
    "vertical": 9,
    "southwest diagonal": 10,
    "east t": 11,
    "southeast diagonal": 12,
    "west t": 13,
    "north t": 14,
    "intersection": 15,
    "untouchable": 16,
}

CATEGORY_TARGETS = {
    "terrain": ("terrain",),
    "collections": ("collection",),
    "collections_and_raw": ("collection", "raw"),
    "collections_and_terrain": ("collection", "terrain"),
    "doodad": ("doodad",),
    "doodad_and_raw": ("doodad", "raw"),
    "item": ("item",),
    "items": ("item",),
    "items_and_raw": ("item", "raw"),
    "raw": ("raw",),
}


def integer(value: str | None, default: int = 0) -> int:
    try:
        return int(value or "")
    except ValueError:
        return default


def read_xml(path: Path) -> ET.Element:
    # Several RME XML files contain a trailing NUL byte. PugiXML accepts it,
    # while Python and Qt's strict XML readers do not.
    contents = path.read_bytes().replace(b"\x00", b"")
    return ET.fromstring(contents)


def write_bytes_atomic(path: Path, contents: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(prefix=path.name, dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(contents)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def write_json_atomic(path: Path, value: object) -> None:
    # Compact JSON keeps the release small; DME does not depend on formatting.
    encoded = (
        json.dumps(value, ensure_ascii=False, separators=(",", ":")) + "\n"
    ).encode("utf-8")
    write_bytes_atomic(path, encoded)


def expanded_ids(node: ET.Element) -> list[int]:
    if node.get("id") is not None:
        item_id = integer(node.get("id"))
        return [item_id] if item_id > 0 else []

    first = integer(node.get("fromid"))
    last = integer(node.get("toid"))
    if first <= 0 or last < first or last - first > 100_000:
        return []
    return list(range(first, last + 1))


def append_unique(target: list[int], values: Iterable[int]) -> None:
    present = set(target)
    for value in values:
        if value > 0 and value not in present:
            target.append(value)
            present.add(value)


def brush_look_id(node: ET.Element, fallback: int = 0) -> int:
    return integer(node.get("server_lookid") or node.get("lookid"), fallback)


def convert_borders(source_directory: Path) -> OrderedDict[str, list[int]]:
    result: OrderedDict[str, list[int]] = OrderedDict()
    root = read_xml(source_directory / "borders.xml")
    for node in root.findall("./border"):
        border_id = integer(node.get("id"))
        if border_id <= 0:
            continue

        tiles = [0] * 13
        for item in node.findall("./borderitem"):
            index = EDGE_INDEX.get((item.get("edge") or "").lower())
            if index is not None:
                tiles[index] = integer(item.get("item"))
        result[str(border_id)] = tiles
    return result


def inline_border_tiles(node: ET.Element) -> list[int]:
    tiles = [0] * 13
    for item in node.findall("./borderitem"):
        index = EDGE_INDEX.get((item.get("edge") or "").lower())
        if index is not None:
            tiles[index] = integer(item.get("item"))
    return tiles


def convert_grounds(
    source_directory: Path,
    global_borders: OrderedDict[str, list[int]],
) -> OrderedDict[str, dict]:
    result: OrderedDict[str, dict] = OrderedDict()
    root = read_xml(source_directory / "grounds.xml")
    for node in root.findall("./brush"):
        if node.get("type") != "ground" or not node.get("name"):
            continue

        items: list[list[int]] = []
        for item in node.findall("./item"):
            for item_id in expanded_ids(item):
                items.append([item_id, integer(item.get("chance"))])
        if not items:
            continue

        borders: list[dict[str, str]] = []
        for border in node.findall("./border"):
            border_id = integer(border.get("id"))
            if border_id > 0:
                border_key = str(border_id)
            else:
                inline_tiles = inline_border_tiles(border)
                if not any(inline_tiles):
                    continue
                border_key = f"g{sum(key.startswith('g') for key in global_borders)}"
                global_borders[border_key] = inline_tiles
            destination = border.get("to")
            if destination is None or destination == "all":
                destination = "*"
            elif destination == "none":
                destination = ""
            borders.append(
                {
                    "align": (
                        "inner"
                        if (border.get("align") or "").lower() == "inner"
                        else "outer"
                    ),
                    "border": border_key,
                    "to": destination,
                }
            )

        friends: list[str] = []
        hate_friends = False
        for child in node:
            if child.tag == "clear_friends":
                friends.clear()
                hate_friends = False
            elif child.tag in ("friend", "enemy"):
                name = child.get("name") or ""
                if name:
                    friends.append("*" if name == "all" else name)
                hate_friends = child.tag == "enemy"

        optional = node.find("./optional")
        optional_id = optional.get("id") if optional is not None else ""
        first_item_id = items[0][0]
        result[node.get("name") or ""] = {
            "borders": borders,
            "friends": friends,
            "hate_friends": hate_friends,
            "items": items,
            "lookid": brush_look_id(node, first_item_id),
            "optional": optional_id or "",
            "zorder": integer(node.get("z-order"), 1),
        }
    return result


def convert_walls(source_directory: Path) -> OrderedDict[str, dict]:
    result: OrderedDict[str, dict] = OrderedDict()
    for material_file in included_material_files(source_directory):
        root = read_xml(material_file)
        for node in root.findall("./brush"):
            if node.get("type") != "wall" or not node.get("name"):
                continue

            wall_items: OrderedDict[str, list[list[int]]] = OrderedDict()
            first_item_id = 0
            for wall in node.findall("./wall"):
                alignment = WALL_ALIGNMENT.get((wall.get("type") or "").lower())
                if alignment is None:
                    continue
                values: list[list[int]] = []
                for item in wall.findall("./item"):
                    chance = integer(item.get("chance"), 100)
                    for item_id in expanded_ids(item):
                        values.append([item_id, chance])
                        if first_item_id == 0:
                            first_item_id = item_id
                if values:
                    wall_items[str(alignment)] = values

            if not wall_items:
                continue
            result[node.get("name") or ""] = {
                "lookid": brush_look_id(node, first_item_id),
                "items": wall_items,
            }
    return result


def convert_doors(source_directory: Path) -> OrderedDict[str, dict]:
    result: OrderedDict[str, dict] = OrderedDict()
    for material_file in included_material_files(source_directory):
        root = read_xml(material_file)
        for brush in root.findall("./brush"):
            if brush.get("type") != "wall" or not brush.get("name"):
                continue
            brush_name = brush.get("name") or ""
            for wall in brush.findall("./wall"):
                alignment_name = (wall.get("type") or "").lower()
                alignment = WALL_ALIGNMENT.get(alignment_name)
                if alignment is None:
                    continue

                groups: OrderedDict[str, list[dict]] = OrderedDict()
                for door in wall.findall("./door"):
                    door_id = integer(door.get("id"))
                    if door_id <= 0:
                        continue
                    door_type = (door.get("type") or "normal").strip().replace(" ", "_")
                    groups.setdefault(door_type, []).append(
                        {
                            "id": door_id,
                            "open": (door.get("open") or "true").lower() == "true",
                            "locked": (door.get("locked") or "false").lower() == "true",
                        }
                    )

                for door_type, variants in groups.items():
                    opened = [entry for entry in variants if entry["open"]]
                    closed = [entry for entry in variants if not entry["open"]]
                    preferred_closed = (
                        next(
                            (entry for entry in closed if not entry["locked"]),
                            closed[0],
                        )
                        if closed
                        else None
                    )
                    for entry in variants:
                        target = preferred_closed if entry["open"] else (
                            opened[0] if opened else None
                        )
                        result[str(entry["id"])] = {
                            "to": target["id"] if target else 0,
                            "open": entry["open"],
                            "brush": brush_name,
                            "align": alignment,
                            "type": door_type,
                            "locked": entry["locked"],
                        }
    return result


def convert_alternative(node: ET.Element) -> dict:
    singles: list[list[int]] = []
    composites: list[dict] = []

    for item in node.findall("./item"):
        chance = integer(item.get("chance"), 1)
        for item_id in expanded_ids(item):
            singles.append([item_id, chance])

    for composite in node.findall("./composite"):
        tiles: list[dict] = []
        for tile in composite.findall("./tile"):
            item_ids: list[int] = []
            for item in tile.findall("./item"):
                item_ids.extend(expanded_ids(item))
            if item_ids:
                tiles.append(
                    {
                        "dx": integer(tile.get("x")),
                        "dy": integer(tile.get("y")),
                        "dz": integer(tile.get("z")),
                        "items": item_ids,
                    }
                )
        if tiles:
            composites.append(
                {
                    "chance": integer(composite.get("chance"), 1),
                    "tiles": tiles,
                }
            )
    return {"singles": singles, "composites": composites}


def merge_alternative(target: dict, source: dict) -> None:
    target["singles"].extend(source["singles"])
    target["composites"].extend(source["composites"])


def first_doodad_item(alternatives: list[dict]) -> int:
    for alternative in alternatives:
        if alternative["singles"]:
            return alternative["singles"][0][0]
        for composite in alternative["composites"]:
            for tile in composite["tiles"]:
                if tile["items"]:
                    return tile["items"][0]
    return 0


def convert_doodads(source_directory: Path) -> OrderedDict[str, dict]:
    result: OrderedDict[str, dict] = OrderedDict()
    root = read_xml(source_directory / "doodads.xml")
    for node in root.findall("./brush"):
        if node.get("type") != "doodad" or not node.get("name"):
            continue

        alternatives: list[dict] = []
        for alternate in node.findall("./alternate"):
            converted = convert_alternative(alternate)
            if converted["singles"] or converted["composites"]:
                alternatives.append(converted)

        direct = convert_alternative(node)
        if direct["singles"] or direct["composites"]:
            if alternatives:
                merge_alternative(alternatives[-1], direct)
            else:
                alternatives.append(direct)

        if not alternatives:
            continue
        fallback = first_doodad_item(alternatives)
        result[node.get("name") or ""] = {
            "lookid": brush_look_id(node, fallback),
            "alternates": alternatives,
        }
    return result


def included_material_files(source_directory: Path) -> list[Path]:
    result: list[Path] = []
    visited: set[Path] = set()

    def visit(path: Path) -> None:
        path = path.resolve()
        if path in visited:
            return
        if not path.is_file():
            raise FileNotFoundError(f"RME material include is missing: {path}")
        visited.add(path)
        result.append(path)
        for include in read_xml(path).findall("./include"):
            filename = include.get("file")
            if filename:
                visit(path.parent / filename)

    visit(source_directory / "materials.xml")
    return result


def convert_connected_brushes(
    source_directory: Path,
    brush_type: str,
    child_tag: str,
) -> OrderedDict[str, dict]:
    result: OrderedDict[str, dict] = OrderedDict()
    for material_file in included_material_files(source_directory):
        root = read_xml(material_file)
        for node in root.findall("./brush"):
            if node.get("type") != brush_type or not node.get("name"):
                continue
            alignments: OrderedDict[str, list[list[int]]] = OrderedDict()
            first_item_id = 0
            for part in node.findall(f"./{child_tag}"):
                alignment = (part.get("align") or "").lower()
                if not alignment:
                    continue
                values: list[list[int]] = []
                direct_id = integer(part.get("id"))
                if direct_id > 0:
                    values.append([direct_id, 1])
                for item in part.findall("./item"):
                    chance = integer(item.get("chance"), 1)
                    for item_id in expanded_ids(item):
                        values.append([item_id, chance])
                if not values:
                    continue
                alignments[alignment] = values
                if first_item_id == 0:
                    first_item_id = values[0][0]
            if alignments:
                result[node.get("name") or ""] = {
                    "lookid": brush_look_id(node, first_item_id),
                    "items": alignments,
                }
    return result


def convert_tilesets(
    source_directory: Path,
    brushes: dict[str, OrderedDict[str, dict]],
) -> OrderedDict[str, OrderedDict[str, list[int]]]:
    result = OrderedDict(
        (category, OrderedDict())
        for category in ("terrain", "doodad", "item", "raw", "collection", "door")
    )
    material_files = included_material_files(source_directory)
    look_ids: dict[str, int] = {}
    # Specialized RME brushes (carpets, tables, archways, and similar) are not
    # automagic brush types in DME yet. They still need a representative item
    # so their RME tileset membership is not lost.
    for material_file in material_files:
        root = read_xml(material_file)
        for node in root.findall("./brush"):
            name = node.get("name")
            if not name or not node.get("type"):
                continue
            fallback = 0
            for item in node.iter("item"):
                ids = expanded_ids(item)
                if ids:
                    fallback = ids[0]
                    break
            look_id = brush_look_id(node, fallback)
            if look_id > 0:
                look_ids[name] = look_id

    for brush_type in ("grounds", "walls", "doodads", "carpets", "tables"):
        for name, definition in brushes[brush_type].items():
            look_id = integer(str(definition.get("lookid", 0)))
            if look_id > 0:
                look_ids[name] = look_id

    for material_file in material_files:
        root = read_xml(material_file)
        for tileset in root.iter("tileset"):
            name = tileset.get("name")
            if not name:
                continue
            for category_node in tileset:
                targets = CATEGORY_TARGETS.get(category_node.tag, ())
                if not targets:
                    continue

                values: list[int] = []
                for entry in category_node:
                    if entry.tag == "item":
                        append_unique(values, expanded_ids(entry))
                    elif entry.tag == "brush":
                        if entry.get("item"):
                            append_unique(values, [integer(entry.get("item"))])
                        else:
                            append_unique(values, [look_ids.get(entry.get("name") or "", 0)])

                for category in targets:
                    destination = result[category].setdefault(name, [])
                    append_unique(destination, values)

    # Empty groups are not useful in DME and are ignored by TilesetStore.
    for category in result:
        result[category] = OrderedDict(
            (name, values)
            for name, values in result[category].items()
            if values
        )

    all_doodads: list[int] = []
    for definition in brushes["doodads"].values():
        append_unique(all_doodads, [integer(str(definition.get("lookid", 0)))])
    if all_doodads:
        result["doodad"] = OrderedDict(
            [("All Doodads", all_doodads), *result["doodad"].items()]
        )

    door_type_names = {
        "normal": "Normal Doors",
        "normal_alt": "Alternative Doors",
        "locked": "Locked Doors",
        "quest": "Quest Doors",
        "magic": "Magic Doors",
        "archway": "Archways",
        "window": "Windows",
        "hatch_window": "Hatch Windows",
    }
    door_examples: OrderedDict[str, list[int]] = OrderedDict()
    for item_id, definition in brushes["doors"].items():
        door_type = str(definition.get("type", "normal"))
        if door_type == "any_window":
            continue
        label = door_type_names.get(
            door_type,
            door_type.replace("_", " ").title(),
        )
        values = door_examples.setdefault(label, [])
        if not values:
            values.append(integer(item_id))
    result["door"] = door_examples
    return result


def sanitized_xml(source: Path) -> bytes:
    contents = source.read_bytes().replace(b"\x00", b"")
    # Parse before copying so a malformed metadata file fails the import.
    ET.fromstring(contents)
    return contents


def convert_profile(
    rme_data_root: Path,
    output_root: Path,
    profile: str,
    source_version: str,
) -> dict[str, int]:
    source_directory = rme_data_root / source_version
    required = (
        "materials.xml",
        "borders.xml",
        "grounds.xml",
        "walls.xml",
        "doodads.xml",
        "tilesets.xml",
        "items.xml",
        "creatures.xml",
    )
    missing = [
        filename
        for filename in required
        if not (source_directory / filename).is_file()
    ]
    if missing:
        raise FileNotFoundError(
            f"RME {source_version} is missing: {', '.join(missing)}"
        )

    borders = convert_borders(source_directory)
    brushes = OrderedDict(
        (
            ("borders", borders),
            ("grounds", convert_grounds(source_directory, borders)),
            ("walls", convert_walls(source_directory)),
            ("doors", convert_doors(source_directory)),
            ("doodads", convert_doodads(source_directory)),
            (
                "carpets",
                convert_connected_brushes(source_directory, "carpet", "carpet"),
            ),
            (
                "tables",
                convert_connected_brushes(source_directory, "table", "table"),
            ),
        )
    )
    tilesets = convert_tilesets(source_directory, brushes)

    destination = output_root / profile
    write_json_atomic(destination / "brushes.json", brushes)
    write_json_atomic(destination / "tilesets.json", tilesets)
    write_bytes_atomic(
        destination / "items.xml",
        sanitized_xml(source_directory / "items.xml"),
    )
    write_bytes_atomic(
        destination / "creatures.xml",
        sanitized_xml(source_directory / "creatures.xml"),
    )

    return {
        "borders": len(brushes["borders"]),
        "grounds": len(brushes["grounds"]),
        "walls": len(brushes["walls"]),
        "doors": len(brushes["doors"]),
        "doodads": len(brushes["doodads"]),
        "carpets": len(brushes["carpets"]),
        "tables": len(brushes["tables"]),
        "tilesets": sum(len(groups) for groups in tilesets.values()),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert OTAcademy/RME materials into DME data profiles."
    )
    parser.add_argument(
        "--rme",
        required=True,
        type=Path,
        help="Path to a checkout of OTAcademy/RME.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "data",
        help="DME data directory (default: repository data directory).",
    )
    arguments = parser.parse_args()

    rme_data_root = arguments.rme.resolve() / "data"
    if not (rme_data_root / "clients.xml").is_file():
        parser.error(f"Not an OTAcademy/RME checkout: {arguments.rme}")

    for profile, source_version in PROFILE_SOURCES:
        counts = convert_profile(
            rme_data_root,
            arguments.output.resolve(),
            profile,
            source_version,
        )
        summary = ", ".join(f"{key}={value}" for key, value in counts.items())
        fallback = (
            f" (RME {source_version} fallback)"
            if profile != source_version
            else ""
        )
        print(f"{profile}{fallback}: {summary}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
