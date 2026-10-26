# Engineering Principles (full text)

> **Current state:** the binding engineering rules for MO57, moved verbatim from `CLAUDE.md` on 2026-10-17 (that file now
> carries a one-line summary of each and points here). Nothing in the wording of Principles 1-11, the tedium section or the
> "too slow" ladder was changed. The last section adds the counter-example and the verification lessons that had lived
> only in agent memory.
> **Read when:** before fixing any bug, adding behaviour to several systems, or adding any "instant / auto / fast-X" mode.
> **Related:** `Docs/PAUSE_POLICY.md` (no pause, ever), `Docs/VISION.md` (the realism pillar in context), `Docs/DECISIONS.md`.

## The rules (MANDATORY — read every session)

**This project is targeting production-ready software. The user has explicitly
called out a pattern of patching symptoms instead of systems and will reject
work that does it. These rules override convenience and override speed.**

### 1. Trace bugs to the layer that creates the bad state, not where it surfaces

When a bug is reported, the first impulse is to fix the visible failure.
Resist it. Walk the call stack backward to the layer that produces the
incorrect state, and fix the bug there.

**Concrete example from this codebase:** A "Inventory full!" notification was
firing falsely. The visible failure was the `CanAddItem` check, but the
actual bug was that `FMOCraftResult` stored depletion failures and inventory
failures in the same `FailedItems` map. The UI couldn't distinguish them, so
it always showed "Inventory Full!" — even when the problem was a depleted
node. Patching `CanAddItem` would have masked the symptom and left the
classification bug to cause future false positives elsewhere.

**Rule:** Before editing, ask "where does the wrong value first appear?"
If you're editing further downstream than that, you're patching a symptom.

### 2. If N systems need the same behavior, build one abstraction — not N copies

When asked to add a behavior to multiple systems (e.g. "moving interrupts
building AND inspection AND gathering"), the wrong move is N parallel edits.
The right move is one shared mechanism (interface, delegate, base class) that
each system plugs into.

**Concrete example from this codebase:** Movement interruption is implemented
as `IMOMovementInterruptibleInterface` + a registration list on `AMOCharacter`.
A new interruptible action (lockpicking, surgery, fishing) plugs in by
implementing the interface and calling `RegisterMovementInterruptible` — no
changes to `AMOCharacter`, no coupling between systems.

**Rule:** If your patch touches N existing files to add behavior B, you've
probably built B in the wrong place. Build it once, register N times.

### 3. A comment claiming behavior doesn't make it true — verify

Documentation drifts. Aspirational comments survive. "Uses click-outside" in
`MOContextMenuBase` was wrong for 2+ years — the click-outside handler was
never implemented. Anyone who trusted the comment had a broken UX.

**Rule:** When a comment describes load-bearing behavior, confirm the code
matches. If it doesn't, fix the code OR fix the comment — never just the
caller that depends on the lie.

### 4. Read the whole flow before changing one node

A single function in a chain rarely has enough context to fix correctly.
Read up to the caller, down to what's called, and across to similar
functions in the same file. The CanAddItem bug had THREE related functions
(`CanAddItem(FGuid)`, `CanAddItemByDefinitionId`, `AddItemByGuid`) plus the
caller path — reading only one was why the first three patches missed.

**Rule:** Before editing a function, read its caller, its callee(s), and
its siblings.

### 5. When the cause is unclear, add diagnostic logging — don't guess and patch

If you've made two patches without certainty, stop. The next patch will
probably also miss. Instrument the suspect layer with enough logging to
prove what's happening, build, have the user reproduce, then read the data.
This is faster than three more guess-patches.

**Rule:** Two failed patches in a row = stop patching, start instrumenting.

### 6. Failure modes must be distinguishable at the data layer

If a struct lumps "kind A failure" and "kind B failure" into the same field,
every consumer downstream has to guess what to do — and most will guess
wrong. Distinguish failures where they're produced. The UI's job is to
display, not to disambiguate.

**Rule:** When a function can fail for multiple reasons, the return value
must carry enough information to tell them apart. Don't make callers guess.

### 7. Defaults are policy

Default values in headers propagate to every subclass that doesn't override
them. `bCloseOnMouseLeave = false` in the base meant every menu was broken
until someone explicitly opted in. The base's default should be the behavior
most subclasses want, with explicit overrides for exceptions.

**Rule:** When you set a default, ask "will most code want this?" If not,
change the default — don't leave it to every subclass to remember to opt in.

### 8. "It compiles" is not "it works"

Build success tells you the types match. It tells you nothing about whether
the logic is correct. Always provide a concrete reproduction step the user
can run, and verify behavior — not just compilation.

**Rule:** Don't claim a fix is done because the build succeeded. Describe
exactly how to test the behavior, and what the expected vs. failure output
looks like.

### 9. Production-ready means extension without modification

If adding a new resource type means editing 5 files, the abstraction is
wrong. If adding a new interruptible action means editing `AMOCharacter`,
the abstraction is wrong. New features should plug in, not require surgery.

**Rule:** When you finish a feature, ask: "to add the next instance of this
pattern, how many existing files would I touch?" If the answer is more than 1
(the new code itself), refactor first.

### 10. Diagnose before designing the fix

Don't propose a solution until you can explain the bug in one sentence
that names the responsible layer. "The inventory check is wrong" is a
symptom. "Depletion failures share a map with inventory failures, so the UI
can't tell them apart" is a diagnosis. Only the diagnosis tells you where
the fix belongs.

**Rule:** If you can't write a one-sentence diagnosis naming the
responsible layer, you don't understand the bug yet.

### 11. The simulation IS the design — every system reflects real-world phenomena

MO57's first design pillar is realism. Every system in the game — timers,
movement, combat, biology, crafting, healing, travel, weather, resource
gathering, AI behavior, social interaction — models the real-world
phenomenon it represents. Convenience shortcuts that bypass the
simulation are a design failure, not a polish step. They make a
different game.

This rule sits above the others because it determines what "correct"
means. The other principles ask "is this code right?". This one asks
"is this game right?" — and the answer drives what's allowed to ship.

**Concrete examples already in this codebase:**
- **Terraforming** runs a 5-second progress timer with
  interrupt-on-movement. Better tools shorten it; the floor is
  non-zero. A swing of the pickaxe is a timed action, not an instant
  click.
- **Wounds** bleed, scab, and scar over real game time via the medical
  cascade. No HP regen — healing is the actual recovery arc with risks
  (infection) and dependencies (clean water, suturing skill).
- **Vitals** (HR, BP, SpO2, glucose, blood volume, temperature) tick
  on real intervals. Cold doesn't just "do damage" — it shifts the
  thermal-comfort moodle, which affects metabolism, which affects
  glucose, which affects mental state.
- **Crafting** consumes materials AND time AND skill. There is no
  "click and it's done."
- **Movement-interruptible actions** register via
  `IMOMovementInterruptibleInterface`. Moving cancels lockpicking,
  surgery, terraforming, fishing — because in reality you can't do
  those things while walking.
- **AI sleep cycles** are real-clock — frozen mobs stay asleep until
  the recheck timer fires, and players can't run up on them in the
  meantime because the recheck distance is meters of travel away.

**Examples to reject, no matter how convenient they sound:**
- "Instant build / instant dig / instant craft" modes for shipped
  builds (cheat console commands for debug are a separate question).
  The one exception is a CREATIVE world (Wes, Oct 26 2026: "in creative
  mode make it so building happens instantly"): a world rule chosen when
  the world is created, through `FMOWorldRules::IsInstant` only -- never
  a skip inside a survival world
- "HP regen over time" — use the actual healing simulation
- "Fast travel" that teleports between distant points — build roads,
  ride horses, take the journey
- Passive auto-pickup (items walk into inventory as you near them
  without input) — but see the QoL section below for the version
  that's fine
- "Skill use is free" — every skill use costs time, energy, material,
  or all three
- Hidden internal timers the player can't see while waiting — if
  something takes 30 seconds, the player must see it taking 30 seconds
- Stat decay / regen rates that read as game-y rather than biological
  ("hunger decreases 1/sec" is wrong; "metabolism burns X kcal/min
  based on activity level + body composition" is right)

### Tedium is not simulation — eliminate the first, preserve the second

Critical distinction:

- **Simulation friction** is the in-world thing taking real time.
  Cooking a stew takes 20 minutes because that's how long stew takes.
  Walking to the well takes 90 seconds because the well is 90 seconds
  away. This is sacred — see the reject list above.
- **UX tedium** is the player making the same gesture N times for the
  same simulated outcome. Clicking each of 80 berries individually to
  move them into a barrel — the *gesture* is tedious; the *simulation*
  is fine with 80 berries changing hands in N seconds.

Quality-of-life features that batch repeated gestures into one action,
with one combined-duration timer, **respect both principles at once**.
Reference example: Project Zomboid has timers on every individual
pickup / drop / equip — realistic, but a "deposit all matching" action
would massively improve the play experience without breaking realism.
Total time is still ~N × per-item duration; player just made one
gesture instead of 80.

**Good QoL patterns that don't break realism:**
- **"Deposit all matching"** / **"Take all"** — one combined-duration
  timer covering all transferred items. Total time stays realistic;
  player makes one gesture. Slight efficiency bonus is acceptable
  (real handfuls are faster than one-at-a-time).
- **Recipe queuing** — enqueue 5 meals; each cooks in sequence at its
  real duration. Player doesn't click "start next" 5 times.
- **Auto-sort inventory by category** — instant; mental organization
  is fast.
- **Press-button-to-sweep pickup** — timed action: press F, character
  spends 4 seconds picking up everything in 3m. Visible, interruptible,
  totally fine. This is the *good* version of "auto-pickup."
- **Stack / unstack** items — instant; it's a UI shuffle, not a
  world event.
- **Workshop crafting batch** — set a station to "craft 20 arrows";
  the station works for 20 × craft-duration with the player free to
  do other things, just like real fletching.

**The killer feature: delegate tedium to AI pawns.** Tedium that can't
be batched (because each instance has its own simulation cost — chop
12 trees in 12 different places) is delegate-able to recruited
survivors via `UMOSurvivorJobQueueComponent`. The player doesn't skip
the simulation; **the simulation happens without the player.** Hauling,
watering crops, restocking firewood, churning butter, processing hides,
basic crafting, tending livestock — all real-time tasks the pawn AI
does on its own clock while the player is elsewhere. This is the loop
that takes MO57 from "Project Zomboid" to "RimWorld." Every system that
the player can do should be plumbed to be pawn-assignable.

### When a system feels too slow, the right moves are (in order):
1. **Tedium check first** — is the player making repeated gestures
   with no per-gesture choice? Batch them or offer as a pawn job.
2. Better feedback so the wait feels productive — progress bar,
   incremental yield, visible environmental change, audio cues.
3. Tool / skill progression that meaningfully reduces but doesn't
   eliminate the duration.
4. Re-check whether the simulation duration was correct — tune down
   toward the realistic floor if the original value was excessive, but
   never below the floor.
5. Add parallel work the player can do while waiting (drink, eat,
   sharpen a tool, plan the next move).

Never collapse a single simulated action's duration to zero, add a
"creative mode" toggle to shipped builds, or hide the realistic timer
behind an animation that "skips" the wait.

**Rule:** Before adding any "instant" / "auto" / "fast-X" mode, ask
three questions in order:
1. *Does the simulation need tuning?* → Tune it.
2. *Is the player skipping the simulation?* → Refuse.
3. *Is this batching repeated gestures with no per-gesture choice?* →
   Allow it, with a combined-duration timer covering the batched
   work — or expose it as an assignable pawn job. Or both.


---

## Examples and lessons that were only in agent memory (added 2026-10-17)

**Counter-pattern (called out by Wes, recurs often enough to memorize):** editing 3 close handlers in 3 controllers to add the
same "restore input mode" logic is wrong. Build one `RestoreAppropriateInputMode()` on the base controller and call it from
each handler. (Principle 2.)

**More templates from this codebase:**
- *Distinguishing failure modes:* `FMOCraftResult` separates `FailedItems` (real inventory rejection) from `DepletedItems`
  (node exhausted) so the UI shows the right message. (Principle 6.)
- *Click-outside in the base class:* `FMOContextMenuClickOutsideHandler` registered in `UMOContextMenuBase::NativeConstruct`;
  every context menu subclass gets click-outside-to-close for free. (Principles 2, 3, 7.)
- *Default policy for a class that "must be assigned in BP defaults":* the death recap's `DeathRecapWidgetClass` was never
  assigned, so a dead last pawn was a silent soft-lock; `ResolveDeathRecapClass()` now defaults it. (Principle 7.)

**Verification lessons (each one cost a wrong fix or a wasted night):**
- **LOOK before theorising.** "ZERO PCG in game" was a coastal spawn on bare sand, found by one screenshot after hours of
  reading Shipping-only code paths. For a visual report, reproduce the player's exact view (seed, camera) first.
- **A control must be able to fail.** "Client pawn z == server z" passed with no client terrain at all; a first-hit trace
  "agreed" at the trace origin. Prove a check can fail (e.g. `nettest game --skew-seed`) before trusting its PASS.
- **Run the probe again on the fixed build, and read both machines.** The host-pawn bug had a second cause (client-side
  gravity on simulated proxies) that only showed after the first fix.
- **Audit the fixes, not just the original code.** Two findings of the July 1 audit were in fixes shipped earlier the same
  campaign.
- **Re-verify tracker claims before implementing.** About 14 of 20 audited defects re-checked in July were already fixed or
  refuted; verify an agent's reachability claim at the layer that creates the state.
- **Measure before redesigning.** The interaction aim grid (Oct 16) and the PCG census (Oct 14) each overturned the obvious
  design change.
- **Identify the exact asset before fixing it.** "The small green bush" was the Black Locust variants, not the saplings that
  got the first (wrong) fix. Enumerate candidates with measured properties; say which one you picked and why.
- **An intermittent failure that moves with the seed is a world-dependent bug.** Replay the seed (`MO.NewGame <seed>`) before
  blaming the test.
