#!/usr/bin/env bash
# Perf experiments on the PACKAGED build: for each experiment (label | extra command-line args | console commands, comma separated), start a new game (seed 7), run MO.Perf.Tour over a few biomes and
# keep the [MOPERF] lines in Saved/PerfRuns/exp_<label>.txt. Prints a comparison table at the end (avg fps / p95 / gpu ms per biome).
#
#   Tools/perf_experiments.sh                       (the default experiment list below; SETTLE / BENCH / BIOMES override the tour)
#   EXPERIMENTS_FILE=my.txt Tools/perf_experiments.sh   one experiment per line:  label|extra args|cmd1,cmd2
#
# Needs a staged packaged build (Tools/ov.bat packages one). Run it with the editor closed: it uses the GPU. Do not run two at once.
set -u
cd /d/UEProjects/MO57 2>/dev/null || cd "$(git rev-parse --show-toplevel)"
EXE="Saved/StagedBuilds_DevTest/Windows/MO57.exe"
LOGDIR="Saved/StagedBuilds_DevTest/Windows/MO57/Saved/Logs"
SETTLE="${SETTLE:-60}"
BENCH="${BENCH:-20}"
BIOMES="${BIOMES:-Rainforest TemperateForest Meadow}"
mkdir -p Saved/PerfRuns

if [ -n "${EXPERIMENTS_FILE:-}" ]; then
  mapfile -t EXPS < "$EXPERIMENTS_FILE"
else
  EXPS=(
    "base||"
    "hairvox0||r.HairStrands.Voxelization 0"
    "hairstrands0||r.HairStrands.Strands 0"
    "swlumen||r.Lumen.HardwareRayTracing 0,r.Lumen.Reflections.HardwareRayTracing 0"
    "norayt|-NoRayTracing|r.Lumen.HardwareRayTracing 0,r.Lumen.Reflections.HardwareRayTracing 0"
    "naniteedge2||r.Nanite.MaxPixelsPerEdge 2"
    "vsmcheap||r.Shadow.Virtual.ResolutionLodBiasDirectional 1,r.Shadow.Virtual.ResolutionLodBiasLocal 1"
  )
fi

for E in "${EXPS[@]}"; do
  IFS='|' read -r LABEL ARGS CMDS <<< "$E"
  [ -z "$LABEL" ] && continue
  echo "=== $LABEL  args='$ARGS'  cmds='$CMDS'"
  rm -f "$LOGDIR/MO57.log"
  EXEC="MO.NewGame 7"
  [ -n "$CMDS" ] && EXEC="$EXEC,$CMDS"
  EXEC="$EXEC,MO.Perf.Tour $SETTLE $BENCH 1 0 $BIOMES"
  "$EXE" -NoSteam -NoQualityBenchmark -log -windowed -ResX=1920 -ResY=1080 $ARGS -ExecCmds="$EXEC" > /dev/null 2>&1 &
  PID=$!
  for i in $(seq 1 120); do
    sleep 10
    if grep -a -q "TOUR DONE" "$LOGDIR/MO57.log" 2>/dev/null; then break; fi
    if ! kill -0 $PID 2>/dev/null; then break; fi
  done
  sleep 3
  kill $PID 2>/dev/null
  sleep 4
  grep -a -h "\[MOPERF\]" "$LOGDIR/MO57.log" | grep -a "frames=" | tail -n $(echo $BIOMES | wc -w) | sed 's/^.*Warning: //' > "Saved/PerfRuns/exp_${LABEL}.txt"
  cat "Saved/PerfRuns/exp_${LABEL}.txt" | cut -c1-200
done

echo
echo "=== comparison (avg fps | p95 ms | gpu ms)"
python - <<'PY'
import glob, os, re
rows = {}
for p in sorted(glob.glob("Saved/PerfRuns/exp_*.txt")):
    label = os.path.basename(p)[4:-4]
    for l in open(p, encoding="utf-8", errors="replace"):
        m = re.search(r"\[MOPERF\]\s+(\w+)\s+frames=\d+ fps=([\d.]+) avg=([\d.]+)ms p50=[\d.]+ p95=([\d.]+).*gpu=([\d.]+)ms", l)
        if m:
            rows.setdefault(m.group(1), {})[label] = (float(m.group(2)), float(m.group(4)), float(m.group(5)))
for biome, d in rows.items():
    print(biome)
    base = d.get("base")
    for label, (fps, p95, gpu) in d.items():
        delta = "" if not base or label == "base" else "  (%+.1f fps, %+.1f ms gpu)" % (fps - base[0], gpu - base[2])
        print("   %-14s %5.1f fps  p95 %5.1f  gpu %5.1f%s" % (label, fps, p95, gpu, delta))
PY
