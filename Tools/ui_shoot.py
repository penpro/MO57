"""Full-resolution UI captures from the standalone instance (`ue.py inst start ui ...`, in a game: `inst do ui run "MO.NewGame 7"`).

    python Tools/ui_shoot.py <outdir> <prefix> hud                       # the HUD as it is (no menu)
    python Tools/ui_shoot.py <outdir> <prefix> InGame Inventory Crafting Skills Quest Possession Status Building
    python Tools/ui_shoot.py <outdir> <prefix> "Show:/MOFramework/UI/Kit/WBP_KitGallery.WBP_KitGallery_C"
    python Tools/ui_shoot.py <outdir> <prefix> Open:InGame Click:OptionsButton Click:LoadButton Close      # `Open:<menu>` stays open; `Click:<widget name>` = MO.Test.ClickWidget, capture, stay open; `Close` = MO.UI.Close

Each target is `MO.UI.Open <name>` (or `MO.UI.Show <class path>` for `Show:...`), a wait, a PrintWindow capture of the instance window (Tools/win_input.ps1), cropped to the game's 1920x1080 (top-left of the
2880x1620 window), then `MO.UI.Close` / `MO.UI.Hide`. Output: <outdir>/<prefix>_<name>.png.  Also writes <prefix>_sheet.png (a 2-column contact sheet at half size).
"""
import os
import subprocess
import sys
import time

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
WIN = os.path.join(ROOT, "Tools", "win_input.ps1")


def pid():
    p = os.path.join(os.environ.get("TEMP", ""), "claude", "inst_ui", "pid.txt")
    return open(p).read().strip()


def run(cmd):
    subprocess.run(UE + ["inst", "do", "ui", "run", cmd], capture_output=True, text=True)


def shoot(out_png):
    r = subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", WIN, "-ProcessId", pid(), "-Action", "shot", "-Out", out_png], capture_output=True, text=True)
    im = Image.open(out_png).convert("RGB")
    im.crop((0, 0, 1920, 1080)).save(out_png)
    return r.stdout.strip()


def main():
    outdir, prefix, targets = sys.argv[1], sys.argv[2], sys.argv[3:]
    os.makedirs(outdir, exist_ok=True)
    files = []
    for t in targets:
        if t == "hud":
            name, opener, closer = "hud", None, None
        elif t.startswith("Click:"):
            name, opener, closer = "click_" + t[6:], "MO.Test.ClickWidget " + t[6:], None   # click a named widget, capture, leave it open
        elif t.startswith("Open:"):
            name, opener, closer = t[5:], "MO.UI.Open " + t[5:], None   # open a menu and leave it open (for Click: targets)
        elif t.startswith("Close"):
            name, opener, closer = None, "MO.UI.Close", None
        elif t.startswith("Show:"):
            name, opener, closer = t.split("/")[-1].split(".")[0], "MO.UI.Show " + t[5:], "MO.UI.Hide"
        else:
            name, opener, closer = t, "MO.UI.Open " + t, "MO.UI.Close"
        if opener:
            run(opener)
        time.sleep(3.0)
        if name is None:
            continue
        f = os.path.join(outdir, "%s_%s.png" % (prefix, name))
        shoot(f)
        files.append(f)
        if closer:
            run(closer)
            time.sleep(1.0)
    w, h = 960, 540
    rows = (len(files) + 1) // 2
    sheet = Image.new("RGB", (w * 2, h * rows), (0, 0, 0))
    for i, f in enumerate(files):
        sheet.paste(Image.open(f).convert("RGB").resize((w, h), Image.LANCZOS), ((i % 2) * w, (i // 2) * h))
    sheet.save(os.path.join(outdir, "%s_sheet.png" % prefix))
    print("wrote %d captures + %s_sheet.png in %s" % (len(files), prefix, outdir))


if __name__ == "__main__":
    main()
