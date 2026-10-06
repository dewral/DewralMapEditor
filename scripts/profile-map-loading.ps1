param(
    [Parameter(Mandatory)][string]$DmePath,
    [Parameter(Mandatory)][string]$MapPath,
    [Parameter(Mandatory)][string]$ClientFolder,
    [Parameter(Mandatory)][int]$ClientVersion,
    [ValidateRange(1, 20)][int]$Runs = 5,
    [switch]$Warm,
    [string]$OutputDirectory = "$PSScriptRoot/../build/load-profile-results"
)
$ErrorActionPreference = 'Stop'
$profileExecutable = (Resolve-Path -LiteralPath $DmePath).Path
$profileMap = (Resolve-Path -LiteralPath $MapPath).Path
$profileClient = (Resolve-Path -LiteralPath $ClientFolder).Path
$null = New-Item -ItemType Directory -Path $OutputDirectory -Force
$profileOutput = (Resolve-Path -LiteralPath $OutputDirectory).Path
$profileResults = [System.Collections.Generic.List[object]]::new()
$processRuns = if ($Warm) { 1 } else { $Runs }
for ($run = 1; $run -le $processRuns; $run++) {
    $logPath = Join-Path $profileOutput "load-$run.log"
    $arguments = '--load-profile --profile-map "{0}" --profile-client "{1}" --profile-version {2} --profile-exit' -f $profileMap, $profileClient, $ClientVersion
    if ($Warm) { $arguments += " --profile-repeat $($Runs + 1)" }
    $process = Start-Process -FilePath $profileExecutable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $logPath
    $watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        while (-not $process.WaitForExit(1000)) {
            if ($watch.Elapsed.TotalSeconds -gt 60) {
                $process.Kill()
                throw "Profiling timed out; inspect $logPath"
            }
        }
        if ($process.ExitCode -ne 0) { throw "DME exited with $($process.ExitCode); inspect $logPath" }
    } finally { $process.Dispose() }
    $loadStart = 0L
    $loadNumber = 0
    foreach ($line in Get-Content -LiteralPath $logPath) {
        if ($line -notmatch 'DME_LOAD elapsed_ms=(\d+) duration_ms=(-?\d+) peak_ram_bytes=(\d+) stage=([^ ]+)') { continue }
        $elapsed = [long]$Matches[1]
        $peak = [long]$Matches[3]
        $stage = $Matches[4]
        if ($stage -eq 'repeat_map_begin') { $loadStart = $elapsed }
        if ($stage -eq 'first_complete_frame') {
            $loadNumber++
            if (-not $Warm -or $loadNumber -gt 1) {
                $profileResults.Add([pscustomobject]@{
                    Run = $profileResults.Count + 1
                    ReadyMs = $elapsed - $loadStart
                    PeakRamBytes = $peak
                    Log = $logPath
                })
            }
        }
    }
    Write-Host "Finished process $run/$processRuns"
}
if ($profileResults.Count -ne $Runs) { throw "Expected $Runs complete views, measured $($profileResults.Count)" }
$profileResults | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $profileOutput 'results.json')
$profileResults | Format-Table -AutoSize
$sortedTimes = @($profileResults.ReadyMs | Sort-Object)
$middle = [int][Math]::Floor($sortedTimes.Count / 2)
$median = if ($sortedTimes.Count % 2) { $sortedTimes[$middle] } else { ($sortedTimes[$middle - 1] + $sortedTimes[$middle]) / 2 }
Write-Host "Median: $median ms ($([Math]::Round($median / 1000, 3)) s)"
