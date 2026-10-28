"""Link the startup diagnostic with existing Windows/MinGW Release app objects.
Usage: python compile-startup-audit.py BUILD_DIR QT_PREFIX OUTPUT_EXE
Does not build stale objects; see README-startup.md for prerequisites.
"""
from pathlib import Path
import re,subprocess,sys,tempfile,os
root=Path.cwd();build=Path(sys.argv[1]).resolve();qt=Path(sys.argv[2]).resolve();exe=Path(sys.argv[3]).resolve()
s=(build/'build.ninja').read_text();m=re.search(r'^build .*: CXX_EXECUTABLE_LINKER__DewralMapEditor_Release (.+)',s,re.M)
objects=[str(build/p) for p in m[1].split(' | ')[0].split() if not p.endswith('/editor/main.cpp.obj')]
compiler=re.search(r'^CMAKE_CXX_COMPILER:(?:STRING|FILEPATH)=(.+)',(build/'CMakeCache.txt').read_text(),re.M)[1]
args=[compiler,'-std=c++17','-O3','-DNDEBUG','-Wno-deprecated-declarations']
for p in [root/'editor/core',root/'editor/map',root/'editor/preview',root/'libs/otformats',qt/'include']+[qt/'include'/x for x in ['QtCore','QtGui','QtQml','QtQuick','QtConcurrent','QtOpenGL','QtNetwork']]:args+=['-I',str(p)]
for module in ['QtCore','QtGui']:
 versions=[p for p in (qt/'include'/module).iterdir() if p.is_dir() and re.fullmatch(r'6\.\d+\.\d+',p.name)]
 assert len(versions)==1
 args+=['-I',str(versions[0]),'-I',str(versions[0]/module)]
args += [str(root/'docs/architecture/benchmarks/startup-audit.cpp')]+objects
args += [str(build/('lib'+x+'.a')) for x in ['OTFormats','DmeLuaScripts','DmeLzma','DmeLua']]
args += ['-L',str(qt/'lib')]+['-l'+x for x in ['Qt6Quick','Qt6Concurrent','Qt6Sql','Qt6OpenGL','Qt6Qml','Qt6Network','Qt6Gui','Qt6Core','dwmapi','user32','d3d11','dxgi','dxguid','d3d12']]
args+=['-o',str(exe)]
with tempfile.NamedTemporaryFile(mode="w", suffix=".rsp", dir=build, delete=False, encoding="utf-8") as response:
 for argument in args[1:]: response.write('"'+argument.replace('\\','/').replace('"','\\"')+'"\n')
 response_path=response.name
try: result=subprocess.run([args[0], '@'+response_path]).returncode
finally: os.unlink(response_path)
raise SystemExit(result)
