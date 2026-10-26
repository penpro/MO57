"""A contact sheet of every style the character creator offers, for a by-eye check (Wes, Oct 25 2026: the MetaHuman hair, beard and mustache
library in the creator). Drives the REAL creator in a game instance -- the picker's next button, then a screenshot of the creator's own view -- so
each tile is what a player sees for that choice, labelled with the picker's text.

    python Tools/ue.py inst start hair --creator --res 1920x1080
    python Tools/ue.py inst do hair run "MO.NewGame 20261025"          (wait until the creator is up: ue.py inst do hair run "MO.Test.FindWidget HairStyleText")
    python Tools/groom_contact_sheet.py hair HairStyle 39 D:/MOAssets/previews/hair/hair_styles.png
    python Tools/groom_contact_sheet.py hair Beard 17 D:/MOAssets/previews/hair/beards.png
    python Tools/groom_contact_sheet.py hair Mustache 17 D:/MOAssets/previews/hair/mustaches.png

Args: instance, picker (HairStyle / Beard / Mustache: its <name>Text and <name>NextButton), how many steps (the choice count: None + the
library), output PNG. Crop: the survivor's head and shoulders in the creator's 1920x1080 framing (PreviewScreenX 0.72). GROOMSHEET lines.
"""
import os
import re
import subprocess
import sys
import time

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = os.path.join(ROOT, "Saved", "Screenshots", "WindowsEditor")
CROP = (1150, 160, 1610, 700)   # head + shoulders at 1920x1080
TILE_W = 230
COLS = 8


def ue(inst, *cmd):
    r = subprocess.run([sys.executable, os.path.join(ROOT, "Tools", "ue.py"), "inst", "do", inst] + list(cmd), capture_output=True, text=True,
                       encoding="utf-8", errors="replace", cwd=ROOT)
    return r.stdout + r.stderr


def label(inst, picker):
    m = re.search(r"WIDGET '%sText' \(TextBlock\) text='([^']*)'" % picker, ue(inst, "run", "MO.Test.FindWidget %sText" % picker))
    return m.group(1) if m else "?"


def shot(inst):
    before = set(os.listdir(SHOTS))
    ue(inst, "run", "shot")
    for _ in range(40):
        new = sorted(set(os.listdir(SHOTS)) - before)
        if new:
            time.sleep(0.5)   # let the writer finish
            return os.path.join(SHOTS, new[-1])
        time.sleep(0.25)
    return None


def main():
    inst, picker, steps, out_png = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    tiles = []
    for i in range(steps):
        time.sleep(1.5)   # the groom streams in and its simulation settles
        name = label(inst, picker)
        path = shot(inst)
        print("GROOMSHEET %2d %-34s %s" % (i, name, os.path.basename(path) if path else "NO SHOT"), flush=True)
        if path:
            tile = Image.open(path).convert("RGB").crop(CROP)
            tile = tile.resize((TILE_W, int(TILE_W * tile.height / tile.width)))
            tiles.append((name, tile))
        ue(inst, "run", "MO.Test.ClickWidget %sNextButton" % picker)
    if not tiles:
        raise SystemExit("no screenshots")
    th = tiles[0][1].height + 22
    rows = (len(tiles) + COLS - 1) // COLS
    sheet = Image.new("RGB", (COLS * TILE_W, rows * th), (24, 24, 24))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for k, (name, tile) in enumerate(tiles):
        x, y = (k % COLS) * TILE_W, (k // COLS) * th
        sheet.paste(tile, (x, y))
        draw.text((x + 4, y + tile.height + 4), name, fill=(235, 235, 235), font=font)
    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    sheet.save(out_png)
    print("GROOMSHEET %d tile(s) -> %s" % (len(tiles), out_png))


if __name__ == "__main__":
    main()
