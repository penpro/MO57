"""DARKTOWER: render a job list of text-to-image prompts through WanGP's Python API (detached, so model downloads can run long).

Usage (in D:\\WanGP, its venv): python wangp_images.py <job.json> <out_dir>
job.json = [{"name": "...", "model": "flux2_klein_4b", "prompt": "...", "seed": 11, "resolution": "832x1216"}, ...]
Each image is copied to <out_dir>/<name>.<ext>; progress lines go to <out_dir>/progress.log ("DONE name secs" / "FAIL name error").
"""
import json
import os
import shutil
import sys
import time

sys.path.insert(0, r"D:\WanGP")
os.chdir(r"D:\WanGP")
from shared.api import init  # noqa: E402

job_path, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)
log = open(os.path.join(out_dir, "progress.log"), "a", encoding="utf-8", buffering=1)
jobs = json.load(open(job_path, encoding="utf-8"))
log.write("START %d jobs\n" % len(jobs))
session = init(console_output=False)
defaults = {}
for j in jobs:
    t0 = time.time()
    try:
        if j["model"] not in defaults:
            defaults[j["model"]] = session.get_default_settings(j["model"])
        s = dict(defaults[j["model"]])
        s["model_type"] = j["model"]
        s["prompt"] = j["prompt"]
        s["seed"] = j.get("seed", 11)
        s["resolution"] = j.get("resolution", "832x1216")
        if "prompt_enhancer" in s:
            s["prompt_enhancer"] = ""
        for k, v in j.get("extra", {}).items():
            s[k] = v
        r = session.run_task(s)
        files = [str(p) for p in r.generated_files]
        if not files:
            log.write("FAIL %s %s\n" % (j["name"], [str(e) for e in r.errors]))
            continue
        ext = os.path.splitext(files[-1])[1]
        shutil.copy(files[-1], os.path.join(out_dir, j["name"] + ext))
        log.write("DONE %s %.0fs\n" % (j["name"], time.time() - t0))
    except Exception as e:  # keep going: one model failing must not stop the rest
        log.write("FAIL %s %r\n" % (j["name"], e))
log.write("END\n")
