# Claude Design prompt: MO57 UI design system

Paste everything under the line into Claude Design. It asks for a design system (tokens + 9-slice panel kit + icon set + per-screen specs) that can be applied to the existing Widget Blueprints with `python Tools/ue.py ui build <spec>`, which is how this project authors UMG (specs, not hand-clicking).

Ground truth used to write it: the 88 Widget Blueprints in `Plugins/MOFramework/Content/UI/` (inventory in the prompt), the packaged-build captures of every in-game menu (Oct 11 2026), and the engineering rules in `CLAUDE.md`.

---

## PROMPT

I am building **MO57**, an ultra-realistic open-world survival game in Unreal Engine 5.8 (PC, Steam co-op, 1-4 players; solo to colony-builder). Design pillars: realism first (everything models a real-world process: wounds bleed and scab over real time, metabolism, hypothermia, crafting takes real time), emergent civilization (you start alone with primitive tools and end up running a settlement of pawns), a fully destructible voxel world. The comparison set for tone is Project Zomboid, Kenshi, RimWorld, Tarkov, SCUM, Rust, Valheim, The Long Dark and Death Stranding's UI restraint. NOT a fantasy RPG, NOT sci-fi neon, NOT mobile-game glossy. The game's sci-fi origin (the player carries encoded "genetic memory" from a colony seeding program) is a late-game reveal, so the UI must not hint at it early; it may become slightly more technical later through a theme swap.

**What I need you to design: one cohesive UI design system that I can implement across every screen**, plus the assets. Right now the UI was built screen by screen and it shows: every menu has a different panel opacity, grey-on-grey text, tiny body text (12 px at 1080p), placeholder squares and "Text Block" strings, red-tinted lists, and no shared button, panel, list-row, tab, slider, checkbox or progress-bar language.

### Hard constraints (read these first)

1. **Engine/UI stack:** Unreal Engine UMG with CommonUI. Buttons are `UCommonButtonBase` widgets (one reusable `WBP_MOButton` with a Normal / Hovered / Pressed / Disabled / Selected style); menus are `UCommonActivatableWidget`s; everything must work with **mouse + keyboard AND gamepad focus** (so every interactive element needs a visible focus state that is distinct from hover).
2. **Reference resolution 1920x1080**, scaling cleanly from 1280x720 to 3840x2160 and ultrawide 21:9 (HUD anchors to safe zones; menus are centred panels, never stretched). **Minimum body text 18 px at 1080p**, secondary/caption 15 px, never below. Text must stay legible on top of a bright sky, snow and dark caves, because HUD text is drawn directly over the 3D world.
3. **Free / redistributable only.** Fonts must be SIL OFL or Apache 2.0 (Google Fonts is fine; name the exact font files and weights). Icons you create must be original or CC0; no trademarked iconography. I do not pay for assets and will not download anything not clearly licensed for commercial games.
4. **Everything must be tintable by code.** Glyph/icon textures are WHITE on transparent so I can tint them by state/severity from design tokens. Panels are 9-slice. I need a **token layer, not baked colour**: a single table I can load as a UE DataAsset / DataTable.
5. **Realism rule for the UI too:** no gamey shortcuts as design (no "XP burst" popups, no cooldown wheels, no glowing loot beams). Feedback should be informative and restrained: the player reads the state of their body, tools, and environment, not rewards. Prefer **diegetic-feeling, paper/leather/canvas/iron/tallow-candle material language for menus** (a field journal, a ledger, a craftsman's workbench card) over holographic panels, but legibility beats texture: no texture may sit behind body text at higher than 8% contrast variation.
6. **Tedium is the enemy.** Lists must support batch actions ("take all", "deposit matching", craft x N) and large click targets (**min 44 px high at 1080p**, 56 px for primary actions).
7. **Deliver in a form I can import**, listed in "Deliverables".

### The screens (this is the full inventory; design all of them to the same system)

HUD (always on, over the 3D world):
- **Status-effect strip ("moodles")**, bottom-left: icon + label pills for Cold/Hot/Wet/Hungry/Thirsty/Tired etc., 2 severities (warning amber, critical red), later maybe 20+ kinds; must stay readable at a glance and not crowd the view.
- **Breath meter** (swimming/diving) mid-lower screen; **interaction prompt** at the reticle ("[E] Pick up Flint"); **reticle**; **harvest/inspection/terraform progress** (a timed action with a visible duration and a "move to cancel" hint); **notifications** (top-right stack, 3 tiers: info, warning, critical); **tutorial banner** (top-centre, title + body); **quest tracker** (right, quest name, wrapped objective, progress bar); **mode indicator**; **wind indicator** (small compass-like arrow, art needed); **FPS/debug counter**; **colony bar** (persistent strip of pawn portraits with mood expression, current activity, alert state: critical = pulsing red border, urgent = orange dot).

Menus (modal, centred, full input focus):
- **Main menu**: New Game (seed input + random), Load, Host co-op, Join (session list with entries), Options, Quit; **loading overlay**; **intro**.
- **In-game menu** (pause-style but the game NEVER pauses: the world keeps running behind it; this menu must make that visible, e.g. a "world continues" clock/readout): Resume, Save, Load, Options, Bug report, Quit.
- **Options**: tabs for Graphics (quality preset + per-setting, resolution, fullscreen mode, VSync, frame cap, render scale), Audio (master/music/effects/ambient/voice sliders), Controls (key-binding rows with rebind capture, conflict highlight), Gameplay. Needs the full control set: tab bar, dropdown/combo, slider with numeric readout, checkbox/toggle, spin box, key-capture chip, "apply / revert" footer, "restart required" badge.
- **Save/Load panel**: slot cards (thumbnail, name, play time, in-game date, version), host-this-save button.
- **Inventory**: backpack grid + equipment slots on a paper-doll + "nearby items" list + item info panel (weight, durability, condition, nutrition...) + **item context menu** + drag visual; slot states (empty, filled, selected, hovered, drop-target-valid/invalid, equipped, damaged, spoiled).
- **Crafting**: station header (name, fuel remaining + time left), recipe list (left, scrollable, each row: icon, name, craftable / not-craftable state, skill gate), recipe detail (right: large name + icon, description, ingredient rows with have/need counts coloured by sufficiency, outputs, required skill with a progress hint, required station, craft time, amount spinner, Craft button, "why can't I craft this" line), craft queue strip. **Building** is the same pattern plus a placement context bar.
- **Skills panel**: skill tree/list with level, XP bar, decay indicator ("rusty": unused skills decay unless schooled), knowledge entries; **Quest journal** (list + detail + Track/Abandon).
- **Character status (medical)**: this is a signature screen. Body diagram with ~55 body parts coloured by injury, wound list (bleeding, bandaged, infected, scarred), vitals panel (heart rate, blood pressure, SpO2, glucose, blood volume, core temperature) with trend arrows and normal ranges, consciousness/shock state, conditions, nutrition/hydration. It should read like a clinical chart a field medic would keep, while remaining scannable in 3 seconds (tiered complexity: summary first, detail on demand).
- **Colony / possession menu**: pawn roster (portrait, name, mood, activity, health, alerts), **character card** (mood, history log, relationships, personality traits, skills), **task assignment** (job queue with reorder, priorities), survivor context menu.
- **Context menus** (ground items, harvestable nodes, survivor): compact radial-or-list, mouse and gamepad friendly.
- **Modals**: confirmation (2-3 buttons), text input, quick-pick (list of choices), error/toast; **bug report** form.
- **Empty / loading / error states** for every list (no placeholder squares: when an icon is missing show a neutral item-type glyph).

### Required content of the design system

1. **Brand + mood** in one paragraph and a mood board description (materials, light, grain). Pick ONE accent colour and justify it; everything else neutral. Include **severity colours** (ok / notice / warning / critical / info) AND a colour-blind-safe variant for each (severity must also be carried by shape/icon, never colour alone).
2. **Design tokens as a table** (name, light-on-dark value, notes): colours (surface 0-4, text primary/secondary/disabled, accent, severity x5, focus ring, selection, hover, pressed, overlay scrim), opacity levels, type scale (display / title / heading / body / caption / numeric-tabular with size, weight, line-height, letter-spacing, which font), spacing scale (4-px base), corner radii, border widths, shadow/outline for text over the world, motion (durations and easings; UI animation is allowed but short, 80-200 ms, no bounce), icon sizes (16/24/32/48/64).
3. **Component library with every state** (normal, hover, focus, pressed, disabled, selected, error): primary / secondary / tertiary / danger / icon buttons; list row; tab bar; dropdown; slider; toggle; checkbox; spin box; text input; progress bar (plain, segmented, with threshold markers); tooltip; scrollbar; panel (3 elevations); modal frame; divider; badge; tag/chip; inventory slot; portrait frame; key-cap glyph (for "[E]" prompts, plus gamepad glyph rule).
4. **Per-screen layout specs at 1920x1080**: for each screen above, an annotated layout with exact pixel rectangles (x, y, w, h), margins, grid, which tokens each element uses, and the focus/gamepad navigation order. Prioritise: HUD, Inventory, Crafting/Building, Character status, Options, Main/In-game menu, Colony, Skills/Quest, modals.
5. **Iconography system**: grid, stroke weight, corner treatment, a drawing rule so my own later icons match. Then **the icon set itself** (see deliverables).
6. **Accessibility**: contrast ratios actually computed (WCAG AA minimum, AAA for body text over panels), text scale option (100/125/150%), a reduced-motion mode, colour-blind mode, and what each does to the tokens.
7. **Do/Don't page** with the 10 mistakes most likely to occur when this is implemented in UMG (e.g. text over bright sky, opacity stacking of nested panels, focus ring clipped by a scroll box).

### Deliverables (as files, not prose)

- `tokens.json` (flat, namespaced keys like `color.surface.1`, `type.body.size`, with a `colorblind` override block).
- Fonts: names, weights, and download links; confirm OFL/Apache.
- **9-slice panel kit** as PNGs with the slice margins stated per file (panel elev 0/1/2, modal, button states, input field, slot, tab, tooltip, progress bar track/fill), delivered at 1x and 2x.
- **Icon set** as SVG + 128 px white-on-transparent PNG: all moodles (cold, very cold, hot, very hot, damp, wet, soaked, hungry, starving, thirsty, dehydrated, tired, exhausted, plus bleeding, infection, pain, fever, injured-limb, unconscious), item-type glyphs (tool, weapon, food, drink, medical, material, clothing, container, book), UI glyphs (close, back, confirm, cancel, lock, warning, info, settings, save, load, host, join, trash, sort, filter, search, arrow up/down/left/right, plus/minus, play/stop, expand/collapse, drag handle), a **wind indicator** arrow, and key-cap + gamepad glyph sheets.
- The annotated **screen specs** (one image + one table per screen) and a one-page **component sheet**.
- A **short implementation order** (what to build first so the whole game changes look the fastest: I expect tokens -> button/panel/list-row -> HUD -> inventory -> crafting -> status -> the rest).

### Questions to answer before you design (ask me ONE round, max 6 questions, then proceed with your best recommendation)

If anything above is ambiguous, pick the option you judge best and say so rather than asking; I prefer a decisive design I can react to over a menu of options.
