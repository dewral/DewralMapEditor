param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$QtPrefix,
    [string]$OutputDirectory = "$PSScriptRoot/../../../build/architecture-audit",
    [switch]$SelectionOnly
)
# Windows/MinGW Release, matching the existing test build; no production edits.
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$qtRoot = (Resolve-Path -LiteralPath $QtPrefix).Path
$cache = Get-Content -LiteralPath (Join-Path $buildRoot 'CMakeCache.txt')
$compilerLine = $cache | Where-Object { $_ -match '^CMAKE_CXX_COMPILER:(FILEPATH|STRING)=' } | Select-Object -First 1
if (-not $compilerLine -or -not ($cache -match '^CMAKE_BUILD_TYPE:STRING=Release$')) { throw 'Wymagany istniejący MinGW Release.' }
$compiler = ($compilerLine -split '=',2)[1]
$qtCached = ($cache | Where-Object { $_ -match '^Qt6_DIR:PATH=' } | Select-Object -First 1) -split '=',2
if (-not $qtCached -or (Resolve-Path $qtCached[1]).Path -ne (Resolve-Path (Join-Path $qtRoot 'lib/cmake/Qt6')).Path) { throw 'SDK nie odpowiada cache.' }
$null = New-Item -ItemType Directory -Path $OutputDirectory -Force
$outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
$env:PATH = "$(Join-Path $qtRoot 'bin');$(Split-Path $compiler);$env:PATH"
$env:QT_FORCE_STDERR_LOGGING = '1'
$includes = @('-I',(Join-Path $sourceRoot 'editor/core'),'-I',(Join-Path $sourceRoot 'editor/map'),'-I',(Join-Path $sourceRoot 'libs/otformats'),'-I',(Join-Path $qtRoot 'include'))
foreach ($module in @('QtCore','QtGui','QtQml','QtQuick','QtConcurrent','QtOpenGL')) {
    $includes += @('-I',(Join-Path $qtRoot "include/$module"))
}
foreach ($module in @('QtCore','QtGui')) {
    $versions = @(Get-ChildItem -LiteralPath (Join-Path $qtRoot "include/$module") -Directory | Where-Object { $_.Name -match '^6\.\d+\.\d+$' })
    if ($versions.Count -ne 1) { throw "Niejednoznaczne private headers $module." }
    $includes += @('-I',$versions[0].FullName,'-I',(Join-Path $versions[0].FullName $module))
}
$formatLibs = @((Join-Path $buildRoot 'libOTFormats.a'),(Join-Path $buildRoot 'libDmeLuaScripts.a'),(Join-Path $buildRoot 'libDmeLzma.a'),(Join-Path $buildRoot 'libDmeLua.a'))
$qtLibs = @('-L',(Join-Path $qtRoot 'lib'),'-lQt6Quick','-lQt6Concurrent','-lQt6Sql','-lQt6OpenGL','-lQt6Qml','-lQt6Network','-lQt6Gui','-lQt6Core','-ld3d11','-ldxgi','-ldxguid','-ld3d12')
$recoveryObjects = @(
    (Join-Path $buildRoot 'CMakeFiles/document_recovery_test.dir/editor/core/documentmanager.cpp.obj'),
    (Join-Path $buildRoot 'CMakeFiles/document_recovery_test.dir/document_recovery_test_autogen/mocs_compilation.cpp.obj')
)
$selectionObjects = @(Get-ChildItem -LiteralPath (Join-Path $buildRoot 'CMakeFiles/map_creature_refresh_test.dir') -Recurse -Filter '*.obj' -File | Where-Object { $_.Name -ne 'map_creature_refresh_test.cpp.obj' } | ForEach-Object { $_.FullName })
if ($selectionObjects.Count -lt 10) { throw 'Najpierw zbuduj map_creature_refresh_test i document_recovery_test.' }
$harnesses = if ($SelectionOnly) { @('selection-continuation') } else { @('io-continuation','selection-continuation') }
foreach ($name in $harnesses) {
    $objects = if ($name -eq 'io-continuation') { $recoveryObjects } else { $selectionObjects }
    $compileArgs = @('-std=c++17','-O3','-DNDEBUG','-Wno-deprecated-declarations') + $includes
    $compileArgs += @((Join-Path $PSScriptRoot "$name.cpp")) + $objects + $formatLibs + $qtLibs
    $compileArgs += @('-o',(Join-Path $outputRoot "$name.exe"))
    & $compiler @compileArgs
    if ($LASTEXITCODE -ne 0) { throw "Kompilacja $name nie powiodła się." }
}
if (-not $SelectionOnly) {
    & (Join-Path $outputRoot 'io-continuation.exe') > (Join-Path $outputRoot 'io-continuation-results.json')
    if ($LASTEXITCODE -ne 0) { throw 'Nie odtworzono wszystkich opisanych przypadków IO.' }
}
$previousPlatform = $env:QT_QPA_PLATFORM
try {
    $env:QT_QPA_PLATFORM = 'minimal'
    & (Join-Path $outputRoot 'selection-continuation.exe') > (Join-Path $outputRoot 'selection-continuation-results.csv')
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark zaznaczania nie przeszedł kontroli wyniku.' }
} finally { $env:QT_QPA_PLATFORM = $previousPlatform }
Write-Host "Wyniki: $outputRoot"
