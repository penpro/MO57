# DARKTOWER lane: bring the COPY's tracked text to origin/master without touching any binary asset. Never run in C:\UEProjects\MO57.
# HEAD and index move to origin/master; non-LFS files are checked out; assets stay as lane_overlay.py left them. Run lane_fix_pointers.py after it.
Set-Location C:\UEProjects\MO57_Lane
if ((Get-Location).Path -ne 'C:\UEProjects\MO57_Lane') { 'wrong folder'; exit 2 }
$env:GIT_LFS_SKIP_SMUDGE = '1'
git config filter.lfs.smudge 'git-lfs smudge --skip -- %f'
git config filter.lfs.process 'git-lfs filter-process --skip'
cmd /c "git fetch origin 2>&1"
$old = (git rev-parse HEAD).Trim()
$new = (git rev-parse origin/master).Trim()
"old $old new $new"
$changed = git diff --name-status --no-renames $old $new
$text = @(); $assets = 0; $deleted = @()
foreach ($c in $changed) {
    $f = $c -split "`t"
    if ($f[0] -eq 'D') { $deleted += $f[1]; continue }
    if ($f[1] -match '\.(uasset|umap|ubulk|uexp|png|jpg|tga|exr|wav|mp4|fbx|glb)$') { $assets++ } else { $text += $f[1] }
}
"changed: {0}, text to check out: {1}, assets left to the overlay: {2}, deleted: {3}" -f $changed.Count, $text.Count, $assets, $deleted.Count
git update-ref HEAD $new
git read-tree $new
if ($LASTEXITCODE -ne 0) { 'read-tree failed'; exit 1 }
$i = 0
while ($i -lt $text.Count) {
    $batch = $text[$i..([Math]::Min($i + 60, $text.Count) - 1)]
    git checkout-index -f -- $batch
    if ($LASTEXITCODE -ne 0) { "checkout-index failed at $i"; exit 1 }
    $i += 60
}
foreach ($d in $deleted) {
    if (Test-Path -LiteralPath $d) { Remove-Item -LiteralPath $d -Force; "removed $d" }
}
git log --oneline -1
'GIT FIRST DONE'
