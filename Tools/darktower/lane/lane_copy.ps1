# DARKTOWER lane: a local copy of the half-pulled clone. Reads C:\UEProjects\MO57, writes only C:\UEProjects\MO57_Lane.
$Src = 'C:\UEProjects\MO57'
$Dst = 'C:\UEProjects\MO57_Lane'
'free on C: before, GB: ' + [int]((Get-PSDrive C).Free / 1GB)
$t0 = Get-Date
& robocopy $Src $Dst /E /MT:16 /R:1 /W:1 /NP /NFL /NDL `
    /XD "$Src\.git\lfs" "$Src\.claude" "$Src\.vs" "$Src\graphify-out" "$Src\Packaged" "$Src\Saved" "$Src\ExportForPS" "$Src\Art" "$Src\SteamArt" `
    /XF nul
$code = $LASTEXITCODE
'robocopy exit code: {0} (under 8 is success), {1:n1} min' -f $code, ((Get-Date) - $t0).TotalMinutes
'free on C: after, GB: ' + [int]((Get-PSDrive C).Free / 1GB)
if ($code -ge 8) { 'COPY FAILED'; exit 1 }
'COPY DONE'
exit 0
