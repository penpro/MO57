"""MO57 design tokens -> tokens.json, tokens.csv (UE DataTable), contrast report."""
import json, csv, os, sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "/mnt/user-data/outputs/MO57_UI_Kit"
os.makedirs(OUT, exist_ok=True)

C = {
    # Surfaces: smoked iron / oiled leather. Warm, low chroma.
    "color.surface.0": ("#0E0D0B", "Backdrop / deepest well; HUD plate base"),
    "color.surface.1": ("#181612", "Panel (elevation 1) - default menu panel"),
    "color.surface.2": ("#221F1A", "Raised: cards, list rows, slots"),
    "color.surface.3": ("#2C2822", "Inputs, secondary buttons, row hover"),
    "color.surface.4": ("#38332B", "Pressed / active fill"),
    "color.paper.surface": ("#E6DDC8", "Paper insert: medical chart, journal page, ledger"),
    "color.paper.ink": ("#1C1914", "Body text on paper"),
    "color.paper.ink2": ("#4B4438", "Secondary text on paper"),
    "color.paper.rule": ("#BFB297", "Ruled lines / dividers on paper"),
    "color.border.subtle": ("#3A342C", "Dividers, panel edge"),
    "color.border.strong": ("#5C5447", "Input + secondary button edge"),
    "color.text.primary": ("#EEE7D7", "Body, labels (tallow-paper white)"),
    "color.text.secondary": ("#BDB3A0", "Secondary, captions"),
    "color.text.disabled": ("#7D7466", "Disabled (WCAG-exempt, still >= 3:1 on surface.1)"),
    "color.text.inverse": ("#15130F", "Text on light fills"),
    "color.accent": ("#6CB8A9", "Verdigris - the ONE accent: interactive / selected"),
    "color.accent.hover": ("#88CBBE", "Accent hover"),
    "color.accent.pressed": ("#4E9A8C", "Accent pressed"),
    "color.accent.ink": ("#0B1916", "Text/icon on accent fill"),
    "color.accent.muted": ("#6CB8A92E", "Selection fill (accent @ 18%)"),
    "color.focus.ring": ("#F3E2B3", "Focus ring - candle-light; never used for hover"),
    "color.hover.overlay": ("#FFFFFF0F", "Hover wash (white @ 6%) over any surface"),
    "color.pressed.overlay": ("#00000033", "Pressed wash (black @ 20%)"),
    "color.overlay.scrim": ("#070605B8", "Modal scrim (@ 72%) - world stays visible"),
    "color.hud.plate": ("#0E0D0BD1", "HUD backing plate (surface.0 @ 82%)"),
    "color.hud.outline": ("#000000CC", "Text outline over the world (@ 80%)"),
    "color.severity.ok": ("#8FBF6A", "Moss - shape: circle + check"),
    "color.severity.notice": ("#D8C56A", "Straw - shape: diamond"),
    "color.severity.warning": ("#E7A042", "Amber - shape: triangle"),
    "color.severity.critical": ("#EE6550", "Blood - shape: octagon + double border"),
    "color.severity.info": ("#7EA8DE", "Slate blue - shape: square"),
    "color.severity.critical.fill": ("#7A2318", "Critical fill behind primary text (danger button)"),
    "color.severity.warning.fill": ("#5E3E12", "Warning fill behind primary text"),
    "color.paper.severity.ok": ("#3A6422", "Severity on paper inserts (ink-dark)"),
    "color.paper.severity.notice": ("#6B5E10", "Severity on paper"),
    "color.paper.severity.warning": ("#7A4E06", "Severity on paper"),
    "color.paper.severity.critical": ("#9E2716", "Severity on paper"),
    "color.paper.severity.info": ("#2E5A8C", "Severity on paper"),
    "color.data.track": ("#2C2822", "Progress track"),
    "color.data.fill": ("#BDB3A0", "Neutral progress fill (time, xp)"),
}
CB = {
    "color.severity.ok": ("#56B4E9", "CB: sky blue"),
    "color.severity.notice": ("#F0E442", "CB: yellow"),
    "color.severity.warning": ("#E69F00", "CB: orange"),
    "color.severity.critical": ("#E890C4", "CB: reddish purple (lightened Okabe-Ito)"),
    "color.severity.info": ("#C9D3E3", "CB: blue-grey"),
    "color.severity.critical.fill": ("#6B2450", "CB critical fill"),
}

FONTS = {"display": "Bitter", "ui": "IBM Plex Sans", "mono": "IBM Plex Mono"}
TYPE = [
    # name, font, weight, px, line-height, letterspacing(em), case, notes
    ("display", "Bitter", 600, 48, 56, 0.0, "none", "Menu titles, main-menu wordmark"),
    ("title", "Bitter", 600, 32, 40, 0.0, "none", "Panel titles"),
    ("heading", "Bitter", 600, 24, 32, 0.0, "none", "Section headings, item names"),
    ("label", "IBM Plex Sans", 600, 15, 20, 0.08, "upper", "Column headers, group labels (caption floor)"),
    ("body", "IBM Plex Sans", 400, 18, 26, 0.0, "none", "Default text. Floor for anything read in sentences"),
    ("body.strong", "IBM Plex Sans", 600, 18, 26, 0.0, "none", "Button labels, row names"),
    ("caption", "IBM Plex Sans", 400, 15, 20, 0.01, "none", "Secondary/meta. Never below 15px"),
    ("hud", "IBM Plex Sans", 600, 18, 22, 0.01, "none", "HUD text over the world: outline 1 + plate"),
    ("numeric", "IBM Plex Mono", 500, 18, 24, 0.0, "none", "Counts, weights, timers (tabular by construction)"),
    ("numeric.large", "IBM Plex Mono", 500, 32, 36, -0.01, "none", "Vitals readouts"),
    ("numeric.small", "IBM Plex Mono", 500, 15, 20, 0.0, "none", "Stack counts on slots"),
]
SPACE = {f"space.{i}": v for i, v in enumerate([0, 4, 8, 12, 16, 24, 32, 48, 64, 96])}
RADIUS = {"radius.0": 0, "radius.1": 2, "radius.2": 4, "radius.pill": 999}
BORDER = {"border.thin": 1, "border.thick": 2, "focus.ring.width": 2, "focus.ring.offset": 2,
          "critical.border.width": 2, "critical.border.gap": 2}
OPACITY = {"opacity.hud.plate": 0.82, "opacity.scrim": 0.72, "opacity.panel": 1.0,
           "opacity.selection": 0.18, "opacity.hover": 0.06, "opacity.pressed": 0.20,
           "opacity.disabled.icon": 0.45, "opacity.texture.max": 0.08}
MOTION = {"motion.instant": 0, "motion.fast": 80, "motion.base": 140, "motion.slow": 200,
          "motion.pulse.critical": 1200,
          "motion.ease.out": "cubic-bezier(0.2,0,0,1)", "motion.ease.in": "cubic-bezier(0.4,0,1,1)",
          "motion.ease.inout": "cubic-bezier(0.4,0,0.2,1)"}
ICON = {f"icon.size.{s}": s for s in (16, 24, 32, 48, 64)}
SIZE = {"control.height.min": 44, "control.height.primary": 56, "row.height": 48, "row.height.dense": 44,
        "slot.size": 64, "slot.gap": 8, "portrait.size": 64, "tab.height": 48, "keycap.height": 32,
        "safe.margin": 40, "panel.padding": 24, "modal.width.s": 480, "modal.width.m": 640,
        "menu.panel.w": 1600, "menu.panel.h": 880}
TEXTFX = {"text.world.outline.size": 1, "text.world.outline.color": "color.hud.outline",
          "text.world.shadow.offset": [0, 2], "text.world.shadow.color": "#00000099",
          "text.world.plate": "color.hud.plate"}
A11Y = {"a11y.textScale.options": [1.0, 1.25, 1.5],
        "a11y.reducedMotion": "all motion.* -> 0 except motion.fast fades; motion.pulse.critical -> static double border",
        "a11y.colorblind": "swap color.severity.* with the colorblind block; shapes unchanged",
        "a11y.highContrast": "hud.plate opacity 0.82 -> 0.95; text.secondary -> text.primary"}


def rel_lum(hexs):
    h = hexs.lstrip('#')[:6]
    r, g, b = (int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))
    f = lambda c: c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    return 0.2126 * f(r) + 0.7152 * f(g) + 0.0722 * f(b)


def blend(fg, bg):
    """alpha-composite #RRGGBBAA over opaque bg"""
    fg = fg.lstrip('#'); bg = bg.lstrip('#')
    a = int(fg[6:8], 16) / 255 if len(fg) == 8 else 1
    out = ''.join(f"{round(int(fg[i:i+2],16)*a + int(bg[i:i+2],16)*(1-a)):02X}" for i in (0, 2, 4))
    return '#' + out


def ratio(a, b):
    la, lb = rel_lum(a), rel_lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def v(k, table=C):
    return table[k][0]


pairs = [
    ("text.primary on surface.1", v("color.text.primary"), v("color.surface.1"), 7.0),
    ("text.primary on surface.2", v("color.text.primary"), v("color.surface.2"), 7.0),
    ("text.primary on surface.3", v("color.text.primary"), v("color.surface.3"), 7.0),
    ("text.secondary on surface.1", v("color.text.secondary"), v("color.surface.1"), 4.5),
    ("text.secondary on surface.2", v("color.text.secondary"), v("color.surface.2"), 4.5),
    ("text.secondary on surface.3", v("color.text.secondary"), v("color.surface.3"), 4.5),
    ("text.disabled on surface.1", v("color.text.disabled"), v("color.surface.1"), 3.0),
    ("accent.ink on accent", v("color.accent.ink"), v("color.accent"), 7.0),
    ("accent text on surface.1", v("color.accent"), v("color.surface.1"), 4.5),
    ("text.primary on selection(row)", v("color.text.primary"), blend(v("color.accent.muted"), v("color.surface.2")), 7.0),
    ("focus.ring vs surface.1 (non-text 3:1)", v("color.focus.ring"), v("color.surface.1"), 3.0),
    ("focus.ring vs accent fill (non-text 3:1)", v("color.focus.ring"), v("color.accent.pressed"), 1.5),
    ("paper.ink on paper", v("color.paper.ink"), v("color.paper.surface"), 7.0),
    ("paper.ink2 on paper", v("color.paper.ink2"), v("color.paper.surface"), 4.5),
    ("text.primary on critical.fill", v("color.text.primary"), v("color.severity.critical.fill"), 4.5),
    ("text.primary on warning.fill", v("color.text.primary"), v("color.severity.warning.fill"), 4.5),
    ("HUD text on plate over WHITE snow", v("color.text.primary"), blend(v("color.hud.plate"), "#FFFFFF"), 4.5),
    ("HUD text on plate over sky #9CC8F0", v("color.text.primary"), blend(v("color.hud.plate"), "#9CC8F0"), 4.5),
    ("HUD text on plate over cave #050505", v("color.text.primary"), blend(v("color.hud.plate"), "#050505"), 7.0),
]
for s in ["ok", "notice", "warning", "critical", "info"]:
    pairs.append((f"severity.{s} on surface.1", v(f"color.severity.{s}"), v("color.surface.1"), 4.5))
    pairs.append((f"severity.{s} on HUD plate over snow", v(f"color.severity.{s}"), blend(v("color.hud.plate"), "#FFFFFF"), 3.0))
for s in ["ok", "notice", "warning", "critical", "info"]:
    pairs.append((f"paper.severity.{s} on paper", v(f"color.paper.severity.{s}"), v("color.paper.surface"), 4.5))
for s in ["ok", "notice", "warning", "critical", "info"]:
    pairs.append((f"CB severity.{s} on surface.1", v(f"color.severity.{s}", CB), v("color.surface.1"), 4.5))

report = []
for name, fg, bg, need in pairs:
    r = ratio(fg, bg)
    report.append({"pair": name, "fg": fg, "bg": bg, "ratio": round(r, 2), "required": need, "pass": r >= need})

tokens = {"$schema": "mo57-ui-tokens/1", "version": "1.0.0", "referenceResolution": [1920, 1080],
          "notes": "px values are at 1080p. UMG Font.Size = round(px * 0.75) because Slate treats font size as points at 96 DPI. Colours are sRGB hex (RRGGBB or RRGGBBAA); convert with FLinearColor::FromSRGBColor."}
for k, (val, note) in C.items():
    tokens[k] = val
for t in TYPE:
    name, fam, w, px, lh, ls, case, note = t
    b = f"type.{name}"
    tokens[f"{b}.font"] = fam
    tokens[f"{b}.weight"] = w
    tokens[f"{b}.size"] = px
    tokens[f"{b}.umgSize"] = round(px * 0.75)
    tokens[f"{b}.lineHeight"] = lh
    tokens[f"{b}.letterSpacing"] = ls
    tokens[f"{b}.letterSpacingUMG"] = round(ls * 1000)
    tokens[f"{b}.case"] = case
for d in (SPACE, RADIUS, BORDER, OPACITY, MOTION, ICON, SIZE, TEXTFX, A11Y):
    tokens.update(d)
tokens["colorblind"] = {k: val for k, (val, _) in CB.items()}
tokens["textScale"] = {"1.25": "multiply type.*.size, lineHeight, control.height.*, row.height by 1.25; layouts reflow, never clip",
                       "1.5": "as above x1.5; HUD moodle strip switches to icon-only + focused label"}

with open(f"{OUT}/tokens.json", "w") as f:
    json.dump(tokens, f, indent=2)

# DataTable-friendly CSV (Name,Value,Type,Notes)
with open(f"{OUT}/tokens_datatable.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["---", "Value", "Kind", "Notes"])
    for k, (val, note) in C.items():
        w.writerow([k, val, "Color", note])
    for k, (val, note) in CB.items():
        w.writerow(["colorblind." + k, val, "Color", note])
    for k, val in tokens.items():
        if k.startswith("color.") or k in ("colorblind", "textScale", "notes", "$schema", "version", "referenceResolution"):
            continue
        kind = "Float" if isinstance(val, (int, float)) else "String"
        w.writerow([k, json.dumps(val) if isinstance(val, list) else val, kind, ""])

with open(f"{OUT}/contrast_report.json", "w") as f:
    json.dump(report, f, indent=2)

for r in report:
    print(f"{'OK ' if r['pass'] else 'FAIL'} {r['ratio']:>6} (need {r['required']})  {r['pair']}")
