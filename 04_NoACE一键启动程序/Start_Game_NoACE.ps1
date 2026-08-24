[CmdletBinding()]
param(
    [string]$GameExe = ''
)

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
$OriginalHash = '4E63733C20EA413131D790CEEAB1E2B1A6E71DC760F622E907C6284043969B11'
$PatchHash = 'FCB031C8EC49BDA023E2AA98461D219DDBC3B5DA6B18B29B1FD98165373D3544'
$OptionalOriginalHash = '62CD48BE79B9A8EE0F59F741CFB4B96B726C42EBE127E5D4B1F2B3B41723CD6C'
$OptionalPatchHash = '7247ED327DBE2D131822C9A088671A84AE3B26B9C6EAC30E08BF438E842A1B9F'

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
function Find-CurrentBundle([string]$Root, [string]$PreferredFolder, [string]$Pattern) {
    $preferred = Join-Path $Root $PreferredFolder
    $items = @()
    if (Test-Path -LiteralPath $preferred) {
        $items = @(Get-ChildItem -LiteralPath $preferred -Recurse -File -Filter $Pattern)
    }
    if ($items.Count -eq 0) {
        $items = @(Get-ChildItem -LiteralPath $Root -Recurse -File -Filter $Pattern)
    }
    if ($items.Count -ne 1) {
        $sample = ($items | Select-Object -ExpandProperty FullName) -join '; '
        throw "Expected one current bundle $Pattern under $Root, found $($items.Count). Candidates: $sample"
    }
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
        throw "Bundle version mismatch for $Target : $currentHash. This package expects $ExpectedOriginalHash or the already patched hash $ExpectedPatchHash. Rebuild the payload for the installed game version."
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
if (-not $GameExe -and $env:PRETERNATURAL_GAME_EXE) { $GameExe = $env:PRETERNATURAL_GAME_EXE }
if ($GameExe) {
    if (-not (Test-Path -LiteralPath $GameExe)) { throw "Game executable was not found: $GameExe" }
    $gameRoot = Split-Path -Parent (Resolve-Path -LiteralPath $GameExe)
}
if (-not $gameRoot) { $gameRoot = 'C:\Program Files (x86)\preternatural' }
if (-not (Test-Path -LiteralPath (Join-Path $gameRoot 'UnityPlayer.dll'))) {
    throw "UnityPlayer.dll was not found under $gameRoot. Set PRETERNATURAL_GAME_ROOT."
}
$codeRoot = Join-Path $env:USERPROFILE 'AppData\LocalLow\pi'
$preferredCache = Split-Path -Leaf $gameRoot
$target = Find-CurrentBundle $codeRoot $preferredCache 'updatescript_500.dll.ab_u_*'
$optionalPlainTarget = Find-CurrentBundle $codeRoot $preferredCache 'il2cppscripts_0.dll.ab'
$optionalHashedTarget = Find-CurrentBundle $codeRoot $preferredCache 'il2cppscripts_0.dll.ab_u_*'
Log "Using gameRoot=$gameRoot cache=$preferredCache"
Log "Bundle targets: forcequit=$target optional=$optionalPlainTarget optional_hashed=$optionalHashedTarget"

# Some clients retain a stale hashed copy from a previous resource revision.
# Only replace it when it is the same revision as the plain Bundle; otherwise
# leave it untouched and continue with the verified current file.
$optionalHashedHash = Hash $optionalHashedTarget
if ($optionalHashedHash -ne $OptionalOriginalHash -and $optionalHashedHash -ne $OptionalPatchHash) {
    Log "Skipping stale optional hashed bundle: $optionalHashedTarget hash=$optionalHashedHash"
    $optionalHashedTarget = $null
}

$running = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -match 'NoAceUnityLauncher|UnityCrashHandler64' -or
    $_.Path -like ($gameRoot + '\*')
}
if ($running) { throw "Close the running game before starting this package. PID: $($running.Id -join ', ')" }

Install-CheckedPatch $target $Patch $OriginalHash $PatchHash ('updatescript_500_original_' + $OriginalHash + '.bak')
$optionalPlainHash = Hash $optionalPlainTarget
if ($optionalPlainHash -eq $OptionalOriginalHash -or $optionalPlainHash -eq $OptionalPatchHash) {
    Install-CheckedPatch $optionalPlainTarget $OptionalPatch $OptionalOriginalHash $OptionalPatchHash ('il2cppscripts_0_res795_original_' + $OptionalOriginalHash + '.bak')
} else {
    Log "Skipping optional-update patch for unrecognized cache revision: $optionalPlainTarget hash=$optionalPlainHash"
}
if ($optionalHashedTarget) {
    Install-CheckedPatch $optionalHashedTarget $OptionalPatch $OptionalOriginalHash $OptionalPatchHash ('il2cppscripts_0_res795_original_' + $OptionalOriginalHash + '.bak')
}

$dataDirs = @(Get-ChildItem -LiteralPath $gameRoot -Directory -Filter '*_Data')
if ($dataDirs.Count -lt 1) { throw "No Unity *_Data directory found under $gameRoot" }
$launcherDataAlias = Join-Path (Split-Path -Parent $Launcher) 'NoAceUnityLauncher_Data'
$realDataDir = $dataDirs[0].FullName
if (Test-Path -LiteralPath $launcherDataAlias) {
    $aliasItem = Get-Item -LiteralPath $launcherDataAlias -Force
    if ($aliasItem.LinkType -ne 'Junction' -and $aliasItem.LinkType -ne 'SymbolicLink') {
        throw "Launcher data alias exists but is not a directory link: $launcherDataAlias"
    }
    $aliasTarget = [string]$aliasItem.Target
    $resolvedReal = (Resolve-Path -LiteralPath $realDataDir).Path
    if ($aliasTarget.TrimEnd('\') -ine $resolvedReal.TrimEnd('\')) {
        throw "Launcher data alias points to $aliasTarget instead of $resolvedReal"
    }
} else {
    New-Item -ItemType Junction -Path $launcherDataAlias -Target $realDataDir | Out-Null
    Log "Created launcher data alias: $launcherDataAlias -> $realDataDir"
}
Log "Starting game root=$gameRoot data=$($dataDirs[0].FullName)"
Start-Process -FilePath $Launcher -WorkingDirectory $gameRoot
Start-Sleep -Seconds 5
$process = Get-Process -Name 'NoAceUnityLauncher' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $process) { throw 'Launcher exited during the first 5 seconds. Check logs.' }
Log "Started PID=$($process.Id) bundle_hash=$(Hash $target)"
