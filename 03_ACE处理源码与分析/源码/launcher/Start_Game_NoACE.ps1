[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$PackageRoot = $PSScriptRoot
$Launcher = Join-Path $PackageRoot 'bin\NoAceUnityLauncher.exe'
$Patch = Join-Path $PackageRoot 'payload\updatescript_500_forcequit_fix.ab'
$OriginalPayload = Join-Path $PackageRoot 'payload\updatescript_500_original.ab'
$OptionalPatch = Join-Path $PackageRoot 'payload\il2cppscripts_0_optional_update_fix.ab'
$OptionalOriginalPayload = Join-Path $PackageRoot 'payload\il2cppscripts_0_res795_original.ab'
$LogDir = Join-Path $PackageRoot 'logs'
$StatusLog = Join-Path $LogDir 'launcher_status.log'
$BundleName = 'updatescript_500.dll.ab_u_4548ac6984db86d9f5a2ad35fbb456b9'
$OptionalPlainName = 'il2cppscripts_0.dll.ab'
$OptionalHashedName = 'il2cppscripts_0.dll.ab_u_a5f32adb0a61c6dea05e89685082c56d'
$OriginalHash = 'D0E7CCEABB57AD5A05C072ED4D9FF3B82D5D9F4364803E13169089BAB21BF6A0'
$PatchHash = 'F25AB3ADABCEC20CEF0EC25E19A6BEFC086C1950F1677D44A80B68C837555666'
$OptionalOriginalHash = 'D9032C445FED9206D8F11238A6721F0F3BEAD9857610A579AED087B9A47A1B55'
$OptionalPatchHash = 'ED9DA6D2B0161A16B3FCCD8578A46032455729969EA73D373F83F5D6CFE2077F'

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
function Log([string]$Message) {
    $line = '[{0}] {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Message
    Add-Content -LiteralPath $StatusLog -Value $line -Encoding UTF8
    Write-Host $line
}
function Hash([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}
function Find-One([string]$Root, [string]$Name) {
    $items = @(Get-ChildItem -LiteralPath $Root -Recurse -File -Filter $Name)
    if ($items.Count -ne 1) { throw "Expected one $Name under $Root, found $($items.Count)." }
    return $items[0].FullName
}
function Install-CheckedPatch(
    [string]$Target,
    [string]$PatchPath,
    [string]$ExpectedOriginalHash,
    [string]$ExpectedPatchHash,
    [string]$BackupName
) {
    $currentHash = Hash $Target
    if ($currentHash -eq $ExpectedPatchHash) {
        Log "Patch already installed: $Target"
        return
    }
    if ($currentHash -ne $ExpectedOriginalHash) {
        throw "Unexpected bundle hash for $Target : $currentHash"
    }
    $backupDir = Join-Path $PackageRoot 'backups'
    New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
    $backup = Join-Path $backupDir $BackupName
    if (-not (Test-Path -LiteralPath $backup)) { Copy-Item -LiteralPath $Target -Destination $backup }
    if ((Hash $backup) -ne $ExpectedOriginalHash) { throw "Backup hash mismatch: $backup" }
    $staged = $Target + '.codex-new'
    Copy-Item -LiteralPath $PatchPath -Destination $staged -Force
    if ((Hash $staged) -ne $ExpectedPatchHash) { throw "Staged patch hash mismatch: $staged" }
    Move-Item -LiteralPath $staged -Destination $Target -Force
    Log "Patched bundle: $Target"
}

if (-not (Test-Path -LiteralPath $Launcher)) { throw "Missing launcher: $Launcher" }
if (-not (Test-Path -LiteralPath $Patch)) { throw "Missing patch payload: $Patch" }
if (-not (Test-Path -LiteralPath $OptionalPatch)) { throw "Missing patch payload: $OptionalPatch" }
if ((Hash $Patch) -ne $PatchHash) { throw 'Patch payload hash mismatch.' }
if ((Hash $OriginalPayload) -ne $OriginalHash) { throw 'Original payload hash mismatch.' }
if ((Hash $OptionalPatch) -ne $OptionalPatchHash) { throw 'Optional update patch payload hash mismatch.' }
if ((Hash $OptionalOriginalPayload) -ne $OptionalOriginalHash) { throw 'Optional update original payload hash mismatch.' }

$gameRoot = $env:PRETERNATURAL_GAME_ROOT
if (-not $gameRoot) { $gameRoot = 'C:\Program Files (x86)\preternatural' }
if (-not (Test-Path -LiteralPath (Join-Path $gameRoot 'UnityPlayer.dll'))) {
    throw "UnityPlayer.dll was not found under $gameRoot. Set PRETERNATURAL_GAME_ROOT."
}
$target = Find-One (Join-Path $env:USERPROFILE 'AppData\LocalLow\pi') $BundleName
$codeRoot = Join-Path $env:USERPROFILE 'AppData\LocalLow\pi'
$optionalPlainTarget = Find-One $codeRoot $OptionalPlainName
$optionalHashedTarget = Find-One $codeRoot $OptionalHashedName

$running = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -match 'NoAceUnityLauncher|UnityCrashHandler64' -or
    $_.Path -like ($gameRoot + '\*')
}
if ($running) { throw "Close the running game before starting this package. PID: $($running.Id -join ', ')" }

Install-CheckedPatch $target $Patch $OriginalHash $PatchHash ('updatescript_500_original_' + $OriginalHash + '.bak')
Install-CheckedPatch $optionalPlainTarget $OptionalPatch $OptionalOriginalHash $OptionalPatchHash ('il2cppscripts_0_res795_original_' + $OptionalOriginalHash + '.bak')
Install-CheckedPatch $optionalHashedTarget $OptionalPatch $OptionalOriginalHash $OptionalPatchHash ('il2cppscripts_0_res795_original_' + $OptionalOriginalHash + '.bak')

$dataDirs = @(Get-ChildItem -LiteralPath $gameRoot -Directory -Filter '*_Data')
if ($dataDirs.Count -lt 1) { throw "No Unity *_Data directory found under $gameRoot" }
Log "Starting game root=$gameRoot data=$($dataDirs[0].FullName)"
Start-Process -FilePath $Launcher -WorkingDirectory $gameRoot
Start-Sleep -Seconds 5
$process = Get-Process -Name 'NoAceUnityLauncher' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $process) { throw 'Launcher exited during the first 5 seconds. Check logs.' }
Log "Started PID=$($process.Id) bundle_hash=$(Hash $target)"
