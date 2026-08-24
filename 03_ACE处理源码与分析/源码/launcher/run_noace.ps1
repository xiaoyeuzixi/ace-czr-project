[CmdletBinding()]
param(
    [string[]]$GameArgs = @()
)

$ErrorActionPreference = 'Stop'
$Launcher = 'D:\vs\ACE boli\noace_launcher\NoAceUnityLauncher.exe'
$GameRoot = 'C:\Program Files (x86)\preternatural'
$WorkDir = 'D:\vs\ACE boli\noace_launcher'
$PlayerLog = 'C:\Users\Administrator\AppData\LocalLow\pi\超自然行动组\Player.log'
$StubLog = Join-Path $WorkDir 'noace_stub.log'
$BootLog = Join-Path $WorkDir 'launcher_boot.log'
$AceNames = @('AntiCheatExpert Protection','ACE-BASE','ACE-GAME','ACE-ADVT','AntiCheatExpert Service','ACE-SSC-DRV64')

Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -match 'NoAceUnityLauncher|超自然行动组|UnityCrashHandler64|ACE-Service64|ACE-Helper'
} | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 800

Remove-Item -LiteralPath $StubLog -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $BootLog -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $PlayerLog -Force -ErrorAction SilentlyContinue

foreach ($name in $AceNames) {
    $q = (& sc.exe query $name 2>&1) -join "`n"
    if ($LASTEXITCODE -eq 0 -and $q -match 'STATE\s+:\s+\d+\s+RUNNING') {
        Write-Warning "ACE service/driver is RUNNING before launch: $name"
    }
}

if (-not (Test-Path -LiteralPath $Launcher)) { throw "Launcher not found: $Launcher" }
if (-not (Test-Path -LiteralPath $GameRoot)) { throw "Game root not found: $GameRoot" }

if ($GameArgs -and $GameArgs.Count -gt 0) {
    Start-Process -FilePath $Launcher -ArgumentList $GameArgs -WorkingDirectory $GameRoot
} else {
    Start-Process -FilePath $Launcher -WorkingDirectory $GameRoot
}
