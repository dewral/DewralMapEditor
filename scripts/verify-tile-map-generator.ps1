param(
    [string] $GeneratorDirectory = 'C:\github2\TileMapGenerator',
    [string] $BuildDirectory = 'build/codex',
    [string] $QtSqlDirectory = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRepository = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $taskRepository
$taskFixture = Join-Path $taskRepository 'build/tilemapgen-fixture'
$taskTemp = Join-Path $taskRepository 'build/tilemapgen-temp'
New-Item -ItemType Directory -Path $taskFixture, $taskTemp -Force | Out-Null
$env:TEMP = $taskTemp
$env:TMP = $taskTemp
$env:MSBUILDDISABLENODEREUSE = '1'
$env:QT_FORCE_STDERR_LOGGING = '1'
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PYTHONPATH = Join-Path $GeneratorDirectory 'src'
$taskPython = Join-Path $GeneratorDirectory '.venv/Scripts/python.exe'
Write-Output 'Preparing the cross-language TileMapGen fixture'
$taskExistingFixture = Join-Path $GeneratorDirectory '.cache/dewral-fixture'
if ((Test-Path -LiteralPath "$taskExistingFixture/catalog.json") -and (Test-Path -LiteralPath "$taskExistingFixture/map.tmg")) {
    Copy-Item -LiteralPath "$taskExistingFixture/catalog.json" -Destination "$taskFixture/catalog.json" -Force
    Copy-Item -LiteralPath "$taskExistingFixture/map.tmg" -Destination "$taskFixture/map.tmg" -Force
} else {
    & $taskPython -u (Join-Path $GeneratorDirectory 'scripts/make_dewral_fixture.py') $taskFixture
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& $taskPython -c 'import sys; from pathlib import Path; from tilemapgen.models import AssetPack; from tilemapgen.storage import save_pack; p=Path(sys.argv[1]); save_pack(p/"materials.tmgpack", AssetPack.model_validate_json((p/"catalog.json").read_text("utf-8")))' $taskFixture
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$taskQt = Join-Path $taskRepository 'build/vcpkg-windows/vcpkg_installed/x64-windows-release'
$env:PATH = "$taskQt\bin;$env:PATH"
$env:QT_PLUGIN_PATH = Join-Path $taskQt 'Qt6/plugins'
$env:QML_IMPORT_PATH = Join-Path $taskQt 'Qt6/qml'
$taskConfigureArguments = @('-S', '.', '-B', $BuildDirectory, "-DTILEMAPGEN_FIXTURE_DIR=$taskFixture", "-DTILEMAPGEN_DIRECTORY=$GeneratorDirectory")
if ($QtSqlDirectory) { $taskConfigureArguments += "-DQt6Sql_DIR=$QtSqlDirectory" }
Write-Output 'Configuring DME'
& cmake @taskConfigureArguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output 'Building DME and the native integration tests'
& cmake --build $BuildDirectory --config Release --target DewralMapEditor tmg_import_test tilemapgen_process_test tilemapgen_settings_test tilemapgen_materials_test map_creature_refresh_test lua_script_engine_test --parallel 1 -- /nr:false
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output 'Running the native integration tests'
& ctest --test-dir $BuildDirectory -C Release -R '^(tilemapgen_.*|dme_startup|map_creature_refresh|lua_script_engine)$' --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$env:QT_QUICK_CONTROLS_STYLE = 'Basic'
$env:QT_QUICK_BACKEND = 'software'
& "$taskQt\tools\Qt6\bin\qmltestrunner.exe" -platform windows -input tests/qml/tst_tile_map_generator.qml -import tests/qml/imports -o -,txt
exit $LASTEXITCODE
