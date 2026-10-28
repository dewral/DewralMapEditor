param(
    [Parameter(Mandatory)][string]$BeforeExe,
    [Parameter(Mandatory)][string]$AfterExe,
    [Parameter(Mandatory)][string]$OutputRoot,
    [Parameter(Mandatory)][string]$ClientFixture,
    [ValidateSet("welcome","map")][string[]]$Scenarios = @("welcome","map")
)
$ErrorActionPreference = 'Stop'
$BeforeExe = (Resolve-Path -LiteralPath $BeforeExe).Path
$AfterExe = (Resolve-Path -LiteralPath $AfterExe).Path
$ClientFixture = (Resolve-Path -LiteralPath $ClientFixture).Path
if (Test-Path -LiteralPath $OutputRoot) { throw 'Use a new output directory for cold cache trials.' }
$taskResults = (New-Item -ItemType Directory -Path $OutputRoot).FullName
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_QUICK_CONTROLS_STYLE = 'Basic'
$env:QT_FORCE_STDERR_LOGGING = '1'
$env:DME_STARTUP_AUDIT_NATIVE = '1'
Remove-Item Env:DME_STARTUP_AUDIT_TOOLS -ErrorAction SilentlyContinue
Remove-Item Env:DME_STARTUP_AUDIT_SCREENSHOT -ErrorAction SilentlyContinue
foreach ($scenario in $Scenarios) {
    if ($scenario -eq 'map') { $env:DME_STARTUP_AUDIT_MAP = Join-Path $ClientFixture 'fixture.otbm'; $client = $ClientFixture }
    else { Remove-Item Env:DME_STARTUP_AUDIT_MAP -ErrorAction SilentlyContinue; $client = '-' }
    foreach ($trial in 1..5) {
        foreach ($variant in @('before','after')) {
            $exe = if ($variant -eq 'before') { $BeforeExe } else { $AfterExe }
            $trialRoot = (New-Item -ItemType Directory -Path (Join-Path $taskResults "$scenario-$variant-$trial")).FullName
            $env:QML_DISK_CACHE_PATH = Join-Path $trialRoot 'cache'
            foreach ($cache in @('cold','warm')) {
                $report = Join-Path $trialRoot "$cache.json"
                $diagnostic = Start-Process -FilePath $exe -ArgumentList @($trialRoot,'tibia-ui','-','1.1.43',$client) -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $report -RedirectStandardError (Join-Path $trialRoot "$cache.log")
                if ($diagnostic.ExitCode -ne 0) { throw "$scenario/$variant/$trial/$cache exited $($diagnostic.ExitCode); inspect $report" }
                Write-Output "$scenario $variant $trial $cache OK"
            }
        }
    }
}
