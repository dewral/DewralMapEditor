"""Behavioral checks for standalone custom material imports (no client assets)."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location("profile_import", Path(__file__).resolve().parents[1] / "scripts/import-rme-profile.py")
profile_import = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile_import)


class ProfileImportTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.source = Path(self.temporary.name) / "source"
        self.target = Path(self.temporary.name) / "data" / "Midhem"
        self.source.mkdir()
        self.target.mkdir(parents=True)
        files = {
            "materials.xml": '<materials><include file="borders.xml"/><include file="grounds.xml"/><include file="walls.xml"/><include file="doodads.xml"/><include file="tilesets.xml"/></materials>',
            "borders.xml": '<materials><border id="7"><borderitem edge="n" item="200"/></border></materials>',
            "grounds.xml": '<materials><brush name="custom grass" type="border" lookid="999"><item id="100" chance="3"/><item id="101" chance="1"/><border align="outer" id="7"/><border align="inner" id="8"/><optional id="9"/></brush></materials>',
            "walls.xml": '<materials><brush name="wall" type="wall" server_lookid="300"><wall type="horizontal"><item id="300" chance="1"/><door id="301" type="normal" open="false"/><door id="302" type="normal" open="true"/></wall></brush></materials>',
            "doodads.xml": '<materials><brush name="tree" type="doodad" server_lookid="500"><alternate><composite chance="2"><tile x="-1" y="2" z="-1"><item id="500"/><item id="501"/></tile><tile x="0" y="2"><item id="502"/></tile></composite></alternate></brush><brush name="carpet" type="carpet" server_lookid="400"><carpet align="center" id="400"/></brush><brush name="table" type="table" server_lookid="410"><table align="alone"><item id="410" chance="1"/></table></brush></materials>',
            "tilesets.xml": '<materials><tileset name="Nature"><terrain><brush name="custom grass"/></terrain><doodad><brush name="tree"/></doodad></tileset><tileset name="Empty"><raw/></tileset></materials>',
            "walls_extra.xml": '<materials><brush name="extra arch" type="doodad" server_lookid="600"><item id="600" chance="1"/></brush><tileset name="Arches"><terrain><brush name="extra arch"/></terrain></tileset></materials>',
            "tilesetsa.xml": '<materials><tileset name="Custom"><raw><item fromid="700" toid="702"/></raw></tileset></materials>',
            "raw_palette.xml": '<materials><tileset name="Custom"><raw><item id="702"/><item id="703"/></raw></tileset></materials>',
            "item_palette.xml": '<materials><tileset name="Items"><items><item id="800"/></items></tileset></materials>',
            "collections.xml": '<materials><tileset name="Collection"><collections><brush name="custom grass"/><brush name="missing"/></collections></tileset></materials>',
        }
        for name, text in files.items():
            (self.source / name).write_text(text, encoding="utf-8")

    def test_legacy_grounds_server_previews_and_auxiliary_palettes(self):
        before = {p.name: p.read_bytes() for p in self.source.iterdir()}
        report = profile_import.import_profile(self.source, self.target)
        brushes = json.loads((self.target / "brushes.json").read_text())
        tilesets = json.loads((self.target / "tilesets.json").read_text())
        self.assertEqual(brushes["grounds"]["custom grass"]["lookid"], 100)
        self.assertEqual(brushes["grounds"]["custom grass"]["items"], [[100, 3], [101, 1]])
        self.assertEqual(tilesets["terrain"]["Nature"], [100])
        self.assertEqual(tilesets["terrain"]["Arches"], [600])
        self.assertEqual(tilesets["raw"]["Custom"], [700, 701, 702, 703])
        self.assertEqual(tilesets["raw"]["Empty"], [])
        self.assertEqual(tilesets["item"]["Items"], [800])
        self.assertEqual(tilesets["collection"]["Collection"], [100])
        self.assertEqual(brushes["doors"]["301"]["to"], 302)
        self.assertEqual(brushes["doors"]["302"]["to"], 301)
        self.assertIn("carpet", brushes["carpets"])
        self.assertIn("table", brushes["tables"])
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.source.iterdir()})
        self.assertTrue(any("Missing palette brush 'missing'" in warning for warning in report["warnings"]))
        self.assertTrue(any("Missing border 8" in warning for warning in report["warnings"]))
        self.assertEqual(brushes["grounds"]["custom grass"]["optional"], "")

    def test_composite_stacks_offsets_and_chance_survive(self):
        brushes, _, _ = profile_import.convert_materials(self.source)
        composite = brushes["doodads"]["tree"]["alternates"][0]["composites"][0]
        self.assertEqual(composite, {"chance": 2, "tiles": [
            {"dx": -1, "dy": 2, "dz": -1, "items": [500, 501]},
            {"dx": 0, "dy": 2, "dz": 0, "items": [502]},
        ]})

    def test_editor_only_metaitems_are_not_emitted_as_server_items(self):
        materials = self.source / "materials.xml"
        materials.write_text(materials.read_text().replace('<materials>', '<materials><metaitem id="80"/>'))
        (self.source / "borders.xml").write_text('<materials><border id="7"><borderitem edge="n" item="80"/></border></materials>')
        (self.source / "walls.xml").write_text('<materials><brush name="wall" type="wall" server_lookid="300"><wall type="pole"><item id="80" chance="1"/></wall><wall type="horizontal"><item id="300" chance="1"/></wall></brush></materials>')
        brushes, _, warnings = profile_import.convert_materials(self.source)
        self.assertNotIn(80, brushes["borders"]["7"])
        self.assertEqual(brushes["walls"]["wall"]["items"], {"6": [[300, 1]]})
        self.assertTrue(any("Editor-only metaitems" in warning for warning in warnings))

    def test_backup_and_missing_metadata_preserve_existing_files(self):
        for name in ("brushes.json", "tilesets.json"):
            (self.target / name).write_bytes(b'{"old":true}\n')
        (self.target / "items.xml").write_bytes(b'<items original="yes"/>')
        (self.target / "creatures.xml").write_bytes(b'<creatures original="yes"/>')
        report = profile_import.import_profile(self.source, self.target)
        backup = Path(report["backup"])
        for name in ("brushes.json", "tilesets.json"):
            self.assertEqual((backup / name).read_bytes(), b'{"old":true}\n')
        self.assertEqual((self.target / "items.xml").read_bytes(), b'<items original="yes"/>')
        self.assertEqual((self.target / "creatures.xml").read_bytes(), b'<creatures original="yes"/>')

    def test_dry_run_writes_nothing(self):
        report = profile_import.import_profile(self.source, self.target, dry_run=True)
        self.assertTrue(report["dry_run"])
        self.assertEqual(list(self.target.iterdir()), [])

    def test_explicit_server_metadata_replaces_and_backs_up_item_names(self):
        (self.target / "items.xml").write_bytes(b'<items original="yes"/>')
        server_metadata = Path(self.temporary.name) / "server-items.xml"
        server_metadata.write_bytes(b'<items><item id="100" name="custom grass"/></items>\x00')
        report = profile_import.import_profile(self.source, self.target, items_xml=server_metadata)
        self.assertEqual((self.target / "items.xml").read_bytes(), b'<items><item id="100" name="custom grass"/></items>')
        self.assertEqual((Path(report["backup"]) / "items.xml").read_bytes(), b'<items original="yes"/>')

    def test_malformed_metadata_fails_before_replacing_anything(self):
        (self.target / "brushes.json").write_bytes(b'{"old":true}')
        (self.source / "items.xml").write_text('<items>', encoding="utf-8")
        with self.assertRaises(ET.ParseError):
            profile_import.import_profile(self.source, self.target)
        self.assertEqual((self.target / "brushes.json").read_bytes(), b'{"old":true}')
        self.assertEqual(len(list(self.target.iterdir())), 1)

    def test_later_write_failure_restores_prior_files(self):
        for name in ("brushes.json", "tilesets.json"):
            (self.target / name).write_bytes(b'{"old":true}')
        write = profile_import.rme.write_bytes_atomic
        def fail_second(path, contents):
            if path.name == "tilesets.json":
                raise OSError("Simulated second-file write failure")
            write(path, contents)
        with patch.object(profile_import.rme, "write_bytes_atomic", side_effect=fail_second):
            with self.assertRaises(OSError):
                profile_import.import_profile(self.source, self.target)
        for name in ("brushes.json", "tilesets.json"):
            self.assertEqual((self.target / name).read_bytes(), b'{"old":true}')


if __name__ == "__main__":
    unittest.main()
