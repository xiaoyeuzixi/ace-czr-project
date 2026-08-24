[CmdletBinding()]
param(
    [string]$GameRoot = 'C:\Program Files (x86)\preternatural',
    [string]$Launcher = 'D:\vs\ACE boli\noace_launcher\NoAceUnityLauncher.exe',
    [int]$DurationSec = 75,
    [int]$IntervalSec = 3,
    [switch]$UseBatchMode
)

$ErrorActionPreference = 'Continue'
$OutRoot = 'D:\vs\ACE boli\dynamic_out'
$Stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunDir = Join-Path $OutRoot "noace_run_$Stamp"
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
$Transcript = Join-Path $RunDir 'collector_transcript.txt'
Start-Transcript -Path $Transcript -Force | Out-Null

function Write-Json($name, $obj) {
    $path = Join-Path $RunDir $name
    $obj | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
}

function Hash-FileSafe($path) {
    try { (Get-FileHash -Algorithm SHA256 -LiteralPath $path -ErrorAction Stop).Hash } catch { $null }
}

function Get-TreeSnapshot($root) {
    if (-not (Test-Path -LiteralPath $root)) { return @() }
    Get-ChildItem -LiteralPath $root -Recurse -Force -File -ErrorAction SilentlyContinue | ForEach-Object {
        [pscustomobject]@{
            FullName = $_.FullName
            Length = $_.Length
            LastWriteTimeUtc = $_.LastWriteTimeUtc.ToString('o')
            SHA256 = Hash-FileSafe $_.FullName
        }
    }
}

function Resolve-ServiceBinaryPath([string]$Path) {
    if (-not $Path) { return '' }
    $expanded = [Environment]::ExpandEnvironmentVariables($Path.Trim())
    if ($expanded -match '^"([^"]+)"') { $expanded = $Matches[1] }
    elseif ($expanded -match '^(.*?\.(?:exe|sys|dll))(?:\s|$)') { $expanded = $Matches[1] }
    if ($expanded -match '^\\SystemRoot\\(.+)$') { $expanded = Join-Path $env:SystemRoot $Matches[1] }
    if ($expanded -match '^\\\?\?\\(.+)$') { $expanded = $Matches[1] }
    return $expanded
}

$ServiceNames = @('AntiCheatExpert Protection','ACE-BASE','ACE-GAME','ACE-ADVT','AntiCheatExpert Service','ACE-SSC-DRV64','ace-game-0')
function Get-ServiceSnapshot($phase) {
    foreach ($name in $ServiceNames) {
        $query = (& sc.exe query $name 2>&1) -join "`n"
        $qcode = $LASTEXITCODE
        $config = if ($qcode -eq 0) { ((& sc.exe qc $name 2>&1) -join "`n") } else { '' }
        $path = if ($config -match 'BINARY_PATH_NAME\s+:\s*(.+)') { $Matches[1].Trim() } else { '' }
        $resolved = Resolve-ServiceBinaryPath $path
        [pscustomobject]@{
            Phase = $phase
            Name = $name
            Present = ($qcode -eq 0)
            State = if ($query -match 'STATE\s+:\s+\d+\s+(\S+)') { $Matches[1] } else { 'ABSENT' }
            StartMode = if ($config -match 'START_TYPE\s+:\s+\d+\s+(\S+)') { $Matches[1] } else { '' }
            Path = $path
            ResolvedPath = $resolved
            FilePresent = if ($resolved) { Test-Path -LiteralPath $resolved } else { $false }
            QueryRaw = $query
            ConfigRaw = $config
        }
    }
}

function Get-InterestingProcesses($phase) {
    Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ExecutablePath -like "$GameRoot*" -or $_.Name -match 'ACE|UnityCrash|NoAce|超自然|preternatural'
    } | ForEach-Object {
        [pscustomObject]@{
            Phase = $phase
            ProcessId = $_.ProcessId
            ParentProcessId = $_.ParentProcessId
            Name = $_.Name
            ExecutablePath = $_.ExecutablePath
            CommandLine = $_.CommandLine
            CreationDate = $_.CreationDate
        }
    }
}

function Get-ProcessModulesSafe($TargetPid, $phase) {
    try {
        $p = Get-Process -Id $TargetPid -ErrorAction Stop
        $p.Modules | ForEach-Object {
            [pscustomobject]@{
                Phase = $phase
                Pid = $TargetPid
                ProcessName = $p.ProcessName
                ModuleName = $_.ModuleName
                FileName = $_.FileName
                BaseAddress = ('0x{0:X}' -f $_.BaseAddress.ToInt64())
                ModuleMemorySize = $_.ModuleMemorySize
            }
        }
    } catch {
        [pscustomobject]@{ Phase=$phase; Pid=$TargetPid; Error=$_.Exception.Message }
    }
}

function Get-NetworkSnapshot($phase) {
    try {
        Get-NetTCPConnection -ErrorAction Stop | Where-Object { $_.OwningProcess -ne 0 } | ForEach-Object {
            $proc = Get-Process -Id $_.OwningProcess -ErrorAction SilentlyContinue
            if ($proc -and ($proc.Path -like "$GameRoot*" -or $proc.ProcessName -match 'ACE|NoAce|超自然|UnityCrash')) {
                [pscustomobject]@{
                    Phase=$phase; OwningProcess=$_.OwningProcess; ProcessName=$proc.ProcessName; Path=$proc.Path
                    LocalAddress=$_.LocalAddress; LocalPort=$_.LocalPort; RemoteAddress=$_.RemoteAddress; RemotePort=$_.RemotePort; State=$_.State
                }
            }
        }
    } catch { @([pscustomobject]@{Phase=$phase;Error=$_.Exception.Message}) }
}

function Get-RecentScmEvents($phase, [datetime]$since) {
    try {
        Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Service Control Manager'; StartTime=$since} -ErrorAction Stop |
            Where-Object { $_.Message -match 'ACE|AntiCheat|超自然|preternatural' } |
            ForEach-Object { [pscustomobject]@{Phase=$phase; TimeCreated=$_.TimeCreated; Id=$_.Id; Provider=$_.ProviderName; Message=$_.Message} }
    } catch { @([pscustomobject]@{Phase=$phase;Error=$_.Exception.Message}) }
}

$StartedAt = Get-Date
$AceInstallRoot = 'C:\Program Files\AntiCheatExpert'
$AceGameDir = Join-Path $GameRoot 'AntiCheatExpert'
Write-Host "RunDir: $RunDir"
Write-Host "GameRoot: $GameRoot"
Write-Host "Launcher: $Launcher"

$pre = [ordered]@{
    StartedAt = $StartedAt.ToString('o')
    ComputerName = $env:COMPUTERNAME
    User = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    IsAdmin = ([Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    GameRootExists = Test-Path -LiteralPath $GameRoot
    LauncherExists = Test-Path -LiteralPath $Launcher
    Services = @(Get-ServiceSnapshot 'pre')
    Processes = @(Get-InterestingProcesses 'pre')
    AceInstallRootTree = @(Get-TreeSnapshot $AceInstallRoot)
    AceGameDirTree = @(Get-TreeSnapshot $AceGameDir)
}
Write-Json 'pre_snapshot.json' $pre

$args = @()
$UnityLog = Join-Path $RunDir 'unity_player.log'
if ($UseBatchMode) { $args += @('-batchmode','-nographics') }
$args += @('-logFile', $UnityLog)

$proc = Start-Process -FilePath $Launcher -ArgumentList $args -WorkingDirectory $GameRoot -PassThru
Write-Host "Started PID=$($proc.Id) Args=$($args -join ' ')"

[object[]]$samples = @()
[object[]]$moduleSamples = @()
[object[]]$netSamples = @()
[object[]]$svcSamples = @()

$end = (Get-Date).AddSeconds($DurationSec)
$i = 0
while ((Get-Date) -lt $end) {
    $phase = ('sample_{0:D3}' -f $i)
    $procs = @(Get-InterestingProcesses $phase)
    foreach($x in $procs){ $samples += $x }
    foreach($s in @(Get-ServiceSnapshot $phase)){ $svcSamples += $s }
    foreach($n in @(Get-NetworkSnapshot $phase)){ $netSamples += $n }
    foreach($pinfo in $procs){
        foreach($m in @(Get-ProcessModulesSafe $pinfo.ProcessId $phase)) { $moduleSamples += $m }
    }
    Write-Host "$phase procs=$($procs.Count)"
    Start-Sleep -Seconds $IntervalSec
    $i++
}

$alive = $false
try { $p2 = Get-Process -Id $proc.Id -ErrorAction Stop; $alive = $true } catch { $alive = $false }
Write-Host "AliveAfterDuration=$alive"
if ($alive) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
}

$post = [ordered]@{
    EndedAt = (Get-Date).ToString('o')
    LauncherPid = $proc.Id
    AliveAfterDuration = $alive
    ExitCode = if ($proc.HasExited) { $proc.ExitCode } else { $null }
    Services = @(Get-ServiceSnapshot 'post')
    Processes = @(Get-InterestingProcesses 'post')
    Network = @(Get-NetworkSnapshot 'post')
    ScmEvents = @(Get-RecentScmEvents 'post' $StartedAt)
    AceInstallRootTree = @(Get-TreeSnapshot $AceInstallRoot)
    AceGameDirTree = @(Get-TreeSnapshot $AceGameDir)
    UnityLogExists = Test-Path -LiteralPath $UnityLog
    UnityLog = $UnityLog
}
Write-Json 'process_samples.json' @($samples)
Write-Json 'module_samples.json' @($moduleSamples)
Write-Json 'service_samples.json' @($svcSamples)
Write-Json 'network_samples.json' @($netSamples)
Write-Json 'post_snapshot.json' $post

$aceLoadedModules = @($moduleSamples | Where-Object { $_.FileName -match 'ACE|AntiCheat|超自然行动组Base|TPShell' -or $_.ModuleName -match 'ACE|AntiCheat|超自然行动组Base|TPShell' })
$servicePresent = @($post.Services | Where-Object { $_.Present -and $_.Name -ne 'ace-game-0' })
$serviceRunning = @($post.Services | Where-Object { $_.State -eq 'RUNNING' })
$summary = [ordered]@{
    RunDir = $RunDir
    GameRoot = $GameRoot
    Launcher = $Launcher
    DurationSec = $DurationSec
    UseBatchMode = [bool]$UseBatchMode
    LauncherPid = $proc.Id
    AliveAfterDuration = $alive
    AceOrTPShellModulesObserved = @($aceLoadedModules | Select-Object -Property Phase,Pid,ProcessName,ModuleName,FileName -Unique)
    AceServicesPresentPost = @($servicePresent | Select-Object Name,State,Path,FilePresent)
    AnyAceServiceRunningPost = @($serviceRunning | Select-Object Name,State,Path,FilePresent)
    NetworkSamplesCount = @($netSamples).Count
    ProcessSamplesCount = @($samples).Count
    ModuleSamplesCount = @($moduleSamples).Count
    UnityLogExists = Test-Path -LiteralPath $UnityLog
}
Write-Json 'summary.json' $summary
$summary | ConvertTo-Json -Depth 6
Stop-Transcript | Out-Null

