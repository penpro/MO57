"""Build Wes's look-check review page (Oct 22 2026: "collect a few screen shots and i can review them and give thumbs up/thumbs down").

    python Tools/review_page.py <manifest.json> <out.html>

The manifest lists the items; each names its shots (paths relative to the manifest's folder, 1280x720 JPEGs from Tools/review_shots.py):
    {"title": "Look checks, Oct 22", "intro": "...",
     "items": [{"id": "rainforest-trees", "title": "Rainforest trees, wider and taller", "ref": "Wes Oct 22",
                "look": "Trunks about three times as thick, crowns about three times as wide ...", "where": "seed 3014 at (598500, -2324000)",
                "pairs": [{"label": "eye height", "before": "rainforest_eye_before.jpg", "after": "rainforest_eye_after.jpg"}],
                "shots": [{"label": "...", "src": "x.jpg"}]}]}            # "shots" for an item with no before
The page reads and writes votes through the artifact's db capability: verdicts/<item id> = {verdict: "up" | "down", note, at}. Publish it
with capabilities {"db": {}} and its JPEGs as files (the script prints the list); read the votes back with ArtifactData list "verdicts".
"""
import html
import json
import os
import sys

PAGE = r"""<title>__TITLE__</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:wght@500;600;700&family=IBM+Plex+Sans:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&display=swap">
<style>
/* Layout: one review column; each check is a row with its notes on the left and its before / after shots on the right, stacking on phones. */
:root {
  --bg: #eef0ec; --surface: #fbfcfa; --ink: #1d231c; --muted: #5b6458; --line: #d4d9d0;
  --accent: #4f6b2c; --accent-ink: #f3f7ec; --up: #2f7a3e; --down: #a2442a; --chip: #e2e7dc;
  --display: "Barlow Semi Condensed", "Arial Narrow", system-ui, sans-serif;
  --body: "IBM Plex Sans", system-ui, -apple-system, "Segoe UI", sans-serif;
  --mono: "IBM Plex Mono", ui-monospace, "Cascadia Mono", Consolas, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg: #151913; --surface: #1d2219; --ink: #e5e9df; --muted: #9aa392; --line: #313828;
  --accent: #9cbf6a; --accent-ink: #172010; --up: #7fc48b; --down: #e08a6d; --chip: #283021; color-scheme: dark } }
:root[data-theme="dark"] {
  --bg: #151913; --surface: #1d2219; --ink: #e5e9df; --muted: #9aa392; --line: #313828;
  --accent: #9cbf6a; --accent-ink: #172010; --up: #7fc48b; --down: #e08a6d; --chip: #283021; color-scheme: dark }
body { background: var(--bg); color: var(--ink); font: 15px/1.55 var(--body); }
.wrap { max-width: 1240px; margin: 0 auto; padding-block: 28px 64px; padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
header.top { display: flex; flex-wrap: wrap; align-items: end; justify-content: space-between; gap: 12px 24px; border-bottom: 1px solid var(--line); padding-bottom: 18px; }
h1 { font: 700 2rem/1.1 var(--display); letter-spacing: .01em; margin: 0; text-wrap: balance; }
.intro { color: var(--muted); max-width: 68ch; margin: 6px 0 0; }
.tally { display: flex; gap: 8px; font: 500 .8rem/1 var(--mono); }
.tally span { background: var(--chip); border-radius: 999px; padding: 6px 10px; font-variant-numeric: tabular-nums; }
.tally .up { color: var(--up); } .tally .down { color: var(--down); }
.store-note { font: .8rem/1.4 var(--mono); color: var(--muted); margin-top: 10px; }
.items { display: grid; gap: 28px; margin-top: 26px; }
.item { display: grid; grid-template-columns: minmax(0, 300px) minmax(0, 1fr); gap: 22px; padding-top: 22px; border-top: 1px solid var(--line); }
.item:first-child { border-top: 0; padding-top: 0; }
.notes { display: flex; flex-direction: column; gap: 10px; min-width: 0; }
.notes h2 { font: 600 1.35rem/1.2 var(--display); margin: 0; text-wrap: balance; }
.ref { font: .75rem/1 var(--mono); color: var(--muted); text-transform: uppercase; letter-spacing: .06em; }
.look { margin: 0; }
.where { font: .8rem/1.4 var(--mono); color: var(--muted); overflow-wrap: anywhere; }
.vote { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 4px; }
.vote button { font: 600 .9rem/1 var(--body); padding: 10px 14px; border-radius: 6px; border: 1px solid var(--line); background: var(--surface); color: var(--ink); cursor: pointer; }
.vote button:hover { border-color: var(--muted); }
.vote button:focus-visible, textarea:focus-visible, .shot:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
.vote button[aria-pressed="true"].up { background: var(--up); border-color: var(--up); color: var(--surface); }
.vote button[aria-pressed="true"].down { background: var(--down); border-color: var(--down); color: var(--surface); }
.vote button:disabled { opacity: .55; cursor: default; }
textarea { width: 100%; min-height: 64px; resize: vertical; font: .9rem/1.4 var(--body); color: var(--ink); background: var(--surface); border: 1px solid var(--line); border-radius: 6px; padding: 8px 10px; box-sizing: border-box; }
.saved { font: .75rem/1 var(--mono); color: var(--muted); min-height: 1em; }
.pairs { display: grid; gap: 14px; min-width: 0; }
.pair-label { font: 500 .75rem/1 var(--mono); color: var(--muted); text-transform: uppercase; letter-spacing: .06em; margin-bottom: 6px; }
.pair { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
.pair.single { grid-template-columns: 1fr; }
figure { margin: 0; min-width: 0; }
figcaption { font: .72rem/1 var(--mono); color: var(--muted); margin-top: 4px; text-transform: uppercase; letter-spacing: .08em; }
.shot { display: block; width: 100%; padding: 0; border: 0; background: var(--chip); border-radius: 4px; overflow: hidden; cursor: zoom-in; aspect-ratio: 16 / 9; max-width: 100%; }
.shot img { display: block; width: 100%; height: 100%; object-fit: cover; }
.viewer { position: fixed; inset: 0; background: rgba(8, 10, 7, .92); display: flex; align-items: center; justify-content: center; z-index: 20;
  padding: calc(16px + env(safe-area-inset-top, 0px)) max(16px, env(safe-area-inset-right, 0px)) calc(16px + env(safe-area-inset-bottom, 0px)) max(16px, env(safe-area-inset-left, 0px)); }
.viewer img { max-width: 100%; max-height: 100%; cursor: zoom-out; }
@media (max-width: 760px) { .item { grid-template-columns: 1fr; } .pair { grid-template-columns: 1fr; } }
</style>
<div class="wrap">
  <header class="top">
    <div><h1>__TITLE__</h1><p class="intro">__INTRO__</p></div>
    <div class="tally" aria-live="polite"><span id="t-total"></span><span class="up" id="t-up"></span><span class="down" id="t-down"></span></div>
  </header>
  <p class="store-note" id="store-note">Votes are saved for Claude to read.</p>
  <div class="items" id="items"></div>
</div>
<div class="viewer" id="viewer" hidden><img id="viewer-img" alt=""></div>
<script>
const DATA = __DATA__;
const verdicts = {};
let db = null;
const $ = (s, el = document) => el.querySelector(s);
function el(tag, attrs = {}, kids = []) {
  const n = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) { if (k === "text") n.textContent = v; else n.setAttribute(k, v); }
  for (const k of [].concat(kids)) if (k) n.append(k);
  return n;
}
function shot(src, label) {
  const b = el("button", { class: "shot", type: "button", "aria-label": "Enlarge: " + label }, el("img", { src, alt: label, loading: "lazy" }));
  b.addEventListener("click", () => { $("#viewer-img").src = src; $("#viewer-img").alt = label; $("#viewer").hidden = false; });
  return b;
}
function render() {
  const root = $("#items");
  root.replaceChildren();
  for (const it of DATA.items) {
    const v = verdicts[it.id] || {};
    const up = el("button", { type: "button", class: "up", id: "up-" + it.id, "aria-pressed": String(v.verdict === "up"), text: "Looks right" });
    const down = el("button", { type: "button", class: "down", id: "down-" + it.id, "aria-pressed": String(v.verdict === "down"), text: "Not yet" });
    const note = el("textarea", { id: "note-" + it.id, placeholder: "What to change (optional)" });
    note.value = v.note || "";
    const saved = el("div", { class: "saved", id: "saved-" + it.id, text: v.at ? "Saved " + new Date(v.at).toLocaleString() : "" });
    if (!db) { up.disabled = down.disabled = true; note.disabled = true; }
    up.addEventListener("click", () => save(it.id, v.verdict === "up" ? "" : "up", note.value));
    down.addEventListener("click", () => save(it.id, v.verdict === "down" ? "" : "down", note.value));
    note.addEventListener("change", () => save(it.id, (verdicts[it.id] || {}).verdict || "", note.value));
    const notes = el("div", { class: "notes" }, [
      it.ref ? el("div", { class: "ref", text: it.ref }) : null,
      el("h2", { text: it.title }),
      el("p", { class: "look", text: it.look }),
      it.where ? el("div", { class: "where", text: it.where }) : null,
      el("div", { class: "vote" }, [up, down]), note, saved]);
    const pairs = el("div", { class: "pairs" });
    for (const p of it.pairs || []) {
      pairs.append(el("div", {}, [el("div", { class: "pair-label", text: p.label }), el("div", { class: "pair" }, [
        el("figure", {}, [shot(p.before, it.title + ", " + p.label + ", before"), el("figcaption", { text: "Before" })]),
        el("figure", {}, [shot(p.after, it.title + ", " + p.label + ", after"), el("figcaption", { text: "After" })])])]));
    }
    for (const s of it.shots || []) {
      pairs.append(el("div", {}, [el("div", { class: "pair-label", text: s.label }), el("div", { class: "pair single" }, [el("figure", {}, [shot(s.src, it.title + ", " + s.label)])])]));
    }
    root.append(el("article", { class: "item", id: it.id }, [notes, pairs]));
  }
  const all = DATA.items.length, ups = DATA.items.filter(i => (verdicts[i.id] || {}).verdict === "up").length,
        downs = DATA.items.filter(i => (verdicts[i.id] || {}).verdict === "down").length;
  $("#t-total").textContent = all + " checks"; $("#t-up").textContent = ups + " right"; $("#t-down").textContent = downs + " not yet";
}
let busy = Promise.resolve();
function save(id, verdict, note) {
  if (!db) return;
  const body = { verdict, note: note.slice(0, 2000), at: new Date().toISOString() };
  verdicts[id] = body;
  render();
  busy = busy.then(() => db.doc("verdicts/" + id).set(body)).then(
    () => { const s = $("#saved-" + id); if (s) s.textContent = "Saved " + new Date(body.at).toLocaleString(); },
    (e) => { const s = $("#saved-" + id); if (s) s.textContent = e && e.code === "invalid_argument" ? "You can view this page but not vote on it." : "Not saved: try again."; });
}
$("#viewer").addEventListener("click", () => { $("#viewer").hidden = true; });
document.addEventListener("keydown", (e) => { if (e.key === "Escape") $("#viewer").hidden = true; });
render();
(async () => {
  try { db = window.claude && window.claude.use ? await window.claude.use("db") : null; } catch (e) { db = null; }
  if (!db) { $("#store-note").textContent = "Voting is unavailable in this view: the shots are all here to look at."; render(); return; }
  db.collection("verdicts").onSnapshot((snap) => {
    for (const d of snap.docs) verdicts[d.id] = d.data();
    render();
  }, () => { $("#store-note").textContent = "Votes could not load: reload the page to try again."; });
  render();
})();
</script>
"""


def main():
    manifest_path, out_path = sys.argv[1], sys.argv[2]
    m = json.load(open(manifest_path, encoding="utf-8"))
    base = os.path.dirname(os.path.abspath(manifest_path))
    files = []
    for it in m["items"]:
        for p in it.get("pairs", []):
            files += [p["before"], p["after"]]
        files += [s["src"] for s in it.get("shots", [])]
    missing = [f for f in files if not os.path.exists(os.path.join(base, f))]
    if missing:
        print("missing shots: " + ", ".join(missing))
        return 1
    page = (PAGE.replace("__TITLE__", html.escape(m["title"])).replace("__INTRO__", html.escape(m.get("intro", "")))
            .replace("__DATA__", json.dumps({"items": m["items"]}).replace("</", "<\\/")))
    open(out_path, "w", encoding="utf-8").write(page)
    print(json.dumps(sorted(set(files))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
