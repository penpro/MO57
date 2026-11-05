"""MO57 promo: the edit. Assembles film.py's shots, grades them, lays the type and overlays, finishes the picture and muxes the sound. One shot at a time, cached, so a re-cut re-renders only what changed.

    python Tools/promo/edit.py [--only ID,ID] [--force] [--res 3840x2160] [--animatic]        Saved/Promo/graded/<shot>.mov for each shot, then the assembly
    python Tools/promo/edit.py --master                                                         mux Saved/Promo/audio/mix_master.wav: the deliverables in Saved/Promo/out/
    python Tools/promo/edit.py --sheet                                                          the contact sheet of every shot: Saved/Promo/out/contact_sheet.png

A shot's picture comes from Saved/Promo/mezz/<id>.mov (game footage, Tools/promo/capture.py) or Saved/Promo/kit/<id>/f_####.png (a kit render). Missing either, --animatic shows a labelled slate, so the timing and the
sound can be checked before the footage exists. Each shot: its grade LUT (Tools/promo/grade.py, film.py's grade name; per-shot fine-tuning in shot_grades.json), a bloom that lives in the highlights, then the type,
then a little vignette. Film grain and the final 10-bit encode go on at the assembly. Nothing here touches what the game drew but the grade.
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import film  # noqa: E402

P = os.path.join(ROOT, "Saved", "Promo")
GRADED = os.path.join(P, "graded")
OUT = os.path.join(P, "out")
LUTS = os.path.join(P, "luts")
KIT = os.path.join(P, "kit")
TUNE = os.path.join(HERE, "shot_grades.json")
FONT = r"C\:/Windows/Fonts/bahnschrift.ttf"
FADES = {"S01_void": (40, 0), "G01_pod": (30, 0), "S05_end": (0, 44)}     # frames of fade from and to black: everywhere else the film cuts
FLARE_AT = {16.0: "flare_warm", 18.0: "flare_warm_r", 26.0: "flare_teal", 31.0: "flare_warm"}     # a sweep of light across the cut at these bars
FLARE_OPACITY = 0.42
# where a card sits, as a share of the picture's height from its centre: each is where its shot is calm, away from a face, the sun, the wolf (looked at, Nov 5)
CARD_Y = {"T01_trillions": 0.21, "T02_who": 0.31, "T03_nothing": 0.30, "T08_instant": -0.30, "T09_regen": -0.24, "T11_find": -0.34, "T12_any": -0.33, "T13_friends": -0.05}


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.exit("ffmpeg failed:\n" + " ".join(cmd)[:1500] + "\n" + r.stderr[-2000:])
    return r


def seq(folder):
    """An image sequence's ffmpeg pattern, or None."""
    path = os.path.join(KIT, folder)
    if not os.path.isdir(path):
        return None
    frames = sorted(f for f in os.listdir(path) if f.startswith("f_") and f.endswith(".png"))
    if not frames:
        return None
    return os.path.join(path, "f_%04d.png"), len(frames), int(frames[0][2:6])


def source_args(shot, frames, w, h, animatic):
    """(ffmpeg input args, filter that gets it to exactly w x h, 24 fps, yuv444p10le)."""
    fit = "scale=%d:%d:flags=lanczos,format=yuv444p10le" % (w, h)
    if shot.kind == "black":
        return ["-f", "lavfi", "-i", "color=c=black:s=%dx%d:r=24:d=%f" % (w, h, frames / 24.0)], "format=yuv444p10le"
    mezz = os.path.join(P, "mezz", shot.id + ".mov")
    if shot.kind == "game" and os.path.isfile(mezz):
        return ["-i", mezz], fit
    s = seq(shot.id)
    if s:
        return ["-framerate", "24", "-start_number", str(s[2]), "-i", s[0]], fit
    if not animatic:
        sys.exit("no picture for %s (use --animatic for slates)" % shot.id)
    return ["-f", "lavfi", "-i", "color=c=0x1a2422:s=%dx%d:r=24:d=%f" % (w, h, frames / 24.0)], (
        "drawtext=fontfile='%s':text='%s':x=(w-text_w)/2:y=(h-text_h)/2:fontsize=%d:fontcolor=white@0.5,format=yuv444p10le" % (FONT, shot.id.replace("_", " "), h // 18))


def overlays_for(shot, start, frames):
    """Everything laid over this shot: (path pattern, first frame of the overlay in shot time, frames of it to skip, blend)."""
    out = []
    end = start + frames
    for c in film.CARDS:
        a, b = film.frame(c.bar), film.frame(c.bar + c.bars)
        if a < end and b > start and os.path.isdir(os.path.join(KIT, "cards", c.id)):
            out.append((c.id, max(a - start, 0), max(start - a, 0), "over"))
    if shot.lower:
        lower = "L_" + shot.id
        if os.path.isdir(os.path.join(KIT, "cards", lower)):
            out.append((lower, 10, 0, "over"))
    if shot.timelapse:
        tl = "TL_" + shot.id
        if os.path.isdir(os.path.join(KIT, "cards", tl)):
            out.append((tl, 0, 0, "over"))
    if shot.id == "S05_end" and os.path.isdir(os.path.join(KIT, "cards", "end_card")):
        out.append(("end_card", 0, 0, "over"))
    for bar, name in FLARE_AT.items():
        cut = film.frame(bar)
        a, b = cut - 8, cut + 8
        if a < end and b > start and os.path.isdir(os.path.join(KIT, "cards", name)):
            out.append((name, max(a - start, 0), max(start - a, 0), "screen"))
    return out


def tune(shot_id):
    if os.path.isfile(TUNE):
        with open(TUNE, encoding="utf-8") as f:
            return json.load(f).get(shot_id, {})
    return {}


def graph(shot, frames, w, h, base_filter, overlays):
    """The per-shot filter graph. Input 0 is the picture; inputs 1.. are overlay sequences."""
    t = tune(shot.id)
    lut = os.path.join(LUTS, shot.grade + ".cube").replace("\\", "/").replace(":", "\\:")
    chain = [base_filter]
    if shot.kind == "game":
        if "eq" in t:
            chain.append("eq=" + t["eq"])
        chain.append("lut3d='%s':interp=tetrahedral" % lut)
    if t.get("in_frames") is not None:
        pass
    parts = ["[0:v]%s,split[a0][b0]" % ",".join(chain)]
    # the bloom: highlights only, at quarter size, screened back
    bloom = float(t.get("bloom", 0.22 if shot.kind == "game" else 0.0))
    if bloom > 0:
        parts.append("[b0]scale=%d:%d:flags=bilinear,curves=all='0/0 0.62/0 1/1',gblur=sigma=%d,scale=%d:%d:flags=bicubic,format=yuv444p10le[gl]" % (w // 4, h // 4, max(h // 90, 4), w, h))
        parts.append("[a0]format=gbrp10le[a1];[gl]format=gbrp10le[gl2];[a1][gl2]blend=all_mode=screen:all_opacity=%.3f,format=yuv444p10le[pic]" % bloom)
    else:
        parts.append("[b0]nullsink")
        parts.append("[a0]null[pic]")
    last = "pic"
    for i, (name, at, skip, mode) in enumerate(overlays, 1):
        lab = "o%d" % i
        pre = "[%d:v]trim=start_frame=%d,setpts=PTS-STARTPTS+%d/(24*TB),scale=%d:%d,format=gbrap" % (i, skip, at, w, h)
        if mode == "over":
            # a soft dark cloud under the type so it reads on any picture: thin letters blurred, their alpha pushed up so the glow fills in, then blurred again. It rises and falls with the type.
            y = int(CARD_Y.get(name, 0.0) * h)
            halo = 1.0 if name.startswith("T") and name[1:3].isdigit() else 0.5
            parts.append(pre + ",split[%sa][%sb]" % (lab, lab))
            parts.append("[%sb]gblur=sigma=%d:planes=15,colorlevels=aimax=0.16,colorchannelmixer=rr=0:gg=0:bb=0:aa=%.2f,gblur=sigma=%d:planes=15[%ss]" % (lab, max(h // 90, 4), 0.66 * halo, max(h // 70, 6), lab))
            parts.append("[%s][%ss]overlay=y=%d:format=yuv444p10:eof_action=pass[%st]" % (last, lab, y, lab))
            parts.append("[%sa]colorlevels=rimax=0.88:gimax=0.88:bimax=0.88[%sb2];[%st][%sb2]overlay=y=%d:format=yuv444p10:eof_action=pass[%sr]" % (lab, lab, lab, lab, y, lab))
            last = lab + "r"
        else:
            # a flare is light: screen it, weighted by its own alpha
            parts.append(pre + ",colorchannelmixer=aa=%.2f[%sc]" % (FLARE_OPACITY, lab))
            parts.append("[%s][%sc]overlay=format=yuv444p10:eof_action=pass[%sr]" % (last, lab, lab))
            last = lab + "r"
    vign = float(t.get("vignette", 0.28 if shot.kind != "black" else 0.0))
    tail = "format=yuv444p10le"
    if vign:
        tail = "vignette=angle=%.3f:mode=forward,format=yuv444p10le" % vign
    fade_in, fade_out = FADES.get(shot.id, (0, 0))
    if fade_in:
        tail += ",fade=t=in:s=0:n=%d" % fade_in
    if fade_out:
        tail += ",fade=t=out:s=%d:n=%d" % (frames - fade_out, fade_out)
    parts.append("[%s]%s,trim=end_frame=%d,setpts=N/(24*TB)[out]" % (last, tail, frames))
    return ";".join(parts)


def render_shot(shot, w, h, animatic, force):
    frames = film.frame(shot.bar + shot.bars) - film.frame(shot.bar)
    start = film.frame(shot.bar)
    os.makedirs(GRADED, exist_ok=True)
    out = os.path.join(GRADED, shot.id + ".mov")
    in_args, base = source_args(shot, frames, w, h, animatic)
    overlays = overlays_for(shot, start, frames)
    g = graph(shot, frames, w, h, base, overlays)
    stamp_data = json.dumps([in_args, g, sources_stamp(shot, overlays)])
    stamp = hashlib.sha1(stamp_data.encode()).hexdigest()
    stamp_path = out + ".stamp"
    if not force and os.path.isfile(out) and os.path.isfile(stamp_path) and open(stamp_path).read() == stamp:
        return out, False
    args = ["ffmpeg", "-y", "-loglevel", "error"] + in_args
    for name, at, skip, mode in overlays:
        args += ["-framerate", "24", "-i", os.path.join(KIT, "cards", name, "f_%04d.png")]
    args += ["-filter_complex", g, "-map", "[out]", "-frames:v", str(frames), "-r", "24", "-c:v", "prores_ks", "-profile:v", "3", "-qscale:v", "7", "-vendor", "apl0", "-pix_fmt", "yuv422p10le", out]
    run(args)
    with open(stamp_path, "w") as f:
        f.write(stamp)
    return out, True


def sources_stamp(shot, overlays):
    """The sources' sizes and times, so a re-rendered kit shot or a new take re-renders the shot."""
    items = []
    for path in [os.path.join(P, "mezz", shot.id + ".mov"), os.path.join(KIT, shot.id), TUNE, os.path.join(LUTS, shot.grade + ".cube")] + [os.path.join(KIT, "cards", o[0]) for o in overlays]:
        if os.path.exists(path):
            st = os.stat(path)
            items.append((os.path.basename(path), st.st_mtime))
    return items


def assemble(w, h):
    """The hard-cut assembly (no re-encode), then one pass of grain over the whole film."""
    lst = os.path.join(GRADED, "_list.txt")
    with open(lst, "w", encoding="utf-8") as f:
        for s in film.SHOTS:
            f.write("file '%s'\n" % os.path.join(GRADED, s.id + ".mov").replace("\\", "/"))
    os.makedirs(OUT, exist_ok=True)
    cut = os.path.join(OUT, "picture_cut.mov")
    run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", cut])
    return cut


def master(animatic=False):
    cut = os.path.join(OUT, "picture_cut.mov")
    audio = os.path.join(P, "audio", "mix_master.wav")
    if not os.path.isfile(audio):
        sys.exit("no mix: python Tools/promo/audio/mix.py")
    grain = "noise=c0s=5:c0f=t+u:c1s=2:c1f=t+u:c2s=2:c2f=t+u"   # applied to the 8-bit web files only as dither; the 10-bit master keeps its own
    # the 4K master: HEVC 10-bit, high bit rate (NVENC, so it does not take the night); the web files from it
    m4k = os.path.join(OUT, "MO57_Promo_4K_master_hevc10.mp4")
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", cut, "-i", audio, "-map", "0:v", "-map", "1:a", "-c:v", "hevc_nvenc", "-preset", "p7", "-tune", "hq", "-rc", "vbr", "-cq", "14", "-b:v", "90M", "-maxrate", "160M", "-profile:v", "main10", "-pix_fmt", "p010le",
         "-color_primaries", "bt709", "-color_trc", "bt709", "-colorspace", "bt709", "-tag:v", "hvc1", "-c:a", "aac", "-b:a", "320k", "-shortest", "-movflags", "+faststart", m4k])
    web = os.path.join(OUT, "MO57_Promo_1080p.mp4")
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", cut, "-i", audio, "-map", "0:v", "-map", "1:a", "-vf", "scale=1920:1080:flags=lanczos:sws_dither=ed,format=yuv420p," + grain, "-c:v", "libx264", "-preset", "slow", "-crf", "17", "-profile:v", "high",
         "-color_primaries", "bt709", "-color_trc", "bt709", "-colorspace", "bt709", "-c:a", "aac", "-b:a", "256k", "-shortest", "-movflags", "+faststart", web])
    print("MASTER", m4k, "\nWEB", web)


def sheet():
    """Every shot's first, middle and last frames, labelled."""
    from PIL import Image, ImageDraw, ImageFont
    os.makedirs(OUT, exist_ok=True)
    tw, th = 480, 270
    pad = 26
    rows = [s for s in film.SHOTS if s.kind != "black"]
    img = Image.new("RGB", (tw * 3 + 180, (th + pad) * len(rows)), (16, 18, 18))
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(r"C:\Windows\Fonts\bahnschrift.ttf", 15)
    for r, s in enumerate(rows):
        src = os.path.join(GRADED, s.id + ".mov")
        frames = film.frame(s.bar + s.bars) - film.frame(s.bar)
        d.text((6, r * (th + pad) + 4), "%s\n%.2f s\n%s" % (s.id, film.seconds(s.bars), s.kind), font=font, fill=(220, 220, 210))
        for c, at in enumerate((1, frames // 2, frames - 2)):
            tmp = os.path.join(OUT, "_t.png")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-ss", "%.4f" % (at / 24.0), "-i", src, "-frames:v", "1", "-vf", "scale=%d:%d" % (tw, th), tmp])
            if os.path.isfile(tmp):
                with Image.open(tmp) as im:
                    img.paste(im.convert("RGB"), (180 + c * tw, r * (th + pad)))
    path = os.path.join(OUT, "contact_sheet.png")
    img.save(path)
    print("SHEET", path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--animatic", action="store_true")
    ap.add_argument("--res", default="3840x2160")
    ap.add_argument("--master", action="store_true")
    ap.add_argument("--sheet", action="store_true")
    a = ap.parse_args()
    if a.master:
        master()
        return
    if a.sheet:
        sheet()
        return
    w, h = (int(v) for v in a.res.split("x"))
    only = set(a.only.split(",")) if a.only else None
    for s in film.SHOTS:
        if only and s.id not in only:
            continue
        out, done = render_shot(s, w, h, a.animatic, a.force)
        print("SHOT %-20s %s" % (s.id, "rendered" if done else "cached"), flush=True)
    if not only:
        print("CUT", assemble(w, h))


if __name__ == "__main__":
    main()
