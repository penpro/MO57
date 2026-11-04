"""The TRELLIS crop plants: generated reference photos -> TRELLIS.2 -> foliage meshes on the world grass's master, A/B tested.

    python Tools/crop_art.py images --model z_image [--crops Barley,Flax] [--stages Ripe] [--seeds 11,23,37] [--run name]
        DARKTOWER renders each crop x stage x seed through WanGP (a scheduled task: it outlives the SSH session; a model WanGP lacks
        is downloaded on first use) -> Saved/CropArt/images/<run>/<Crop>_<Stage>_<model>_s<seed>.<ext>
    python Tools/crop_art.py clean <image> [<image> ...]
        paint the soil block out of each photo (Tools/crop_image_clean_blender.py) -> Saved/CropArt/clean/<stem>.png
    python Tools/crop_art.py trellis [--preset fine] <image> [<image> ...]
        TRELLIS.2 in DARKTOWER's ComfyUI (node 316 true: never Pixal3D) -> Saved/CropArt/glb/<image stem>[_fine].glb; "fine" keeps
        thin parts (awns, tendrils): the crops use it
    python Tools/crop_art.py prep <glb> <Crop> <Stage> [--ref <its photo>] [--tris 20000] [--tex 1024] [--tag ""]
        Blender: Tools/glb_item_prep_blender.py (orient keep, the crop's height, tris, texture px) then Tools/crop_finish_blender.py
        (vertex colours + ORT mask) and a preview -> Saved/CropArt/prep/Crop_<Crop>_<Stage><tag>.glb, _ORT.png, _preview.png
    python Tools/crop_art.py import <Crop> <Stage> [--tag ""] [--dest /Game/ImageTo3D/Crops]
        editor: ue.py asset import -> <dest>/<Crop>/<Stage><tag>, then Content/Python/crop_mesh_prepare.py (the foliage master,
        Nanite, no collision). Close every -game instance that loaded the mesh first: it locks the package and the save fails.

Why (DECISIONS 2026-11-03, "Crop plants are made with TRELLIS"; Wes: "download any and all models you want for the best results and if
results are questionable refine things and do A/B testing until we have photorealistic assets that we like"):
  - image models: Apache-2.0 / MIT only for shipped art. Nov 3 A/B (Saved/CropArt/ab1): FLUX.2 Klein 4B drew lavender for flax and a
    generic plant for pea; Z-Image Turbo drew true flax (pale-blue five-petal saucers, needle leaves) and true pea (paired leaflets,
    stipules, tendrils, pods). FLUX.2 Klein 9B / dev are out (their licence's 4(a) bars "data produced by the FLUX Model" from
    commercial use).
  - the prompts are botanical on purpose: "ripe barley" alone drew wheat (plump upright ears); the stage prompts name what tells the
    crop apart at that stage (two-row barley's long awns and nodding necks, emmer's flat awned ears, flax bolls, dry pea pods).
  - a field plot holds 196 plants on 16 m2, so each mesh is a CLUMP of several plants (real cereal stands ~300 plants / m2).
  - 20k triangles a plant holds its shape; 3k tears TRELLIS's thin leaf shells apart (Oct 30, DECISIONS F55).
Docs/Farming_And_Husbandry.md section 2 (crop art).
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys
import time
import urllib.request
import uuid

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue  # noqa: E402

ROOT = ue.ROOT
TOOLS = os.path.join(ROOT, "Tools")
ART = os.path.join(ROOT, "Saved", "CropArt")
BLENDER = r"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"
DT = "darktower"
DT_DIR = "C:/Users/Wes/mo57_crops"
COMFY = "http://192.168.1.169:8188"
WORKFLOW = r"D:/ClaudeMeetClaude/inbox-for-darkside/2026-10-24-copperawl/workflow_trellis2_api.json"

STYLE = ("studio product photograph on a plain light grey seamless background, the whole plant in frame from the cut base to the tips, "
         "centered, no pot, no soil, cut off at ground level, soft diffuse lighting, sharp focus, photorealistic botanical reference")
# Height = the ripe plant's real height (cm): the prep scales to it; the field plot then scales every stage by DT_Crops RipeHeightCm.
CROPS = {
    "Barley": {"height": 85, "stages": {
        "Sprouted": "a small clump of young two-row barley (Hordeum vulgare) seedlings at tillering, about 15 cm tall: six shoots, each with "
                    "three to five narrow bright green leaves with a faint blue-grey bloom, no stems or ears yet",
        "Growing": "a clump of seven barley plants (Hordeum vulgare) just in ear: upright green stems, broad green leaves with clasping "
                   "auricles, slender green two-row ears just out of the flag-leaf sheath with very long straight green awns",
        "Ripe": "a clump of seven ripe two-row barley stalks (Hordeum vulgare): slender flat golden ears about 8 cm long with very long "
                "straight awns twice the length of the ear, every ear nodding over on a bent neck, dry pale straw stems, a few dry curled leaves",
    }},
    "Emmer": {"height": 110, "stages": {
        # Nov 3: "no stems or ears yet" still drew plants in ear (both seeds): say what is there, not what is not, and say it twice.
        "Sprouted": "a small clump of very young emmer wheat (Triticum dicoccum) seedlings at the three-leaf stage, only 15 cm tall, like "
                    "young grass: six short shoots of three or four narrow soft green leaves each, leaves only, no stalks, no ears, no spikes",
        "Growing": "a clump of seven emmer wheat plants (Triticum dicoccum) in ear: tall green stems, green leaves, flat two-sided green "
                   "ears of tightly packed spikelets, each spikelet with long awns, held upright",
        "Ripe": "a clump of seven ripe emmer wheat stalks (Triticum dicoccum): flat two-sided reddish-brown ears of tightly packed "
                "spikelets with long dark awns, tall thin dry straw stems, ears upright to slightly nodding, a few dry leaves",
    }},
    "Flax": {"height": 90, "stages": {
        "Sprouted": "a small clump of young flax (Linum usitatissimum) seedlings about 12 cm tall: thin single upright stems densely set "
                    "with tiny narrow blue-green leaves, no flowers",
        "Growing": "a clump of common flax plants (Linum usitatissimum) in flower: slender upright stems branching only at the top, small "
                   "narrow blue-green leaves along the stems, many small pale sky-blue five-petal flowers only 2 cm across, round green buds",
        "Ripe": "a clump of ripe flax plants (Linum usitatissimum) ready to pull: slender dry golden-brown stems, many small round seed "
                "capsules (bolls) at the branching tops, a few dry leaves",
    }},
    "Pea": {"height": 70, "stages": {
        "Sprouted": "a young field pea seedling (Pisum sativum) about 15 cm tall: a single green stem with two pairs of rounded leaflets, "
                    "large leafy stipules and the first curling tendrils, no flowers",
        "Growing": "a field pea plant (Pisum sativum) in flower: sprawling green stems with paired oval leaflets and large leafy stipules, "
                   "curling tendrils, white flowers and a few young flat green pods",
        "Ripe": "a mature dry field pea plant (Pisum sativum) at harvest: yellowed and browning stems and dry leaves, many plump dry "
                "straw-coloured pea pods hanging from the vines",
    }},
}


def ssh(cmd, timeout=120):
    return subprocess.run(["ssh", DT, cmd], capture_output=True, text=True, timeout=timeout).stdout


def scp(src, dst, timeout=600):
    subprocess.run(["scp", "-q", src, dst], check=True, timeout=timeout)


# ---------------------------------------------------------------------------- images (DARKTOWER, WanGP)

def cmd_images(a):
    crops = a.crops.split(",") if a.crops else list(CROPS)
    stages = a.stages.split(",") if a.stages else ["Sprouted", "Growing", "Ripe"]
    seeds = [int(s) for s in a.seeds.split(",")]
    run = a.run or time.strftime("%Y%m%d-%H%M%S")
    jobs = [{"name": "%s_%s_%s_s%d" % (c, s, a.model, seed), "model": a.model, "seed": seed, "resolution": a.resolution,
             "prompt": CROPS[c]["stages"][s] + ", " + STYLE} for c in crops for s in stages for seed in seeds]
    local = os.path.join(ART, "jobs")
    os.makedirs(local, exist_ok=True)
    job_file = os.path.join(local, run + ".json")
    json.dump(jobs, open(job_file, "w", encoding="utf-8"), indent=1)
    ssh("New-Item -ItemType Directory -Force %s | Out-Null" % DT_DIR.replace("/", "\\"))
    for f in ("wangp_images.py", "wangp_run_job.ps1"):
        scp(os.path.join(TOOLS, "darktower", f), "%s:%s/%s" % (DT, DT_DIR, f))
    scp(job_file, "%s:%s/%s.json" % (DT, DT_DIR, run))
    out_dir = "D:\\WanGP\\outputs\\mo57_" + run
    print(ssh("powershell -NoProfile -ExecutionPolicy Bypass -File %s\\wangp_run_job.ps1 -Job %s\\%s.json -Out %s -Name MO57_%s"
              % (DT_DIR.replace("/", "\\"), DT_DIR.replace("/", "\\"), run, out_dir, run)).strip(), "--", len(jobs), "jobs, run", run, flush=True)
    seen = set()
    while True:
        time.sleep(30)
        log = ssh("Get-Content %s\\progress.log -ErrorAction SilentlyContinue" % out_dir).replace("\r", "").splitlines()
        for line in log:
            if line not in seen:
                seen.add(line)
                print(line, flush=True)
        if "END" in log:
            break
    dest = os.path.join(ART, "images", run)
    os.makedirs(dest, exist_ok=True)
    scp("%s:%s/*.*" % (DT, out_dir.replace("\\", "/")), dest)
    print("IMAGES", dest, len(glob.glob(os.path.join(dest, "*.*"))))


def cmd_clean(a):
    out = os.path.join(ART, "clean")
    os.makedirs(out, exist_ok=True)
    for path in a.images:
        dst = os.path.join(out, os.path.splitext(os.path.basename(path))[0] + ".png").replace("\\", "/")
        for ln in _blender("crop_image_clean_blender.py", os.path.abspath(path).replace("\\", "/"), dst):
            print(ln)


# ---------------------------------------------------------------------------- TRELLIS.2 (DARKTOWER, ComfyUI)

def _upload(path):
    name = "MO57_" + os.path.basename(path)
    boundary = uuid.uuid4().hex
    body = (("--%s\r\nContent-Disposition: form-data; name=\"image\"; filename=\"%s\"\r\nContent-Type: application/octet-stream\r\n\r\n"
             % (boundary, name)).encode() + open(path, "rb").read()
            + ("\r\n--%s\r\nContent-Disposition: form-data; name=\"overwrite\"\r\n\r\ntrue\r\n--%s--\r\n" % (boundary, boundary)).encode())
    req = urllib.request.Request(COMFY + "/upload/image", body, {"Content-Type": "multipart/form-data; boundary=" + boundary})
    return json.load(urllib.request.urlopen(req, timeout=60))["name"]


def cmd_trellis(a):
    os.makedirs(os.path.join(ART, "glb"), exist_ok=True)
    for path in a.images:
        stem = os.path.splitext(os.path.basename(path))[0]
        wf = json.load(open(WORKFLOW, encoding="utf-8"))
        wf["122"]["inputs"]["image"] = _upload(path)
        wf["316"]["inputs"]["value"] = True   # TRELLIS.2; the template's false picks Pixal3D, whose licence is not cleared
        stem = stem + ("" if a.preset == "default" else "_" + a.preset)
        wf["9001"]["inputs"]["filename_prefix"] = "3d/MO57_Crop_" + stem
        if a.preset == "fine":
            # Thin parts (awns, tendrils, flax stems) die in the template's remesh: a 768 grid (~1.2 mm voxels at plant size), "solid"
            # sign mode (a graph cut that fills thin shells), 20 smoothing passes, and drop_small_components 0.01 (a loose awn is
            # under 1 % of the mesh). Here: a 1536 grid, "udf" (keeps thin double sheets), 3 passes, 0.0005, and room for 1.5M faces.
            r = wf["241"]["inputs"]
            for k in [k for k in r if k.startswith("sign_mode.")]:
                del r[k]
            r.update({"resolution": 1536, "sign_mode": "udf", "sign_mode.qef": False, "sign_mode.drop_inverted_components": False,
                      "sign_mode.drop_enclosed_components": False, "smooth_iters": 3, "drop_small_components": 0.0005})
            wf["186"]["inputs"]["target_face_count"] = 1500000
        for k, v in (a.set or []):
            node, field = k.split(".", 1)
            wf[node]["inputs"][field] = json.loads(v)
        req = urllib.request.Request(COMFY + "/prompt", json.dumps({"prompt": wf, "client_id": "mo57"}).encode(), {"Content-Type": "application/json"})
        pid = json.load(urllib.request.urlopen(req, timeout=60))["prompt_id"]
        t0 = time.time()
        while time.time() - t0 < 2400:
            time.sleep(10)
            hist = json.load(urllib.request.urlopen(COMFY + "/history/" + pid, timeout=60))
            if pid not in hist:
                continue
            st = hist[pid].get("status", {})
            files = [f.get("filename") for node in hist[pid].get("outputs", {}).values() for v in node.values() if isinstance(v, list)
                     for f in v if isinstance(f, dict) and str(f.get("filename", "")).endswith(".glb")]
            if st.get("status_str") != "success" or not files:
                print("TRELLIS FAIL", stem, st.get("status_str"), json.dumps(st.get("messages", [])[-1:], default=str)[:800], flush=True)
                break
            local = os.path.join(ART, "glb", stem + ".glb")
            scp("%s:D:/ComfyUI/output/3d/%s" % (DT, files[-1]), local)
            print("TRELLIS", stem, "%.0fs" % (time.time() - t0), local, flush=True)
            break


# ---------------------------------------------------------------------------- prep (Blender)

def _blender(script, *args, timeout=1800):
    r = subprocess.run([BLENDER, "-b", "--factory-startup", "--python", os.path.join(TOOLS, script), "--"] + [str(x) for x in args],
                       capture_output=True, text=True, timeout=timeout)
    return [ln for ln in r.stdout.splitlines() if ln.startswith(("PREP", "CROPFINISH", "GLBPREVIEW", "CLEANIMG", "Error", "Traceback"))]


def cmd_prep(a):
    name = "Crop_%s_%s%s" % (a.crop, a.stage, a.tag)
    pdir = os.path.join(ART, "prep")
    os.makedirs(pdir, exist_ok=True)
    mid = os.path.join(pdir, name + "_item.glb").replace("\\", "/")
    out = os.path.join(pdir, name + ".glb").replace("\\", "/")
    ort = os.path.join(pdir, name + "_ORT.png").replace("\\", "/")
    src = os.path.abspath(a.glb).replace("\\", "/")
    for ln in _blender("glb_item_prep_blender.py", src, mid, CROPS[a.crop]["height"], "keep", a.tris, a.tex):
        print(ln)
    extra = [a.rough_lo, a.rough_hi, a.transl, os.path.abspath(a.ref).replace("\\", "/") if a.ref else "-"]
    for ln in _blender("crop_finish_blender.py", mid, out, ort, *extra):
        print(ln)
    for ln in _blender("glb_preview_blender.py", out[:-4] + "_novc.glb", os.path.join(pdir, name + "_preview.png").replace("\\", "/"), 1024):
        print(ln)


# ---------------------------------------------------------------------------- import (editor)

def cmd_import(a):
    name = "Crop_%s_%s%s" % (a.crop, a.stage, a.tag)
    pdir = os.path.join(ART, "prep")
    folder = "%s/%s/%s%s" % (a.dest, a.crop, a.stage, a.tag)
    env = dict(os.environ, MSYS_NO_PATHCONV="1")
    r = subprocess.run([sys.executable, os.path.join(TOOLS, "ue.py"), "asset", "import", os.path.join(pdir, name + ".glb"), folder],
                       capture_output=True, text=True, env=env, timeout=600)
    print("\n".join(ln for ln in r.stdout.splitlines() if "[import]" in ln)[:400])
    ort = os.path.join(pdir, name + "_ORT.png").replace("\\", "/")
    code = "import importlib, crop_mesh_prepare as c; importlib.reload(c); out(c.prepare(%r, %r))" % (folder, ort)
    r = subprocess.run([sys.executable, os.path.join(TOOLS, "ue.py"), "py", "--timeout", "500", "-c", code],
                       capture_output=True, text=True, env=env, timeout=600)
    print("\n".join(ln for ln in r.stdout.splitlines() if "CROPPREP" in ln or "Traceback" in ln or "Error:" in ln))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("images")
    s.add_argument("--model", default="z_image")
    s.add_argument("--crops", default="")
    s.add_argument("--stages", default="")
    s.add_argument("--seeds", default="11,23,37")
    s.add_argument("--resolution", default="832x1216")
    s.add_argument("--run", default="")
    s = sub.add_parser("clean")
    s.add_argument("images", nargs="+")
    s = sub.add_parser("trellis")
    s.add_argument("images", nargs="+")
    s.add_argument("--set", nargs=2, action="append", metavar=("NODE.FIELD", "JSON"), help="override a workflow input, e.g. --set 241.resolution 1024")
    s.add_argument("--preset", choices=("default", "fine"), default="default")
    s = sub.add_parser("prep")
    s.add_argument("glb")
    s.add_argument("crop", choices=list(CROPS))
    s.add_argument("stage", choices=["Sprouted", "Growing", "Ripe"])
    s.add_argument("--tris", type=int, default=20000)
    s.add_argument("--tex", type=int, default=1024)
    s.add_argument("--tag", default="")
    s.add_argument("--rough-lo", type=float, default=0.6)
    s.add_argument("--rough-hi", type=float, default=0.9)
    s.add_argument("--transl", type=float, default=0.5)
    s.add_argument("--ref", default="", help="the reference photo the GLB was made from: its colours are matched (TRELLIS bakes pale)")
    s = sub.add_parser("import")
    s.add_argument("crop", choices=list(CROPS))
    s.add_argument("stage", choices=["Sprouted", "Growing", "Ripe"])
    s.add_argument("--tag", default="")
    s.add_argument("--dest", default="/Game/ImageTo3D/Crops")
    a = ap.parse_args()
    {"images": cmd_images, "clean": cmd_clean, "trellis": cmd_trellis, "prep": cmd_prep, "import": cmd_import}[a.cmd](a)


if __name__ == "__main__":
    main()
