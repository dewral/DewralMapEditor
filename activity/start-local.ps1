$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
try {
    $taskHealth = Invoke-RestMethod 'http://127.0.0.1:3000/api/health' -TimeoutSec 2
    if ($taskHealth.service -eq 'dme-activity') { Write-Output 'DME Activity is already running.'; exit 0 }
    throw 'Port 3000 belongs to another service.'
} catch {
    if ($_.Exception.Message -eq 'Port 3000 belongs to another service.') { throw }
}
if (Get-NetTCPConnection -LocalPort 3000 -State Listen -ErrorAction SilentlyContinue) { throw 'Port 3000 is occupied. DME Activity could not verify the running service.' }
if (!(Test-Path -LiteralPath (Join-Path $taskRoot 'dist/server.js'))) { throw 'Run npm ci and npm run build in activity first.' }
$taskProcess = Start-Process -FilePath (Get-Command node.exe).Source -ArgumentList 'start-local.mjs' -WorkingDirectory $taskRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $taskRoot '.local-server.log') -RedirectStandardError (Join-Path $taskRoot '.local-server-error.log')
$taskProcess.Id | Set-Content -LiteralPath (Join-Path $taskRoot '.local-server.pid')
$taskReady = $false
for ($taskAttempt = 0; $taskAttempt -lt 60; $taskAttempt++) {
    if ($taskProcess.HasExited) { throw 'DME Activity exited. Check activity/.local-server-error.log.' }
    try {
        $taskHealth = Invoke-RestMethod 'http://127.0.0.1:3000/api/health' -TimeoutSec 1
        if ($taskHealth.service -eq 'dme-activity') { $taskReady = $true; break }
    } catch { }
    Start-Sleep -Milliseconds 250
}
if (!$taskReady) { throw 'DME Activity did not become ready. Check activity/.local-server-error.log.' }
Write-Output 'DME Activity started. Open http://localhost/dme-activity/'
