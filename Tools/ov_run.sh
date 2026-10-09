#!/usr/bin/env bash
# Called by Tools/ov.bat (do not call directly). Snapshots Tools/ov_task.sh, runs the snapshot from the repo root, tees its output to a log.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
hist="$root/Saved/Logs/ov_history"
mkdir -p "$hist"
ts="$(date +%Y%m%d_%H%M%S)_$$"
snap="$hist/$ts.sh"
log="$hist/$ts.log"
[ -f "$here/ov_task.sh" ] || echo 'echo "(no task script: Tools/ov_task.sh is written per job and is not tracked)"' > "$here/ov_task.sh"
cp "$here/ov_task.sh" "$snap"
echo "[ov] $ts: running a snapshot of Tools/ov_task.sh (kept in Saved/Logs/ov_history/)"
cd "$root" || exit 99
bash "$snap" 2>&1 | tee "$log"
rc=${PIPESTATUS[0]}
echo "[ov] $ts: exit code $rc"
exit "$rc"
