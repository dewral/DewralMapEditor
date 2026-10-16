# Run with: powershell -NoProfile -File tests/profile_map_loading_test.ps1
$ErrorActionPreference = 'Stop'
$profileScript = Join-Path $PSScriptRoot '../scripts/profile-map-loading.ps1'
$testOutput = Join-Path $PSScriptRoot '../build/profile-map-loading-test'
$readyLine = 'DME_LOAD elapsed_ms=3000 duration_ms=-1 peak_ram_bytes=100 stage=first_complete_frame'

# Exercise the real log validation without requiring copyrighted client assets
# or deliberately resetting the user's graphics device.
function Start-Process {
    param($FilePath, $ArgumentList, $WindowStyle, [switch]$PassThru, $RedirectStandardError)
    $sampleLog | Set-Content -LiteralPath $RedirectStandardError
    $process = [pscustomobject]@{ ExitCode = 0 }
    $process | Add-Member ScriptMethod WaitForExit { param($timeout) return $true }
    $process | Add-Member ScriptMethod Dispose {}
    return $process
}

$cases = @(
    @{ Name = 'healthy'; Lines = @($readyLine); Fails = $false },
    @{ Name = 'device-loss'; Lines = @('Device loss detected in Present()', $readyLine); Fails = $true },
    @{ Name = 'recovery'; Lines = @('Graphics device lost, cleaning up scenegraph and releasing RHI', $readyLine); Fails = $true },
    @{ Name = 'buffer'; Lines = @($readyLine, 'Failed to create buffer: COM error 0x887a0005'); Fails = $true },
    @{ Name = 'vertex'; Lines = @($readyLine, 'Failed to (re)build vertex/index buffer of size 960'); Fails = $true },
    @{ Name = 'uniform'; Lines = @($readyLine, 'Failed to build uniform buffer of size 512 bytes'); Fails = $true }
)
foreach ($case in $cases) {
    $sampleLog = $case.Lines
    $failure = $null
    try {
        & $profileScript -DmePath $PSCommandPath -MapPath $PSCommandPath `
            -ClientFolder $PSScriptRoot -ClientVersion 1098 -Runs 1 `
            -OutputDirectory (Join-Path $testOutput $case.Name) | Out-Null
    } catch {
        $failure = $_.Exception.Message
    }
    if ($case.Fails) {
        if (-not $failure -or $failure -notlike 'Graphics failure during map loading;*') {
            throw "Expected a graphics failure for $($case.Name), received: $failure"
        }
    } elseif ($failure) {
        throw "Healthy map load failed: $failure"
    }
    Write-Host "PASS: $($case.Name)"
}
