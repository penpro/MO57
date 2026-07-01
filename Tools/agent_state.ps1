<#
.SYNOPSIS
  One-shot MO57 editor/PIE state check for autonomous testing -- NO screenshots.

  Answers "is the editor open?" and "what stage is PIE at?" purely from script:
  Get-Process for the editor, and a bridge python probe (result in ue_out.txt, which is
  flushed immediately -- unlike MO57.log which the file logger buffers) for the PIE stage.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File Tools\agent_state.ps1
#>
$ErrorActionPreference = "Stop"
$TmpDir = Join-Path $env:LOCALAPPDATA "Temp\claude"
$Cmd = Join-Path $TmpDir "ue_cmd.txt"
$Out = Join-Path $TmpDir "ue_out.txt"
if (-not (Test-Path $TmpDir)) { New-Item -ItemType Directory -Path $TmpDir -Force | Out-Null }
function Send([string]$line) { Add-Content -Path $Cmd -Value $line -Encoding ascii }

$p = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue
if (-not $p) { Write-Host "EDITOR:  not running"; return }
Write-Host ("EDITOR:  running (PID {0}, up since {1:HH:mm:ss})" -f $p.Id, $p.StartTime)

# Probe the live world via the bridge. atl.pawn(world) is non-null only in-game.
Send "py:import agent_test_lib as atl; _p=atl.pawn(world); out('PIE_STATE world=' + world.get_name() + ' inGame=' + ('YES' if _p else 'NO') + ' pawn=' + (_p.get_name() if _p else 'none'))"
Start-Sleep -Seconds 1
# Take the evaluated result line, NOT the bridge's "[py-ok] ...out('PIE_STATE...)" echo.
$line = Get-Content $Out -Tail 8 -ErrorAction SilentlyContinue | Select-String 'PIE_STATE' | Where-Object { $_.Line -notmatch '\[py-' } | Select-Object -Last 1
if ($line) {
  Write-Host ("PIE:     " + (($line.Line) -replace '.*PIE_STATE ', ''))
} else {
  Write-Host "PIE:     no response (bridge idle -- editor may be mid-load or not ticking; foreground it and retry)"
}
