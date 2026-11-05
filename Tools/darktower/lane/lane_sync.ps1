# DARKTOWER lane: put the lane tree's tracked text at a ref (a branch on origin) without touching a binary asset. Run over ssh; never in C:\UEProjects\MO57.
# Usage: powershell -ExecutionPolicy Bypass -File lane_sync.ps1 -Ref darktower/batch-2026-11-04
param([Parameter(Mandatory = $true)][string]$Ref)
Set-Location C:\UEProjects\MO57_Lane
if ((Get-Location).Path -ne 'C:\UEProjects\MO57_Lane') { 'wrong folder'; exit 2 }
$env:GIT_LFS_SKIP_SMUDGE = '1'
$ErrorActionPreference = 'Continue'
cmd /c "git fetch origin 2>&1"
$new = (git rev-parse "origin/$Ref").Trim()
$old = (git rev-parse HEAD).Trim()
"HEAD $old -> origin/$Ref $new"
$lfs = @(git diff --name-only $old $new | Where-Object { $_ -match '\.(uasset|umap|ubulk|uexp)$' })
if ($lfs.Count -gt 0) {
    "WARNING: {0} binary assets differ between the two: they become LFS pointers here. Copy the real ones (lane_overlay.py) and scan for pointers (lane_fix_pointers.py)." -f $lfs.Count
    $lfs | Select-Object -First 20
}
cmd /c "git checkout -q --detach $new 2>&1"
if ($LASTEXITCODE -ne 0) { 'checkout failed'; exit 1 }
git log --oneline -1
git diff --stat --name-only $old $new | Measure-Object | Select-Object -ExpandProperty Count
'SYNC DONE'
