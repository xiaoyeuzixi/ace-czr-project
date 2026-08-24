[CmdletBinding()]
param(
    [switch]$Execute
)

$ErrorActionPreference = 'Stop'

$GameRoot = 'C:\Program Files (x86)\preternatural'
$AceSetup = Join-Path $GameRoot 'AntiCheatExpert\ACE-Setup64.exe'
$GameExe = Get-ChildItem -LiteralPath $GameRoot -Filter '*.exe' -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Length -gt 500KB -and $_.Name -notmatch '^(UnityCrashHandler|uninst|ACE-)' } |
    Select-Object -First 1 -ExpandProperty FullName
$ManagedServiceNames = @('AntiCheatExpert Protection', 'ACE-BASE', 'ACE-GAME')
$AuxiliaryServiceNames = @('ACE-ADVT', 'ace-game-0', 'AntiCheatExpert Service', 'ACE-SSC-DRV64')

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Resolve-ServiceBinaryPath {
    param([string]$Path)

    if (-not $Path) {
        return ''
    }

    $expanded = [Environment]::ExpandEnvironmentVariables($Path.Trim())
    if ($expanded -match '^"([^"]+)"') {
        $expanded = $Matches[1]
    } elseif ($expanded -match '^(.*?\.(?:exe|sys|dll))(?:\s|$)') {
        $expanded = $Matches[1]
    }
    if ($expanded -match '^\\SystemRoot\\(.+)$') {
        $expanded = Join-Path $env:SystemRoot $Matches[1]
    }
    return $expanded
}

function Get-ServiceSnapshot {
    param(
        [string[]]$Names,
        [string]$Scope
    )

    foreach ($name in $Names) {
        $query = (& sc.exe query $name 2>&1) -join "`n"
        $present = $LASTEXITCODE -eq 0
        $config = if ($present) { ((& sc.exe qc $name 2>&1) -join "`n") } else { '' }
        $state = if ($query -match 'STATE\s+:\s+\d+\s+(\S+)') { $Matches[1] } else { '' }
        $startMode = if ($config -match 'START_TYPE\s+:\s+\d+\s+(\S+)') { $Matches[1] } else { '' }
        $path = if ($config -match 'BINARY_PATH_NAME\s+:\s*(.+)') { $Matches[1].Trim() } else { '' }
        $resolvedPath = Resolve-ServiceBinaryPath -Path $path
        [PSCustomObject]@{
            Name = $name
            Scope = $Scope
            Present = $present
            State = $state
            StartMode = $startMode
            Path = $path
            FilePresent = if ($resolvedPath) { Test-Path -LiteralPath $resolvedPath } else { $false }
        }
    }
}

if (-not $GameExe -or -not (Test-Path -LiteralPath $GameExe)) {
    throw "Game executable was not found under: $GameRoot"
}
if (-not (Test-Path -LiteralPath $AceSetup)) {
    throw "Bundled ACE uninstaller not found: $AceSetup"
}

Write-Host 'Current ACE registration:'
$beforeManaged = @(Get-ServiceSnapshot -Names $ManagedServiceNames -Scope 'This game/core')
$beforeAuxiliary = @(Get-ServiceSnapshot -Names $AuxiliaryServiceNames -Scope 'Shared/unknown')
@($beforeManaged) + @($beforeAuxiliary) | Format-Table -AutoSize

if ($beforeAuxiliary | Where-Object Present) {
    Write-Warning 'Shared/unknown ACE registrations were found. They are audited only and are not deleted by this script.'
}

$gameProcessName = [IO.Path]::GetFileNameWithoutExtension($GameExe)
$protectedProcessNames = @($gameProcessName, 'ACE-Service64', 'ACE-Helper')
$running = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $protectedProcessNames -contains $_.ProcessName
}
if ($running) {
    $running | Select-Object ProcessName, Id, Path | Format-Table -AutoSize
    throw 'Close the game and ACE processes before uninstalling.'
}

if (-not $Execute) {
    Write-Host ''
    Write-Host 'Audit only. Run this script from an elevated PowerShell with -Execute to invoke the bundled official uninstaller:'
    Write-Host "  & '$PSCommandPath' -Execute"
    exit 0
}

if (-not (Test-Administrator)) {
    throw 'The official ACE removal requires an elevated PowerShell session.'
}

Write-Host "Running: $AceSetup -q"
$process = Start-Process -FilePath $AceSetup -ArgumentList '-q' -Wait -PassThru
Write-Host "ACE uninstaller exit code: $($process.ExitCode)"

Start-Sleep -Seconds 2
Write-Host 'ACE registration after uninstall:'
$after = @(Get-ServiceSnapshot -Names $ManagedServiceNames -Scope 'This game/core')
$after | Format-Table -AutoSize

if ($after | Where-Object Present) {
    Write-Warning 'One or more ACE registrations remain. Reboot once, then rerun this script without -Execute to audit again.'
} else {
    Write-Host 'ACE service and driver registrations are absent. Reboot before testing a vendor-supported no-ACE build.'
}

Write-Host 'Shared/unknown ACE registrations (audit only):'
Get-ServiceSnapshot -Names $AuxiliaryServiceNames -Scope 'Shared/unknown' | Format-Table -AutoSize

Write-Host ''
Write-Warning 'The retail game executable still imports its TenProtect/TPShell Base DLL at process load time. This script does not patch that dependency.'
