param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$QtPrefix,
    [string]$OutputDirectory = "$PSScriptRoot/../../../build/architecture-audit"
)
# Diagnostyka dla istniejącego Release MinGW/Windows. Nie konfiguruje aplikacji.
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path "$PSScriptRoot/../../..").Path
$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$qtRoot = (Resolve-Path -LiteralPath $QtPrefix).Path
$cache = Get-Content -LiteralPath (Join-Path $buildRoot 'CMakeCache.txt')
$compilerLine = $cache | Where-Object { $_ -match '^CMAKE_CXX_COMPILER:(FILEPATH|STRING)=' } | Select-Object -First 1
if (-not $compilerLine) { throw 'Nie znaleziono kompilatora w cache.' }
$compiler = ($compilerLine -split '=', 2)[1]
if (-not (Test-Path -LiteralPath (Join-Path $buildRoot 'libOTFormats.a'))) {
    throw 'Wymagany istniejący build MinGW z libOTFormats.a. Najpierw zbuduj targety wskazane w raporcie.'
}
if (-not ($cache -match '^CMAKE_BUILD_TYPE:STRING=Release$')) { throw 'Ten harness był weryfikowany dla single-config Release.' }
$qtCached = ($cache | Where-Object { $_ -match '^Qt6_DIR:PATH=' } | Select-Object -First 1) -split '=',2
$expectedQt = (Resolve-Path (Join-Path $qtRoot 'lib/cmake/Qt6')).Path
if (-not $qtCached -or (Resolve-Path $qtCached[1]).Path -ne $expectedQt) { throw 'QtPrefix nie odpowiada SDK w cache.' }
$null = New-Item -ItemType Directory -Path $OutputDirectory -Force
$outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
$env:PATH = "$(Join-Path $qtRoot 'bin');$(Split-Path $compiler);$env:PATH"
$env:QT_FORCE_STDERR_LOGGING = '1'
$includeArgs = @('-I', (Join-Path $sourceRoot 'editor/map'), '-I', (Join-Path $sourceRoot 'libs/otformats'))
$includeArgs += @('-I', (Join-Path $qtRoot 'include'))
foreach ($module in @('QtCore','QtGui','QtQml','QtQuick','QtOpenGL')) {
    $includeArgs += @('-I', (Join-Path $qtRoot "include/$module"))
}
foreach ($module in @('QtCore','QtGui')) {
    $privateVersions = @(Get-ChildItem -LiteralPath (Join-Path $qtRoot "include/$module") -Directory | Where-Object { $_.Name -match '^6\.\d+\.\d+$' })
    if ($privateVersions.Count -ne 1) { throw "Nie można jednoznacznie dobrać private headers $module." }
    $includeArgs += @('-I',$privateVersions[0].FullName,'-I',(Join-Path $privateVersions[0].FullName $module))
}
function Compile-Audit([string]$Name, [string[]]$Extra) {
    $arguments = @('-std=c++17','-O3','-DNDEBUG','-Wno-deprecated-declarations') + $includeArgs
    $arguments += @((Join-Path $PSScriptRoot "$Name.cpp"),'-o',(Join-Path $outputRoot "$Name.exe")) + $Extra
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) { throw "Kompilacja ${Name}: $LASTEXITCODE" }
}
Compile-Audit 'index-benchmark' @()
$qtLibraries = @('-L',(Join-Path $qtRoot 'lib'),'-lQt6Gui','-lQt6Sql','-lQt6Qml','-lQt6Network','-lQt6Core')
Compile-Audit 'map-benchmark' (@((Join-Path $buildRoot 'libOTFormats.a'),(Join-Path $buildRoot 'libDmeLzma.a')) + $qtLibraries)
$rhiObjectRoot = Join-Path $buildRoot 'CMakeFiles/map_rhi_backend_test.dir'
$backend = @(Get-ChildItem -LiteralPath $rhiObjectRoot -Recurse -File -Filter 'maprhibackend.cpp.obj')
$shaders = @(Get-ChildItem -LiteralPath $rhiObjectRoot -Recurse -File -Filter 'qrc_test_map_shaders.cpp.obj')
if ($backend.Count -ne 1 -or $shaders.Count -ne 1) { throw 'Najpierw zbuduj map_rhi_backend_test; wymagane jednoznaczne obiekty backend/shaders.' }
Compile-Audit 'rhi-benchmark' (@($backend[0].FullName,$shaders[0].FullName,'-L',(Join-Path $qtRoot 'lib'),'-lQt6Quick','-lQt6OpenGL','-lQt6Gui','-lQt6Qml','-lQt6Network','-lQt6Core','-ld3d11','-ldxgi','-ldxguid','-ld3d12'))
& (Join-Path $outputRoot 'index-benchmark.exe') > (Join-Path $outputRoot 'index-results.csv')
if ($LASTEXITCODE -ne 0) { throw 'Benchmark indeksu nie przeszedł walidacji.' }
& (Join-Path $outputRoot 'map-benchmark.exe') > (Join-Path $outputRoot 'map-results.csv')
if ($LASTEXITCODE -ne 0) { throw 'Benchmark mapy nie przeszedł round-trip/undo.' }
& (Join-Path $outputRoot 'map-benchmark.exe') --bounds > (Join-Path $outputRoot 'bounds-repro.txt')
if ($LASTEXITCODE -ne 0) { throw 'Reprodukcja granic nie wykonała się.' }
foreach ($api in @('opengl','d3d11')) {
    foreach ($trial in 1..3) {
        & (Join-Path $outputRoot 'rhi-benchmark.exe') $api > (Join-Path $outputRoot "rhi-$api-$trial.csv") 2> (Join-Path $outputRoot "rhi-$api-$trial.log")
        if ($LASTEXITCODE -ne 0) { throw "Benchmark QRhi $api nie przeszedł walidacji pikseli." }
    }
}
Write-Host "Gotowe. Surowe wyniki: $outputRoot"
