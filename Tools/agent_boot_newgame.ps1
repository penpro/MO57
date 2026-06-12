# agent_boot_newgame.ps1 - one-command, zero-screenshot, seed-safe new game boot.
#
#   powershell -File Tools\agent_boot_newgame.ps1 [-Seed 12345] [-SurvivorName "Test Pilot"] [-SkipNoon]
#
# Drives the NORMAL boot flow entirely in-process via claude_bridge:
#   LoadingLevel PIE -> intro -> SkipIntroVideo() -> StartNewGame() -> possession -> noon.
# Stage transitions are detected by tailing Saved/Logs/MO57.log - no screenshots, no clicks.
# Preconditions: editor running with MO57, claude_bridge loaded (init_unreal does this).

param(
    [int]$Seed = 0,
    [string]$SurvivorName = "",
    [switch]$SkipNoon
)

$ErrorActionPreference = "Stop"
$CmdFile = "C:\Users\penum\AppData\Local\Temp\claude\ue_cmd.txt"
$OutFile = "C:\Users\penum\AppData\Local\Temp\claude\ue_out.txt"
$GameLog = "D:\UEProjects\MO57\Saved\Logs\MO57.log"

function Send-Bridge([string]$line) {
    Add-Content $CmdFile -Value $line -Encoding ascii
}

function Wait-LogPattern([string]$pattern, [int]$timeoutSec, [long]$fromOffset) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        try {
            $fs = New-Object System.IO.FileStream($GameLog, 'Open', 'Read', 'ReadWrite')
            $fs.Seek($fromOffset, 'Begin') | Out-Null
            $sr = New-Object System.IO.StreamReader($fs)
            $chunk = $sr.ReadToEnd(); $sr.Close()
            if ($chunk -match $pattern) { return $true }
        } catch {}
        Start-Sleep -Milliseconds 700
    }
    return $false
}

$startOffset = (Get-Item $GameLog).Length
Write-Output "[boot] log offset $startOffset"

# Stage 0: bridge alive?
Send-Bridge 'py:out("boot-ping")'
Start-Sleep -Seconds 1
$tail = Get-Content $OutFile -Tail 3
if (-not ($tail -match "boot-ping")) { Write-Output "[boot] FAIL: bridge not responding"; exit 1 }

# Stage 1: ensure LoadingLevel, ending any running PIE first
Send-Bridge 'py:import agent_test_lib as atl; (atl.end_pie(out) if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor() else out("[atl] no PIE"))'
Start-Sleep -Seconds 4
Send-Bridge 'py:import agent_test_lib as atl; atl.ensure_loading_level(out)'
Start-Sleep -Seconds 8

# Stage 2: begin PIE (LoadingLevel boots into the menu map + intro)
Send-Bridge 'py:import agent_test_lib as atl; atl.begin_pie(out)'
if (-not (Wait-LogPattern "Media playback started|ShowMainMenu: pushed" 60 $startOffset)) {
    Write-Output "[boot] FAIL: intro/menu never appeared"; exit 1
}
Write-Output "[boot] intro/menu up"

# Stage 3: skip intro (safe even if already on the menu)
Send-Bridge 'py:import agent_test_lib as atl; atl.skip_intro(world, out)'
if (-not (Wait-LogPattern "ShowMainMenu: pushed|Main menu pushed" 20 $startOffset)) {
    Write-Output "[boot] FAIL: main menu never appeared"; exit 1
}
Write-Output "[boot] main menu up"

# Stage 4: start new game through the real flow
$seedArg = if ($Seed -ne 0) { "seed=$Seed, " } else { "" }
$nameArg = if ($SurvivorName) { "survivor_name='$SurvivorName', " } else { "" }
Send-Bridge ("py:import agent_test_lib as atl; atl.start_new_game(world, out, " + $seedArg + $nameArg + ")")
if (-not (Wait-LogPattern "possessed initial pawn" 120 $startOffset)) {
    Write-Output "[boot] FAIL: pawn never possessed (world gen can take ~45s - check log)"; exit 1
}
Write-Output "[boot] pawn possessed"

# Stage 5: noon for visibility
if (-not $SkipNoon) {
    Send-Bridge 'py:import agent_test_lib as atl; atl.set_noon(world, out)'
    Start-Sleep -Seconds 1
}

Write-Output "[boot] READY - in game at noon. Bridge output tail:"
Get-Content $OutFile -Tail 6
exit 0
