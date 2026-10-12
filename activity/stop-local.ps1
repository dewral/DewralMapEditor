$ErrorActionPreference = 'Stop'
$taskSettings = Get-Content -LiteralPath (Join-Path $PSScriptRoot '.local-settings.json') -Raw | ConvertFrom-Json
$taskBody = @{email=$taskSettings.adminEmail; password=$taskSettings.adminPassword} | ConvertTo-Json
$null = Invoke-RestMethod 'http://127.0.0.1:3000/api/login' -Method Post -ContentType 'application/json' -Body $taskBody -SessionVariable taskSession
$null = Invoke-RestMethod 'http://127.0.0.1:3000/api/local/stop' -Method Post -ContentType 'application/json' -Body '{}' -WebSession $taskSession
Write-Output 'DME Activity stopped and its database closed.'
