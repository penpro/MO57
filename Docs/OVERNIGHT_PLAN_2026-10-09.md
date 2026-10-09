# Overnight plan — 2026-10-09 (draft for approval)

Goal: make the co-op + packaged build trustworthy, in an order where every step is verifiable without Wes. Each item has a **done-when**
and a **control** (a check I have seen fail, so a PASS means something). Small clear bugs get fixed; anything bigger is logged with a
diagnosis, not patched blind. Progress goes to `Docs/OVERNIGHT_LOG.md` under "## Overnight 2026-10-09".

## The list (in this order)

0. **Housekeeping** — only if approved: commit + push today's UI work (thumbnail capture fix, readable text fields, save-tile layout) in two
   chunks (C++ + test; asset + spec). Verify the staged column first; never `git add -A`; `.claude/settings.local.json` stays out.

1. **`ue.py nettest packaged`** — automate the packaged-build smoke I have done by hand five times: (optional) package, host via
   `-ExecCmds="MO.Session.Host ..."`, join a second copy with `MO57.exe 127.0.0.1`, then assert from the two logs: no Fatal / no ensure, same
   seed on both, client clock + sky follow the host, the client's spawn manager is silent while the host's runs, a non-blank save thumbnail.
   *Done when* it PASSes on the current package. *Control:* host started with `MO.WorldSync.Withhold 1` must make it FAIL on the clock check.
   (It would have caught the intro-timer crash, the client-side wolves and the blank thumbnail.)

2. **Client trust-boundary tests** (extends `nettest actions`; the audit's open H17 / H19 / H22 family). From the CLIENT process, send requests the
   server must refuse: place a building far out of reach / inside terrain, harvest a node out of reach, drop or craft with items it does not
   own, attack out of range, take a pawn another player controls. Each has a legitimate twin that must be ACCEPTED (control).
   *Done when* every refusal is observed on the host. Small holes (e.g. `ServerPlaceBuilding` trusting the client transform: reach + collision
   recheck) get fixed with the test; bigger ones are logged.

3. **Join/leave churn + concurrent actions** (`nettest churn`): host + 2 clients if RAM/VRAM allow (checked first with `nvidia-smi`; falls back to
   1 client). N rounds of leave/rejoin; both clients grab the same dropped item on the same tick (exactly one must get it — no duplication);
   host saves while both are connected and reloads. Asserts pawn counts, no fall-through rescues, no Error-level lines. Fix small, log big.

4. **Audit burn-down, up to 4 items** from `Docs/PROJECT_STATUS.md` "still open", each re-verified by reading the code first (the tracker has
   been stale before), each with a regression test, e.g. the verified use-after-free read in `UMOGameUIManagerSubsystem::NotifyPlayerRemoved`.
   Anything that is a design fork (H36 offline crafting) is skipped and listed.

5. **Weather through the bridge only** (your preference) — INVESTIGATION + spec, no Blueprint edits, native UDS replication stays ON.
   Reflect over the Ultra Dynamic Weather actor and `BP_WeatherBridge` to find what a client can legally call; write the exact minimal
   Blueprint change into `Docs/`. If a C++-only client apply exists, prototype it behind a default-OFF CVar with a nettest showing a client
   follows the host with native replication switched off. *Done when* there is a recommendation + evidence.

6. **Terraform unit 3 (excavation) — design and task breakdown only**, from your binding design (designation-based, pawn-automated dig / dump /
   flatten zones, spoil-as-material, haul job, dump-destination popup). Written to `Docs/` for your checkpoint. **Nothing is built.**

7. **Morning package** — full battery on the final binaries (`ue.py auto` + `nettest game|hostsave|features|actions|packaged|churn`), the
   Development package refreshed and boot-smoked, `python -m graphify update .`, memory notes, a summary at the top of the log, and (if approved)
   commit + push in logical chunks.

## Rules (carried over)
Editor closed to build (if you have it open I do non-build work and note it) · never touch your saves (Harper_Wright-01, Test158, v1gate); my own
test saves are `zz_*` and deleted · stop only processes I launched, by recorded pid · local/free only · two failed patches in a row = stop and
instrument · negative results are reported as negative · I do **not** edit your Blueprints, switch off native sky replication, or build the
colonist-ground-far-from-players fork · real-window checks (computer-use, `MO57.exe` only) are allowed while you sleep.

## Not in this plan
Real two-account Steam sessions (needs a second person) · the `BP_WeatherBridge` edit itself · colonist ground (design fork).
