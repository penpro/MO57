"""MO57 icon set. 24-unit grid, 2-unit padding (live area 20x20), 2-unit stroke,
round caps + joins, white on transparent. Escalated (tier-2) moodles = base glyph at
80% scale anchored top-left + solid escalation notch in the lower-right corner."""
import os, sys, io
import cairosvg
from PIL import Image

OUT = sys.argv[1] if len(sys.argv) > 1 else "/mnt/user-data/outputs/MO57_UI_Kit"
SVG_DIR, PNG_DIR = f"{OUT}/icons/svg", f"{OUT}/icons/png128"
os.makedirs(SVG_DIR, exist_ok=True); os.makedirs(PNG_DIR, exist_ok=True)

S = 'fill="none" stroke="#FFFFFF" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"'
F = 'fill="#FFFFFF" stroke="none"'

def p(d, extra=""):
    return f'<path d="{d}" {S} {extra}/>'
def fp(d):
    return f'<path d="{d}" {F}/>'
def c(cx, cy, r, fill=False):
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" {F if fill else S}/>'

DROP = "M12 3C12 3 6 10 6 14.5a6 6 0 0 0 12 0C18 10 12 3 12 3Z"
def drop_at(x, y, s):
    # drop centred at x, top at y, scale s (base height 17.5)
    return f'<g transform="translate({x} {y}) scale({s}) translate(-12 -3)"><path d="{DROP}" fill="none" stroke="#FFFFFF" stroke-width="{2/s:.2f}" stroke-linecap="round" stroke-linejoin="round"/></g>'

BASE = {
    # ---------- moodles ----------
    "cold": p("M12 3v18M4.2 7.5l15.6 9M4.2 16.5l15.6-9M9.5 4.5 12 6.5l2.5-2M9.5 19.5l2.5-2 2.5 2M4 11l2.6 1.5L4 14M20 11l-2.6 1.5L20 14"),
    "hot": c(12, 12, 4) + p("M12 2.5v2.5M12 19v2.5M2.5 12H5M19 12h2.5M5.3 5.3l1.8 1.8M16.9 16.9l1.8 1.8M5.3 18.7l1.8-1.8M16.9 7.1l1.8-1.8"),
    "damp": p(DROP),
    "wet": drop_at(8, 3, 0.8) + drop_at(16.5, 8, 0.7),
    "soaked": drop_at(7.5, 2.5, 0.65) + drop_at(16.5, 2.5, 0.65) + p("M3 19c1.5-1.3 3-1.3 4.5 0s3 1.3 4.5 0 3-1.3 4.5 0 3 1.3 4.5 0"),
    "hungry": p("M3 11h18a9 7 0 0 1-18 0ZM9 20.5h6M8 7c0-1.5 1-1.5 1-3M12 7c0-1.5 1-1.5 1-3M16 7c0-1.5 1-1.5 1-3"),
    "thirsty": p("M5.5 4h13l-1.6 16.5H7.1ZM7 15.5h10"),
    "tired": p("M3 11c3 4.5 15 4.5 18 0M6.2 14.2 4.8 16.5M12 15.6V18.3M17.8 14.2l1.4 2.3M4 7.5h16"),
    "bleeding": p("M10 3S5 9.5 5 13.2a5 5 0 0 0 10 0C15 9.5 10 3 10 3Z") + drop_at(18.5, 13, 0.42),
    "infection": c(12, 12, 4.5) + p("M12 7.5V5.5M12 16.5v2M7.5 12h-2M16.5 12h2M8.8 8.8 7.6 7.6M15.2 15.2l1.2 1.2M8.8 15.2l-1.2 1.2M15.2 8.8l1.2-1.2") + "".join(c(x, y, 1.4, True) for x, y in [(12,4),(12,20),(4,12),(20,12),(6.5,6.5),(17.5,17.5),(6.5,17.5),(17.5,6.5)]),
    "pain": p("M13.5 2.5 5 13.5h6l-1.5 8 9-11.5h-6.2Z"),
    "fever": p("M8 4.5a2 2 0 0 1 4 0v10a4 4 0 1 1-4 0ZM10 9v8M16 6h4M16 10h3M16 14h4"),
    "injured_limb": c(9.5, 4.5, 2) + c(14.5, 4.5, 2) + c(9.5, 19.5, 2) + c(14.5, 19.5, 2) + p("M10.3 6.3V10.5l1.8-1.3 1.6.6V6.3M10.3 17.7v-4.2l1.8 1.3 1.6-.6v3.5"),
    "unconscious": c(12, 12, 9) + p("M7.5 10.5c1 1 2.5 1 3.5 0M13 10.5c1 1 2.5 1 3.5 0M9.5 16h5"),
    # ---------- item types ----------
    "item_tool": p("M9 3.5h9.5l2 3.5H9ZM13.5 7v13.5"),
    "item_weapon": p("M20 4 9.5 14.5l-2-2L18 2.5ZM10.5 17.5l-4-4M7.5 15.5l-4 4.5M3.5 20l1 0"),
    "item_food": p("M12 7.5c-3-2.2-8-1.2-8 4.5 0 5 4 9 6 9 1 0 1.5-.5 2-.5s1 .5 2 .5c2 0 6-4 6-9 0-5.7-5-6.7-8-4.5ZM12 7.5c0-2 1-4 3.5-4.5"),
    "item_drink": c(12, 14, 7) + p("M10 7V3.5h4V7M8 14h8"),
    "item_medical": p("M4.6 15.2 15.2 4.6a2.8 2.8 0 0 1 4 0l.2.2a2.8 2.8 0 0 1 0 4L8.8 19.4a2.8 2.8 0 0 1-4 0l-.2-.2a2.8 2.8 0 0 1 0-4ZM9.5 10.3l4.2 4.2") + c(11.2, 11.2, 0.6, True) + c(12.8, 12.8, 0.6, True),
    "item_material": c(7, 15.5, 4) + c(17, 15.5, 4) + c(12, 7.5, 4) + c(7, 15.5, 1, True) + c(17, 15.5, 1, True) + c(12, 7.5, 1, True),
    "item_clothing": p("M8.5 3.5 3 6.5l2 4 2-1V20.5h10V9.5l2 1 2-4-5.5-3c0 2-1.5 3-3.5 3s-3.5-1-3.5-3Z"),
    "item_container": p("M6 8.5h12a1.5 1.5 0 0 1 1.5 1.5v9a1.5 1.5 0 0 1-1.5 1.5H6A1.5 1.5 0 0 1 4.5 19v-9A1.5 1.5 0 0 1 6 8.5ZM9 8.5V6a3 3 0 0 1 6 0v2.5M8 14h8v3.5H8Z"),
    "item_book": p("M3.5 5h6A2.5 2.5 0 0 1 12 7.5V20a2 2 0 0 0-2-2H3.5ZM20.5 5h-6A2.5 2.5 0 0 0 12 7.5V20a2 2 0 0 1 2-2h6.5Z"),
    "item_unknown": p("M12 3 20 7.5v9L12 21l-8-4.5v-9ZM4 7.5l8 4.5 8-4.5M12 12v9"),
    # ---------- severity shapes ----------
    "sev_ok": c(12, 12, 9) + p("M8 12.5l2.7 2.7L16 9.8"),
    "sev_notice": p("M12 2.8 21.2 12 12 21.2 2.8 12Z") + c(12, 12, 1.6, True),
    "sev_warning": p("M12 3.2 21.5 20H2.5ZM12 9.5v4.5") + c(12, 17, 1.2, True),
    "sev_critical": p("M8.3 3h7.4L21 8.3v7.4L15.7 21H8.3L3 15.7V8.3ZM12 7.5v5.5") + c(12, 16.5, 1.2, True),
    "sev_info": p("M5 3.5h14A1.5 1.5 0 0 1 20.5 5v14a1.5 1.5 0 0 1-1.5 1.5H5A1.5 1.5 0 0 1 3.5 19V5A1.5 1.5 0 0 1 5 3.5ZM12 11v6") + c(12, 7.8, 1.2, True),
    # ---------- UI glyphs ----------
    "close": p("M6 6l12 12M18 6 6 18"),
    "back": p("M15 4.5 7.5 12l7.5 7.5"),
    "confirm": p("M4.5 12.5l5 5L19.5 7"),
    "cancel": c(12, 12, 8.5) + p("M6 18 18 6"),
    "lock": p("M6 11h12a1 1 0 0 1 1 1v8a1 1 0 0 1-1 1H6a1 1 0 0 1-1-1v-8a1 1 0 0 1 1-1ZM8 11V8a4 4 0 0 1 8 0v3M12 15v2.5"),
    "settings": c(12, 12, 3) + c(12, 12, 6.5) + p("M12 2.5v3M12 18.5v3M2.5 12h3M18.5 12h3M5.3 5.3l2.1 2.1M16.6 16.6l2.1 2.1M5.3 18.7l2.1-2.1M16.6 7.4l2.1-2.1"),
    "save": p("M12 3.5v11M7.5 10l4.5 4.5 4.5-4.5M4 16.5v3.5h16v-3.5"),
    "load": p("M12 14.5v-11M7.5 8 12 3.5 16.5 8M4 16.5v3.5h16v-3.5"),
    "host": p("M2.5 20.5 12 4l9.5 16.5ZM12 20.5l-3-5.5M12 20.5l3-5.5M1.5 20.5h21"),
    "join": p("M3 12h10.5M10 8l4 4-4 4M15 3.5h4.5a1 1 0 0 1 1 1v15a1 1 0 0 1-1 1H15"),
    "trash": p("M4 6.5h16M9.5 6.5v-3h5v3M6 6.5l1 14h10l1-14M10 10.5v6M14 10.5v6"),
    "sort": p("M7 4v16M3.5 16.5 7 20l3.5-3.5M17 20V4M13.5 7.5 17 4l3.5 3.5"),
    "filter": p("M3.5 4.5h17l-6.5 8v6l-4 2v-8Z"),
    "search": c(10.5, 10.5, 6.5) + p("M15.3 15.3 20.5 20.5"),
    "arrow_up": p("M12 19.5v-15M6 10.5l6-6 6 6"),
    "arrow_down": p("M12 4.5v15M6 13.5l6 6 6-6"),
    "arrow_left": p("M19.5 12h-15M10.5 6l-6 6 6 6"),
    "arrow_right": p("M4.5 12h15M13.5 6l6 6-6 6"),
    "plus": p("M12 5v14M5 12h14"),
    "minus": p("M5 12h14"),
    "play": p("M7 4.5 19.5 12 7 19.5Z"),
    "stop": p("M7 6h10a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1Z"),
    "expand": p("M6 9l6 6 6-6"),
    "collapse": p("M6 15l6-6 6 6"),
    "drag_handle": "".join(c(x, y, 1.5, True) for x in (9, 15) for y in (6, 12, 18)),
    "clock": c(12, 12, 9) + p("M12 7v5l3.5 2"),
    "flame": p("M12 21c-4 0-6.5-2.6-6.5-6 0-4 3.5-6 4-11 3 2 7 6 7 11 0 3.4-2.5 6-4.5 6ZM12 21c-1.6 0-2.5-1.1-2.5-2.5 0-1.8 1.5-2.6 2.5-4.5 1 1.9 2.5 2.7 2.5 4.5 0 1.4-.9 2.5-2.5 2.5Z"),
    "weight": p("M5 20.5 7 9.5h10l2 11ZM9.5 9.5a2.5 2.5 0 1 1 5 0"),
    "heart": p("M12 20S3.5 14.8 3.5 8.8A4.3 4.3 0 0 1 12 7a4.3 4.3 0 0 1 8.5 1.8C20.5 14.8 12 20 12 20ZM5 12.5h3.5l1.5-2.5 2 4.5 1.5-2h5.5"),
    "person": c(12, 7.5, 3.5) + p("M4.5 20.5c0-4.1 3.4-7 7.5-7s7.5 2.9 7.5 7"),
    "skill": p("M4 20.5h16M6.5 20.5V14M11 20.5V10M15.5 20.5V6.5M20 20.5v-14") ,
    "quest": p("M5 21V3.5M5 4.5h12l-2.5 4 2.5 4H5"),
    "map_pin": p("M12 21s-6.5-6-6.5-11a6.5 6.5 0 0 1 13 0c0 5-6.5 11-6.5 11Z") + c(12, 10, 2.2),
    "refresh": p("M19.5 12a7.5 7.5 0 1 1-2.2-5.3M19.5 4v4.5H15"),
    "bug": p("M8.5 9h7a1 1 0 0 1 1 1v4.5a4.5 4.5 0 0 1-9 0V10a1 1 0 0 1 1-1ZM9.5 9V7.5a2.5 2.5 0 0 1 5 0V9M3.5 12h4M16.5 12h4M4.5 7l3 2M19.5 7l-3 2M4.5 18.5l3.3-2M19.5 18.5l-3.3-2M12 12v6"),
    "random": p("M3.5 7h4.2c3.6 0 5 10 8.6 10h4.2M3.5 17h4.2c1.4 0 2.4-1.5 3.3-3.4M13 10.4c.9-1.9 1.9-3.4 3.3-3.4h4.2M18 4l2.5 3L18 10M18 14l2.5 3-2.5 3"),
    "trend_up": p("M5 17 17 5M8 5h9v9"),
    "trend_down": p("M5 7l12 12M17 10v9H8"),
    "trend_flat": p("M4 12h15M14.5 7.5 19 12l-4.5 4.5"),
    # ---------- wind ----------
    "wind_arrow": p("M12 2.5 18 20.5 12 16.5 6 20.5Z"),
    "wind_ring": c(12, 12, 10) + p("M12 1v3M12 20v3M1 12h3M20 12h3") + fp("M12 0.8 13.6 3.6h-3.2Z"),
    # ---------- input glyphs (generic, no platform trademarks) ----------
    "mouse_left": p("M12 3.5h0a6 6 0 0 1 6 6v5a6 6 0 0 1-12 0v-5a6 6 0 0 1 6-6ZM12 3.5V10M6 10h12") + fp("M11 4.6A5 5 0 0 0 7 9h4Z"),
    "mouse_right": p("M12 3.5h0a6 6 0 0 1 6 6v5a6 6 0 0 1-12 0v-5a6 6 0 0 1 6-6ZM12 3.5V10M6 10h12") + fp("M13 4.6A5 5 0 0 1 17 9h-4Z"),
    "mouse_wheel": p("M12 3.5h0a6 6 0 0 1 6 6v5a6 6 0 0 1-12 0v-5a6 6 0 0 1 6-6Z") + fp("M11 6h2v5h-2Z"),
    "pad_face_south": c(12, 4.5, 2.3) + c(4.5, 12, 2.3) + c(19.5, 12, 2.3) + c(12, 19.5, 3.2, True),
    "pad_face_east": c(12, 4.5, 2.3) + c(4.5, 12, 2.3) + c(19.5, 12, 3.2, True) + c(12, 19.5, 2.3),
    "pad_face_west": c(12, 4.5, 2.3) + c(4.5, 12, 3.2, True) + c(19.5, 12, 2.3) + c(12, 19.5, 2.3),
    "pad_face_north": c(12, 4.5, 3.2, True) + c(4.5, 12, 2.3) + c(19.5, 12, 2.3) + c(12, 19.5, 2.3),
    "pad_dpad_up": p("M9.5 3.5h5v6h6v5h-6v6h-5v-6h-6v-5h6Z") + fp("M10 4h4v5.5h-4Z"),
    "pad_dpad_down": p("M9.5 3.5h5v6h6v5h-6v6h-5v-6h-6v-5h6Z") + fp("M10 14.5h4V20h-4Z"),
    "pad_dpad_left": p("M9.5 3.5h5v6h6v5h-6v6h-5v-6h-6v-5h6Z") + fp("M4 10h5.5v4H4Z"),
    "pad_dpad_right": p("M9.5 3.5h5v6h6v5h-6v6h-5v-6h-6v-5h6Z") + fp("M14.5 10H20v4h-5.5Z"),
    "pad_shoulder_l": p("M3.5 15.5V12a5 5 0 0 1 5-5h12v8.5Z") + fp("M7 10.5h2v3H7z"),
    "pad_shoulder_r": p("M20.5 15.5V12a5 5 0 0 0-5-5h-12v8.5Z") + fp("M15 10.5h2v3h-2z"),
    "pad_trigger_l": p("M6 20.5V9a5.5 5.5 0 0 1 11 0v11.5Z") + fp("M9 12h2v5H9z"),
    "pad_trigger_r": p("M18 20.5V9A5.5 5.5 0 0 0 7 9v11.5Z") + fp("M13 12h2v5h-2z"),
    "pad_stick_l": c(12, 12, 9) + c(12, 12, 4.5) + fp("M3.5 21h3v-3h-3Z"),
    "pad_stick_r": c(12, 12, 9) + c(12, 12, 4.5) + fp("M17.5 21h3v-3h-3Z"),
    "pad_menu": p("M6 7.5h12M6 12h12M6 16.5h12"),
    "pad_view": p("M4 6.5h10v8H4ZM10 14.5v3h10v-8h-6"),
}

NOTCH = '<path d="M21.5 15.5v6h-6Z" fill="#FFFFFF" stroke="none"/>'
ESCALATED = {"very_cold": "cold", "very_hot": "hot", "starving": "hungry",
             "dehydrated": "thirsty", "exhausted": "tired"}

ICONS = {}
for k, body in BASE.items():
    ICONS[k] = body
for k, base in ESCALATED.items():
    ICONS[k] = f'<g transform="scale(0.8)">{BASE[base]}</g>' + NOTCH

GROUPS = {
    "moodles": ["cold", "very_cold", "hot", "very_hot", "damp", "wet", "soaked", "hungry", "starving",
                "thirsty", "dehydrated", "tired", "exhausted", "bleeding", "infection", "pain", "fever",
                "injured_limb", "unconscious"],
    "items": ["item_tool", "item_weapon", "item_food", "item_drink", "item_medical", "item_material",
              "item_clothing", "item_container", "item_book", "item_unknown"],
    "severity": ["sev_ok", "sev_notice", "sev_warning", "sev_critical", "sev_info"],
    "ui": ["close", "back", "confirm", "cancel", "lock", "settings", "save", "load", "host", "join", "trash",
           "sort", "filter", "search", "arrow_up", "arrow_down", "arrow_left", "arrow_right", "plus", "minus",
           "play", "stop", "expand", "collapse", "drag_handle", "clock", "flame", "weight", "heart", "person",
           "skill", "quest", "map_pin", "refresh", "bug", "random", "trend_up", "trend_down", "trend_flat"],
    "wind": ["wind_arrow", "wind_ring"],
    "input": ["mouse_left", "mouse_right", "mouse_wheel", "pad_face_south", "pad_face_east", "pad_face_west",
              "pad_face_north", "pad_dpad_up", "pad_dpad_down", "pad_dpad_left", "pad_dpad_right",
              "pad_shoulder_l", "pad_shoulder_r", "pad_trigger_l", "pad_trigger_r", "pad_stick_l",
              "pad_stick_r", "pad_menu", "pad_view"],
}
assert set(sum(GROUPS.values(), [])) == set(ICONS), set(ICONS) ^ set(sum(GROUPS.values(), []))


def svg(body, size=24):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 24 24">'
            f'{body}</svg>')

def prefix(name):
    for g, names in GROUPS.items():
        if name in names:
            return {"moodles": "moodle_", "items": "", "severity": "", "ui": "ui_", "wind": "", "input": "input_"}[g]

files = {}
for name, body in ICONS.items():
    fn = f"ic_{prefix(name)}{name}"
    s = svg(body)
    open(f"{SVG_DIR}/{fn}.svg", "w").write(s)
    cairosvg.svg2png(bytestring=s.encode(), write_to=f"{PNG_DIR}/{fn}.png", output_width=128, output_height=128)
    files[name] = fn

# contact sheet on dark ground for review
names = sum(GROUPS.values(), [])
cols = 12
cell = 80
rows = (len(names) + cols - 1) // cols
sheet = Image.new("RGBA", (cols * cell, rows * cell), (24, 22, 18, 255))
for i, n in enumerate(names):
    im = Image.open(f"{PNG_DIR}/{files[n]}.png").resize((48, 48), Image.LANCZOS)
    sheet.alpha_composite(im, ((i % cols) * cell + 16, (i // cols) * cell + 16))
sheet.save(sys.argv[2] if len(sys.argv) > 2 else "/tmp/sheet.png")

import json
json.dump({"groups": GROUPS, "files": files,
           "rules": {"grid": 24, "padding": 2, "liveArea": 20, "stroke": 2, "caps": "round", "joins": "round",
                     "cornerRadius": "1 on rectangles, 1.5 on large rects", "fills": "only dots/notches >= 2 units",
                     "escalation": "tier-2 = base glyph scaled 0.8 from top-left + solid notch M21.5 15.5v6h-6Z",
                     "colour": "white only; tint at runtime"}},
          open(f"{OUT}/icons/manifest.json", "w"), indent=2)
print(len(ICONS), "icons")
