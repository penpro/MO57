# MO57 — Fable 5 Charter: From Framework to A-Rated Indie

**Author:** Fable 5 planning pass, 2026-07-02
**Status:** North-star charter for the next development epoch
**Companion docs:** `MO57_Master_Plan.md` (system audit), `PROJECT_STATUS.md` (issue tracker), `AUTONOMOUS_TOOLING.md` (the build/verify loop), `CLAUDE.md` (vision + architecture)

---

## 0. The Thesis

**MO57 already has more systems depth than most *shipped* survival games** — a body-part wound simulation, destructible voxel terrain, GUID-persistent identity, possess-any-pawn AI, a 122-recipe paleolithic→iron tech tree, and a weighted-build-part construction system. The audit-fix campaign (C1–C12 / 55 highs) made it **stable**. This session made it **fast to iterate** (`Tools/ue.py`: build→relaunch→boot→test in one command).

But almost none of that depth is yet **a game a stranger would call fun**, and none of it is **verified in co-op**. The gap between where we are and an A-rated indie is not "more systems" — it's converting *framework maturity* into *game maturity*:

> **Framework maturity** = the system exists, compiles, and the author can make it work.
> **Game maturity** = a co-op pair who has never seen the code has fun with it, understands it without a manual, and it never desyncs or eats their save.

**The Fable 5 mission: take every pillar from framework to game, ship Steam co-op, and put a coat of "real game" paint on the whole thing — so the next screenshot could be on a Steam store page.**

---

## 1. What "A-Rated Indie" Means for MO57

The bar is the tier of **Valheim, Satisfactory, Core Keeper, V Rising, Enshrouded** — not AAA fidelity, but *cohesion, feel, and a loop that holds a co-op group for 40+ hours*. Concretely, MO57 hits A-rated when:

1. **A friend can join your world from the Steam friends list in two clicks**, and everything you both do stays in sync.
2. **The core loop is legible and satisfying**: chop → knap → craft → build → survive → settle, each step with clear feedback and game feel.
3. **Combat feels weighty** and the wound sim is a *headline feature*, not a hidden spreadsheet.
4. **The world looks intentional** — cohesive lighting, weather, terrain, vegetation, and a consistent art read.
5. **There's a reason to keep playing** — a progression arc from lone survivor to settlement, with escalating threats and goals.
6. **It never crashes, never desyncs, never loses a save**, and runs at a stable frame rate.
7. **The first 15 minutes teach you the game** without a wiki.

Everything below serves those seven outcomes.

---

## 2. Operating Principle — the Four Gates

Every system this epoch touches must pass four gates before it's "done." This is the definition of framework→game:

| Gate | Question | How we verify (post-`ue.py`) |
|------|----------|------------------------------|
| **Co-op correct** | Does it replicate + stay in sync for a remote client? | 2-client PIE harness (Pillar 0) + Steam smoke |
| **Legible** | Can a new player tell what happened and why, from UI/feedback alone? | Play the loop as a naive user; UI-drive harness |
| **Fun in the loop** | Does it make the moment-to-moment better, not just more simulated? | Playtest the vertical slice |
| **Verified** | Is there a regression gate so it can't silently break? | `MO.Test.*` / automation test in `ue.py test` |

**Rule:** no pillar is "shipped" until all four gates are green. "It compiles" and "the host can do it" are *table stakes*, not done.

---

## 3. Pillar 0 — The Iteration Foundation (DO THIS FIRST)

*Rationale: you cannot build fun without a fast, verified feedback loop. This session built `ue.py`; the 2026-07-02 test-loop assessment found the specific primitives still missing. These are cheap, they compound, and every other pillar depends on them. Build them in the first `cycle`.*

- [ ] **`MO.Test.Input <RequestMove|RequestInteract|RequestJump|RequestAttack> [args]`** — drive gameplay through `IMOControllableInterface::Execute_Request*`, sidestepping the Enhanced-Input synthetic-key wall (#144). Unlocks movement / interaction / combat testing through the real controller→pawn seam.
- [ ] **`MO.Test.ClickWidget <name>`** — extend `MO.Test.FindWidget`'s widget iteration to find a `UMOCommonButton` and broadcast `OnClicked`. Unlocks real UI end-to-end flows (click Craft → assert enqueue).
- [ ] **`MO.AI.DumpBlackboard <pawn>` + `MO.AI.SetKey <pawn> <key> <val>`** — first-class AI observe/tweak (blackboard is already reflection-reachable via the bridge; these make it ergonomic + scriptable). Add a `UBTService_StateLogger` for BT decision visibility.
- [ ] **`ue.py test --auto`** — run the **91 already-written headless automation tests** (`MOFrameworkTests.cpp`, `MOMedicalSystemTests.cpp`) via `UnrealEditor-Cmd -unattended -nullrhi -ExecCmds="Automation RunTests MOFramework.+Medical.;Quit"`, parse pass/fail → exit code. Dormant coverage becomes a CI gate for free.
- [ ] **Save/load round-trip automation tests** — give→save→reload→assert for inventory, crafting queue (incl. offline progress), vitals. Closes the highest release-risk gap (40+ `BuildSaveData`/`ApplySaveData` pairs, currently zero exercised).
- [ ] **2-client PIE harness (#165)** — `MOEditorTestHelper` (enumerate PIE worlds by net-mode) + `editor.set_pie_players` command + `test_multiplayer.py`. This is the gate that makes **every MP pillar below verifiable**. Wire it into `ue.py mptest`.

**Exit:** `ue.py` can drive input, click UI, tweak AI, run the full automation suite headless, verify a save round-trip, and run a 2-client co-op assertion — all with pass/fail exit codes.

---

## 4. Pillar 1 — Steam Multiplayer (the biggest structural lift)

**Where we are:** Host-authoritative listen-server model with the *authority RPCs written* (`ServerApplyTerraform`, `ServerRequestEnqueueCraft`, `ServerPickUpWorldItem`, `ServerStartAttack`, `ServerPlaceBuilding`, `Server_PossessPawn`) and FastArraySerializer replication on inventory/crafting/wounds. **But there is NO online layer at all** — grep confirms zero `OnlineSubsystem`/Steam/session code. Today "multiplayer" means "two PIE windows on one machine, unverified."

**The gap to A-rated:** the entire Steam session lifecycle + verified replication. This is the "invite a friend, Satisfactory-style" pillar from the vision and it's the single biggest lift.

### 1A. Online transport & Steam session layer (net-new)
- [ ] Enable `OnlineSubsystemSteam` + `OnlineSubsystemUtils` plugins; set `DefaultEngine.ini` `[OnlineSubsystemSteam]` (dev App ID `480`/Spacewar to start; real App ID at store-page time).
- [ ] Session subsystem (`UMOSessionSubsystem`, GameInstance): `HostSession` (create listen-server session, presence + friends-only), `FindFriendSessions`, `JoinSession`, `DestroySession`, delegates for join/leave/failure.
- [ ] Steam friends-list invite flow: invite via overlay, accept-invite handler, join-in-progress. "Two clicks from the friends list" is the acceptance test.
- [ ] Travel: host `ServerTravel` into the loaded world; clients `ClientTravel` on join; seamless-travel for the loading level.
- [ ] Connection lifecycle: player join spawns/possesses a pawn from the shared world; **late-join full-state sync**; graceful leave; host migration is **out of scope** (host-authoritative, friends-only — document this).

### 1B. Verified replication (harden #132 / #162 / #163 via the 2-client harness)
- [ ] **Building state (#162)** — `UMOBuildProgressComponent` currently has zero replicated properties; a client sees completed walls as walk-through "Building…" forever. Replicate build-state/percent with an `OnRep` that runs the same `EnterCompletedState` collision-promotion the server does. *(Systemic pattern #1: restore/replicate path must replay live side effects.)*
- [ ] **Terraform visual (#132 remainder)** — `MulticastApplyTerraform` so remote clients see host voxel edits (first confirm Voxel Pro's own edit-replication to avoid double-apply).
- [ ] **Combat / crafting / pickup** — verify the existing RPCs land + replicate under real 2-client (combat state → client animation; queue delta → client UI; inventory delta).
- [ ] **Server-validation hardening (#163)** — reach/scale/rate on `ServerPlaceBuilding`; server-side `CanCraftRecipe` in enqueue; LOS+rate on pickup; don't broadcast authoritative success on the forwarded terraform path.

### 1C. Co-op world & saves
- [ ] World sharing: joiner plays in the host's persistent world; on leave, host keeps the world; per-player pawn records persist (GUID identity already supports this).
- [ ] Steam Cloud saves for the host's world.
- [ ] Co-op-aware persistence: the `UMOPersistenceSubsystem` snapshot must capture all connected players' pawns + the shared world consistently.

**Exit gates:** friend joins from Steam overlay in 2 clicks → both players build/craft/fight/dig with zero desync in a 30-min session → host saves, everyone reloads intact.

---

## 5. Pillar 2 — Building System (framework → real)

**Where we are:** Solid foundation — weighted `FMOBuildPart` construction, snapping for walls / half-walls / floors / roofs (incl. Peak45), structural collision profiles, ghost placement with rotation/flip, deposit ledger + load restoration. But the *part vocabulary is thin* (mostly stick-tier) and it lacks the pieces that make base-building a **destination**.

**The gap to A-rated:** part variety, tiers, interiors, and the build UX that makes players *want* to build.

- [ ] **Part vocabulary**: foundations, pillars, beams, stairs/ramps, **doors + windows** (openable, replicated), fences/gates, roofing variety, angled/triangular pieces. This is the #1 thing separating "shack" from "settlement."
- [ ] **Material tiers**: primitive (sticks/hide) → timber (planks/logs) → wattle-and-daub → stone → brick, each a visual + durability step. Ties into the crafting tree.
- [ ] **Structural integrity** (optional, high-flavor): support/load rules so unsupported spans collapse — a Valheim signature. Scope carefully (fun vs. frustration).
- [ ] **Interiors & furniture**: placeable storage, beds (set "home" for pawns), crafting stations *as* buildings (station-in-building binds to home/workplace per the vision), lighting (torches/hearth), decoration.
- [ ] **Shelter integration**: buildings feed the *existing* multi-axis shelter/exposure model (overhead/wind/enclosure) — a roofed, walled, hearthed room should measurably beat the weather. This makes building *matter* to survival.
- [ ] **Build UX**: material-gather flow, build-progress feedback, deconstruct/refund, snap-preview clarity, "assign a survivor to build this" (ties to Pillar 5).
- [ ] **Building health / decay / repair** and destructibility (combat + weather).

**Interconnections:** parts crafted via Pillar 3 · shelter via weather sim · stations host Pillar 3 crafting · built-by-pawn via Pillar 5 · all placement must be **co-op-replicated** (#162).

---

## 6. Pillar 3 — Crafting & Progression (framework → real)

**Where we are:** 122 recipes across a paleolithic→bronze→iron arc, 6 stations (campfire/workbench/forge/alchemy/kitchen/loom), skill + discovery gating, tool requirements, and a now-complete medical production chain. The authoring loop (MCP `rows set` + `ue.py`) is fast. But the tree has **holes**, quality/durability don't yet *matter*, and the "genetic memory" discovery fantasy is only half-wired.

**The gap to A-rated:** a tech tree that reads as a *journey*, where each tier feels earned and tools/quality matter.

- [ ] **Close the tree's gaps**: audit every station tier for missing intermediate steps; ensure every craftable is reachable from gatherable raws (the medical-chain reconciliation pattern, applied tree-wide via `MO.Test.ValidateData` + the recon script).
- [ ] **Quality & durability that matter**: tool quality affects craft speed/yield; durability drives repair/replace loops; weapon/tool wear (already modeled) surfaced in UI.
- [ ] **Cooking & food depth**: recipes that interact with the metabolism/nutrition sim (SCUM-lite, without the tedium) — cooking improves nutrition/preservation; spoilage.
- [ ] **Clothing & armor crafting**: hide→leather→cloth→armor, feeding warmth (weather sim) and combat mitigation (Pillar 4).
- [ ] **Discovery / "genetic memory"**: fully wire `bRequiresDiscovery` + knowledge so recipes unlock as *remembering* (the sci-fi lore hook), with clear UI for "you can attempt this / you've remembered this."
- [ ] **Teaching & schools** (vision pillar): learn-by-doing (slow) vs. taught-by-pawn (2×) vs. school-maintained trees; skill decay. This is the RimWorld/Kenshi social-progression hook.
- [ ] **Batch crafting + queue UX** polish (queue exists + is replicated).

**Interconnections:** recipes gate on Pillar 5 skills · stations built by Pillar 2 · outputs feed Pillar 2/4 · content authored via the MCP loop · every recipe must pass `ValidateData`.

---

## 7. Pillar 4 — Combat (skeleton → felt)

**Where we are — better than it looks:** `MOCombatComponent` already does **sweep-based hit detection → `BoneNameToBodyPart` → wound application into the anatomy sim** with damage profiles, plus light attack / block / parry / dodge and their Server RPCs. **The realistic-injury differentiator is wired** — a sword hit maps to a body part and produces a real, treatable wound. What's missing is everything that makes it *feel* like combat and gives it *something to fight*.

**The gap to A-rated:** feel, variety, and enemies. This is the pillar with the highest fun-per-hour upside.

- [ ] **Game feel**: hit-stop/impact pause, hit-flash, camera shake, blood/impact VFX, weighty audio, directional hit-reactions. Weightless combat kills the whole game; this is priority one for the pillar.
- [ ] **Movesets & weapon variety**: light/heavy/combo per weapon class (club/axe/spear/knife/sword — items exist), reach/speed/damage-type identity, wind-up/recovery timing that reads.
- [ ] **Defense timing**: parry/block/dodge windows with clear feedback + stamina cost; make the anatomy-aware blocking meaningful.
- [ ] **Enemy combat AI** (the biggest hole): predator creatures that actually hunt/fight (BTs exist), and hostile humanoids/factions (ties to Pillar 5). Without enemies, combat has no purpose.
- [ ] **Ranged**: bow / atlatl / sling (items + `FletchArrows` exist) — aim, draw, projectile, hit → same wound pipeline.
- [ ] **Stamina, hit reactions, death, dismemberment(?), loot-on-death** (permadeath + gear-stays-at-death-location is already the design).
- [ ] **Surface the wound sim as a feature**: the death recap (exists) + a legible wound/treatment UI make "I broke his leg and he bled out" a *story the player sees*, not a hidden number. This is the marketing screenshot.

**Interconnections:** damage → medical sim (done) · weapons/armor from Pillar 3 · enemies from Pillar 5 · fully RPC-driven for co-op (#132/#163).

---

## 8. Pillar 5 — Pawns, AI & Emergent Civilization

**Where we are:** `MOCreatureController` (prey/predator) + `MOSurvivorController` (job queue) over 4 behavior trees and a centralized blackboard; possession system; EQS query *assets pending*; the **colony-management system is fully designed on paper** (`MO57_Colony_Management_Design`, CLAUDE.md) but not built.

**The gap to A-rated:** this is the pillar that turns "survival sandbox" into "your story" — the Kenshi/RimWorld hook the vision is built around. It's mostly greenfield-with-a-design.

- [ ] **Creature AI polish**: finish the pending Deer ABP state machine + montages (rest/sleep/flee/death), predator hunting, prey flocking/fleeing (EQS escape routes), day/night activity — most C++ is done, **Blueprint/anim setup is the blocker**.
- [ ] **EQS assets** (`EQ_FindHarvestableItems`, `EQ_FindHarvestTargets`, `EQ_FindEscapeRoute`): author + wire into BTs (C++ generators/tests exist; add the `MO.AI.RunEQS` runner from Pillar 0 to test them).
- [ ] **Survivor recruitment + job assignment**: find/recruit survivors, assign jobs (gather/craft/build/teach) without possession, bind to home + workplace.
- [ ] **Colony management** (the designed system): `UMOColonyManagerSubsystem` alert tiers, `UMOPersonalityComponent` (Conscientiousness/Sociability/Stability), `UMOCharacterHistoryComponent`, the colony bar + overview + character-card UI. Ship the design.
- [ ] **Pawn needs & autonomy**: eat/sleep/flee instincts + streamlined job routines; morale/relationships; population growth; permadeath + respawn-as-new-pawn loop.
- [ ] **Smart Objects** for interaction points (native UE, per the refactor roadmap).

**Interconnections:** pawns work Pillar 2/3 · fight in Pillar 4 · learn via Pillar 3 teaching · the colony UI is Pillar 6 · **all pawn actions must be co-op-safe**.

---

## 9. Pillar 6 — Game Feel & Presentation (the "looks like a real game" layer)

**Where we are:** audio is furthest along (multi-layer ambient, semantic event groups, weather-reactive); HUD + status moodles + thermal indicator exist; CommonUI layer stack is in. **Animation is the biggest presentation hole** (creature ABPs pending, action anims thin); VFX is thin; the atmosphere plugins (Ultra Dynamic Sky/Weather, Oceanology) are *planned, not integrated*; terrain realism (`VHG_Realistic`) is in progress.

**The gap to A-rated:** this is *the* pillar that decides whether a screenshot looks like a real game. Systems depth is invisible; presentation is all the player sees.

- [ ] **Animation** (highest impact): player locomotion + action anims (chop/mine/craft/attack/hit-react/death/carry), creature ABPs (pending setup), IK/foot-planting, montage-driven interactions. Weightless or T-posing anything reads as "asset flip."
- [ ] **VFX**: impact/blood, dust/debris on dig+build, fire/smoke, water splashes, weather particles, gather feedback (chips fly when you chop).
- [ ] **Atmosphere**: integrate **Ultra Dynamic Sky + Weather** (planned plugins) with the existing clock + weather sim; **Oceanology** water; Lumen/HWRT tuning for the voxel world (#117); cohesive time-of-day lighting.
- [ ] **World cohesion**: finish `VHG_Realistic` terrain (#115), biomes, World Features POIs (springs #122–123, ore veins, caves — #118–121), PCG vegetation density/variety, a consistent material/color palette (art direction).
- [ ] **UI/UX polish**: execute the CommonUI abstraction (the `UMOListMenuBase` / `BindButtonClick` refactor from the UI audit — kill the 5 near-identical menu containers) so menus are *cohesive*; tooltips, transitions, feedback, controller support, consistent iconography.
- [ ] **Camera & controls feel**: responsive camera, smooth transitions, satisfying interaction feedback.
- [ ] **Audio finish**: impact/action SFX to match the ambient layer's quality; music; UI sounds; wire the weather-volume slider (#104).

**Interconnections:** animation drives Pillar 4 feel + Pillar 5 believability · UI serves every pillar · atmosphere sells the world.

---

## 10. Pillar 7 — The Meta-Loop & Progression (sandbox → game)

**Where we are:** quest system exists, tutorial is partial, permadeath + respawn is designed, GUID persistence is solid, the 6 development phases are defined. But the *why-keep-playing* arc isn't yet realized end-to-end.

**The gap to A-rated:** a legible progression from lone survivor to settlement with escalating stakes.

- [ ] **The onboarding arc**: the first 15 minutes teach chop→knap→fire→craft→shelter→eat through the tutorial + quest system, no wiki needed.
- [ ] **The progression spine**: survival → tools → shelter → first survivor → settlement → civilization (CLAUDE.md phases 1–4), each with clear goals + unlocks + a felt power curve.
- [ ] **Escalating threats**: weather severity, predators, resource scarcity, (later) rival factions/raids — a reason the settlement must grow stronger.
- [ ] **Goals & objectives**: quests/milestones that pull players forward without railroading (survival-sandbox-appropriate).
- [ ] **The discovery arc**: the "genetic memory" tech unlocks feel like *remembering*, seeding the sci-fi reveal (late-game/DLC).
- [ ] **Session shape**: satisfying save/quit/resume, co-op session join into an in-progress world.

**Interconnections:** the meta-loop *is* the interconnection of Pillars 2–5 wrapped in goals + feedback.

---

## 11. Pillar 8 — Ship-Readiness (A-rated polish)

**Where we are:** stable (audit campaign), crash reporting + commit-hash-on-title done, build/cook/package pipeline works, multi-save-slot works. The accessibility/localization/store-facing items are pending.

- [ ] **Performance**: frame budget pass, voxel streaming + AI LOD (partly done — freeze/tick-LOD), profile the co-op case.
- [ ] **Accessibility & localization** (#50–53): FText discipline + string tables + CJK fallback; subtitles, UI scale, colorblind, hold/toggle; photo mode; in-game bug report.
- [ ] **Steam store readiness**: store page, capsule/hero art, trailer, **achievements**, Steam Cloud, rich presence, **Workshop/mod support** (the modding-foundation vision pillar — pak mount + load order, #59).
- [ ] **Modding foundation**: the runtime overlay system exists for DataTables; extend to recipes/quests/skills/medical (#114) and expose the C++ mod path.
- [ ] **Telemetry & live-ops hooks**: crash ingestion endpoint wired (#111 done — pick the endpoint), opt-in analytics, replay/highlight buffer for crash repros (#61).

---

## 12. Phasing — How Fable 5 Sequences This

Three milestones, each a shippable checkpoint. **Do not build breadth before the vertical slice proves the loop is fun.**

### Milestone A — "Verified Vertical Slice" (proves the loop + the tooling)
*Pillar 0 fully · one complete loop end-to-end, co-op-verified.*
- Pillar 0 test primitives + 2-client harness live.
- The chop→knap→craft→build-a-shelter→survive-a-night loop works **for two players in PIE**, verified.
- Combat feel pass on the light attack (hit-stop, VFX, audio, hit-react) so one fight *feels* good.
- **Gate:** a co-op pair plays 20 minutes of the core loop and it's fun + never desyncs (in PIE).

### Milestone B — "Steam Playable" (the friend-joins-your-game moment)
*Pillar 1 · breadth on Pillars 2–4 · presentation pass.*
- Steam session/invite live; a friend joins from the overlay into your world.
- Building part vocabulary + tiers; crafting tree gaps closed; combat movesets + first real enemies.
- Animation + VFX + atmosphere pass so it *looks* like a game.
- **Gate:** invite a real friend over Steam, play 90 minutes of build+craft+fight together, host saves, both reload intact.

### Milestone C — "A-Rated" (store-page ready)
*Pillars 5–8 · the settlement fantasy + polish + ship-readiness.*
- Colony/survivor layer; the progression spine + onboarding; escalating threats.
- Full presentation cohesion; accessibility/localization; Steam achievements + Workshop.
- Performance + stability at scale; store page + trailer.
- **Gate:** the seven A-rated outcomes (§1) are all true; a stranger plays 40 hours.

---

## 13. First Three Moves for Fable 5

Concrete, do them in order, each ends in a green `ue.py cycle --test`:

1. **Ship Pillar 0's input + click + AI primitives** in one `cycle` (small C++ in `MOCheatSubsystem`), then wire `ue.py test --auto` to run the 91 dormant automation tests. *You now have real UI/input/AI test rails + a headless gate.*
2. **Build the 2-client harness (#165)** — `MOEditorTestHelper` + `set_pie_players` + `test_multiplayer.py`. *Every MP claim from here is verifiable.*
3. **Stand up the Steam session layer (Pillar 1A)** — `OnlineSubsystemSteam` + `UMOSessionSubsystem` + the invite flow, and get one friend-join working over Spacewar App ID 480. *The single biggest structural unlock; everything co-op depends on it.*

Then drive Milestone A to its fun-gate.

---

## 14. The Bar

MO57 is A-rated when the seven outcomes in §1 are simultaneously true and every pillar has passed the four gates in §2. Until then, the honest status of any system is one of: *framework* (exists), *co-op-correct*, *legible*, *fun*, *verified* — and "done" means all five.

**The framework is built. The campaign made it stable. This session made it fast to iterate. Fable 5's job is to make it a game.**
