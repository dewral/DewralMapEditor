#!/usr/bin/env python3
"""Import a standalone RME material folder into one local DME profile."""

from __future__ import annotations

import argparse
import copy
import importlib.util
import json
import shutil
import tempfile
import uuid
import xml.etree.ElementTree as ET
from collections import OrderedDict
from pathlib import Path

spec = importlib.util.spec_from_file_location("rme_data", Path(__file__).with_name("import-rme-data.py"))
assert spec and spec.loader
rme = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rme)

SIDECARS = ("tilesetsa.xml", "raw_palette.xml", "item_palette.xml", "collections.xml", "walls_extra.xml")
BRUSH_KINDS = ("grounds", "walls", "doodads", "carpets", "tables")


def convert_materials(source: Path) -> tuple[dict, dict, list[str]]:
    """Normalize legacy material types while retaining source placement IDs."""
    material_files = rme.included_material_files(source)
    for name in SIDECARS:
        path = (source / name).resolve()
        if path.is_file() and path not in material_files:
            material_files.append(path)
    roots = [rme.read_xml(path) for path in material_files]
    metaitems = {rme.integer(node.get("id")) for root in roots for node in root.findall("./metaitem")}
    definitions = OrderedDict()
    for root in roots:
        for node in root.findall("./brush"):
            name = node.get("name")
            if name and node.get("type"):
                definitions[name] = copy.deepcopy(node)

    normalized = {name: ET.Element("materials") for name in (
        "borders.xml", "grounds.xml", "walls.xml", "doodads.xml", "tilesets.xml", "materials.xml",
    )}
    warnings = []
    for root in roots:
        for border in root.findall("./border"):
            normalized["borders.xml"].append(copy.deepcopy(border))
        for tileset in root.iter("tileset"):
            normalized["tilesets.xml"].append(copy.deepcopy(tileset))
    for name, node in definitions.items():
        kind = node.get("type")
        if kind == "border":
            kind = "ground"
            node.set("type", kind)
        if not list(node) and not node.get("server_lookid"):
            warnings.append(f"Empty brush placeholder omitted: {name}")
            continue
        if node.get("lookid") and not node.get("server_lookid"):
            # XML lookid may be a client sprite ID. DME always wants server IDs.
            item_id = next((ids[0] for item in node.iter("item")
                            if (ids := rme.expanded_ids(item))), 0)
            if not item_id:
                raise ValueError(f"No server preview ID for {name}")
            node.set("server_lookid", str(item_id))
        if kind == "ground":
            normalized["grounds.xml"].append(node)
        elif kind == "wall":
            normalized["walls.xml"].append(node)
            if node.findall("./friend"):
                warnings.append(f"DME does not support wall friend/redirect rules: {name}")
        elif kind in ("doodad", "carpet", "table"):
            normalized["doodads.xml"].append(node)
        elif kind == "wall decoration":
            # Doodad variants place decorations on a wall without replacing it.
            decoration = ET.Element("brush", {"name": name, "type": "doodad",
                                               "server_lookid": node.get("server_lookid", "0")})
            for wall in node.findall("./wall"):
                alternate = ET.SubElement(decoration, "alternate")
                for item in wall.findall("./item"):
                    alternate.append(copy.deepcopy(item))
                for door in wall.findall("./door"):
                    window = ET.SubElement(decoration, "alternate")
                    ET.SubElement(window, "item", {"id": door.get("id", "0"), "chance": "1"})
            normalized["doodads.xml"].append(decoration)
            warnings.append(f"Wall decoration offered as doodad orientation variants: {name}")
        else:
            raise ValueError(f"Unsupported brush type {kind!r}: {name}")
        if node.findall(".//specific") or any(b.get("super") or b.get("ground_equivalent") for b in node.findall("./border")):
            warnings.append(f"DME does not support border-specific actions or super/ground-equivalent rules: {name}")

    for name in normalized:
        if name != "materials.xml":
            ET.SubElement(normalized["materials.xml"], "include", {"file": name})
    with tempfile.TemporaryDirectory(prefix="dme-materials-") as temporary:
        directory = Path(temporary)
        for name, root in normalized.items():
            (directory / name).write_bytes(ET.tostring(root, encoding="utf-8"))
        borders = rme.convert_borders(directory)
        brushes = OrderedDict((
            ("borders", borders),
            ("grounds", rme.convert_grounds(directory, borders)),
            ("walls", rme.convert_walls(directory)),
            ("doors", rme.convert_doors(directory)),
            ("doodads", rme.convert_doodads(directory)),
            ("carpets", rme.convert_connected_brushes(directory, "carpet", "carpet")),
            ("tables", rme.convert_connected_brushes(directory, "table", "table")),
        ))
        tilesets = rme.convert_tilesets(directory, brushes)

    # RME metaitems are editor-only placeholders, absent from the server OTB.
    # DME has no metaitem implementation; never emit them as real map items.
    for key, border in borders.items():
        omitted = set(border) & metaitems
        if omitted:
            borders[key] = [0 if item_id in metaitems else item_id for item_id in border]
            warnings.append(f"Editor-only metaitems {sorted(omitted)} omitted from border {key}")
    for kind in ("grounds", "walls", "carpets", "tables"):
        for name, brush in brushes[kind].items():
            groups = {"ground": brush["items"]} if kind == "grounds" else brush["items"]
            for slot, values in list(groups.items()):
                omitted = {pair[0] for pair in values} & metaitems
                if omitted:
                    groups[slot] = [pair for pair in values if pair[0] not in metaitems]
                    warnings.append(f"Editor-only metaitems {sorted(omitted)} omitted from {name}/{slot}")
                    if not groups[slot]:
                        del groups[slot]
            if kind == "grounds":
                brush["items"] = groups.get("ground", [])
            if not brush["items"]:
                raise ValueError(f"No server items remain in {name}")
    for category in tilesets.values():
        for name, values in category.items():
            category[name] = [item_id for item_id in values if item_id not in metaitems]

    for name, ground in brushes["grounds"].items():
        for border in ground["borders"]:
            if border["border"] not in borders:
                warnings.append(f"Missing border {border['border']} referenced by {name}; that border rule was omitted")
        ground["borders"] = [border for border in ground["borders"] if border["border"] in borders]
        if ground["optional"] and ground["optional"] not in borders:
            warnings.append(f"Missing optional border {ground['optional']} referenced by {name}; rule omitted")
            ground["optional"] = ""

    known = {name for kind in BRUSH_KINDS for name in brushes[kind]}
    for tileset in normalized["tilesets.xml"].iter("tileset"):
        name = tileset.get("name")
        if not name:
            continue
        for category in tileset:
            targets = rme.CATEGORY_TARGETS.get(category.tag, ())
            for target in targets:
                tilesets[target].setdefault(name, [])
            if targets:
                for entry in category.findall("./brush"):
                    if not entry.get("item") and entry.get("name") not in known:
                        warnings.append(f"Missing palette brush {entry.get('name')!r} in {category.tag}/{name}; entry omitted")
    # Reject unusable results before touching an existing profile.
    if not any(brushes[kind] for kind in BRUSH_KINDS):
        raise ValueError("No usable brush definitions found")
    if not any(tilesets.values()):
        raise ValueError("No usable tileset categories found")
    return brushes, tilesets, list(dict.fromkeys(warnings))


def import_profile(source: Path, destination: Path, dry_run: bool = False,
                   items_xml: Path | None = None) -> dict:
    source = source.resolve()
    destination = destination.resolve()
    brushes, tilesets, warnings = convert_materials(source)
    # Parse every optional metadata file before backing up or changing anything.
    contents = {
        "brushes.json": (json.dumps(brushes, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8"),
        "tilesets.json": (json.dumps(tilesets, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8"),
    }
    for name in ("items.xml", "creatures.xml"):
        if (source / name).is_file():
            contents[name] = rme.sanitized_xml(source / name)
    if items_xml is not None:
        contents["items.xml"] = rme.sanitized_xml(items_xml.resolve())
    report = {
        "source": str(source), "destination": str(destination),
        "brush_counts": {kind: len(values) for kind, values in brushes.items()},
        "tileset_counts": {kind: len(groups) for kind, groups in tilesets.items()},
        "files": list(contents), "warnings": warnings, "dry_run": dry_run,
    }
    if dry_run:
        return report
    destination.mkdir(parents=True, exist_ok=True)
    originals = {name: (destination / name).read_bytes() if (destination / name).exists() else None for name in contents}
    existing = [name for name in (*contents, "material-import-report.json") if (destination / name).exists()]
    backup = None
    if existing:
        backup = destination / ("backup-before-material-import-" + uuid.uuid4().hex[:8])
        backup.mkdir()
        for name in existing:
            shutil.copy2(destination / name, backup / name)
            if name in originals and (backup / name).read_bytes() != originals[name]:
                raise RuntimeError(f"Profile file changed while backing up: {name}")
        report["backup"] = str(backup)
    written = []
    try:
        for name, data in contents.items():
            # Reject a concurrent editor save rather than overwriting it.
            current = (destination / name).read_bytes() if (destination / name).exists() else None
            if current != originals[name]:
                raise RuntimeError(f"Profile file changed during import: {name}")
            rme.write_bytes_atomic(destination / name, data)
            written.append(name)
            if (destination / name).read_bytes() != data:
                raise RuntimeError(f"Could not verify imported file: {name}")
        rme.write_json_atomic(destination / "material-import-report.json", report)
    except BaseException:
        for name in written:
            if originals[name] is None:
                (destination / name).unlink()
            else:
                rme.write_bytes_atomic(destination / name, originals[name])
        raise
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--materials", required=True, type=Path, help="Standalone RME XML material folder")
    parser.add_argument("--destination", required=True, type=Path, help="Exact DME data/<profile> folder to update")
    parser.add_argument("--dry-run", action="store_true", help="Show counts and source gaps without writing files")
    parser.add_argument("--items-xml", type=Path, help="Optional server items.xml to use instead of source metadata")
    arguments = parser.parse_args()
    try:
        report = import_profile(arguments.materials, arguments.destination, arguments.dry_run, arguments.items_xml)
    except (OSError, ValueError, RuntimeError, ET.ParseError) as error:
        parser.exit(1, f"Import failed: {error}\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
