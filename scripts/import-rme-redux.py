#!/usr/bin/env python3
"""Import all RME Redux material profiles, without copying client binaries."""
from __future__ import annotations
import argparse, importlib.util, json, shutil, tempfile, uuid
from pathlib import Path

spec=importlib.util.spec_from_file_location('profile_import',Path(__file__).with_name('import-rme-profile.py'))
profile_import=importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile_import)
FILES=('brushes.json','tilesets.json','items.xml','creatures.xml')

def import_all(source:Path, output:Path, backup:Path, excluded:list[str], revision:str='') -> dict:
    source=source.resolve();output=output.resolve();backup=backup.resolve()
    data=source/'data'
    excluded_names={name.casefold() for name in excluded}
    profiles=sorted(p for p in data.iterdir() if p.is_dir() and (p/'materials.xml').is_file() and p.name.casefold() not in excluded_names)
    if not profiles:raise ValueError('No material profiles found')
    reports=[]
    # Convert and validate every profile before updating any installed files.
    with tempfile.TemporaryDirectory(prefix='dme-redux-') as temp:
        staged=Path(temp)
        for profile in profiles:
            report=profile_import.import_profile(profile,staged/profile.name)
            reports.append({'profile':profile.name,'sourceProfile':profile.name,'brushes':report['brush_counts'],'tilesets':report['tileset_counts'],'warnings':report['warnings']})
        # Preserve the existing conservative material fallback for these clients.
        if any(p.name=='760' for p in profiles):
            for version in ('772','780','792'):
                if (output/version).is_dir() and not any(p.name==version for p in profiles):
                    shutil.copytree(staged/'760',staged/version)
                    report=next(r for r in reports if r['profile']=='760')
                    reports.append({**report,'profile':version,'sourceProfile':'760'})
        originals={}
        for report in reports:
            name=report['profile']
            for filename in FILES:
                if not (staged/name/filename).is_file():continue
                target=output/name/filename
                originals[target]=target.read_bytes() if target.exists() else None
        for target,contents in originals.items():
            if contents is not None:
                saved=backup/target.relative_to(output)
                saved.parent.mkdir(parents=True,exist_ok=True);saved.write_bytes(contents)
        written=[]
        try:
            for target,contents in originals.items():
                current=target.read_bytes() if target.exists() else None
                if current!=contents:raise RuntimeError(f'Profile changed during import: {target}')
                profile_import.rme.write_bytes_atomic(target,(staged/target.relative_to(output)).read_bytes())
                written.append(target)
        except BaseException:
            for target in written:
                if originals[target] is None:target.unlink()
                else:profile_import.rme.write_bytes_atomic(target,originals[target])
            raise
    manifest={'repository':'https://github.com/Open-Tibia-Tools/remeres-map-editor-redux','revision':revision,'excluded':excluded,'profiles':reports}
    profile_import.rme.write_json_atomic(output/'rme-redux-import.json',manifest)
    return manifest

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rme',type=Path,required=True)
    parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'data')
    parser.add_argument('--exclude',action='append',default=[])
    parser.add_argument('--backup',type=Path,default=Path(__file__).resolve().parents[1]/'build'/'rme-import-backups'/uuid.uuid4().hex[:12])
    parser.add_argument('--revision',default='')
    args=parser.parse_args()
    manifest=import_all(args.rme,args.output,args.backup,args.exclude,args.revision)
    print(f"Imported {len(manifest['profiles'])} profiles. Backup: {args.backup}")
    for profile in manifest['profiles']:print(profile['profile'],profile['brushes'],profile['tilesets'])
    return 0
if __name__=='__main__':raise SystemExit(main())
