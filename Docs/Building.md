# Building (placement, construction, build state)

> **Covers:** placing a building (ghost), depositing materials, constructing, cancel / refund, how a building looks in each state,
> and the co-op rules. Terraforming and excavation: `Docs/Terraform_Excavation_Plan.md`. The building system is deliberately custom
> (weighted build parts; no native alternative: `Docs/TECHNICAL_REFERENCE.md` "Systems Confirmed Custom").
> **Current state (2026-10-17):** construction is server-authoritative (Oct 13); placement goes through one server path; a building
> looks pale-white "planned" until complete (Oct 16, Wes kept it); a new player can place a building (the Oct 15 aim-trace blocker
> is fixed); the first campfire completes end to end.
> **Entry points:** `UMOBuildingComponent` (`RequestBuildAction` -> Server RPC `ServerBuildRequest`, `EMOBuildRequest`, 15 m reach;
> `TryPlaceGhost` -> `ServerPlaceBuilding`), `UMOBuildProgressComponent` (authority-gated gather / start / cancel-refund; replicates
> `FMOBuildRepState`), `AMOBuildableActor` (`RefreshBuildStateVisual`, `PlannedColor` / `PlannedGlow`),
> `AMOPlayerController::ForwardUIContractRequest` -> `ClientUIContractRequest`, `UMOInteractionSubsystem::IsWithinInteractReach`.
> **Tests:** `MO.Test.Build`, `nettest actions` steps `_client_build`, `_client_building_look`. **Open items:** `Docs/BACKLOG.md` §
> Building and terraform. **Decisions:** DECISIONS 2026-10-13 (server-driven), 2026-10-16 (pale ghosts), F7 (cancel / refund semantics).

## 1. Server-driven construction (Wes, Oct 13: "co-op build for sure needs to be server/host driven"; 757e47c1)

- Players reach construction only through `UMOBuildingComponent::RequestBuildAction` (a Server RPC from a client, 15 m reach) with an
  `EMOBuildRequest`. `UMOBuildProgressComponent` mutators refuse without authority; it owns gather / start / cancel-refund and
  replicates `FMOBuildRepState` (a TMap ledger flattened to a sorted array); clients re-broadcast its delegates.
- **UI asked of a remote controller:** interaction handlers run on the server, so a UI request made on the server's copy of a joiner's
  controller opened nowhere. `AMOPlayerController::ForwardUIContractRequest` -> `ClientUIContractRequest` sends it to the owning client
  (build menu, containers, crafting, station menu, butchering). **A new `IMOUIContractInterface` request must start with
  `ForwardUIContractRequest`.**
- **Rule:** a new player action = a request through a controller-owned component RPC, with the logic in an authority-gated component;
  never in a widget. Test new co-op verbs with a `nettest actions` step that checks on the HOST and includes a "direct client call
  refused" control.

## 2. Placement and the ghost

- One path: `TryPlaceGhost -> ServerPlaceBuilding` (in place on the authority); the placement preview never replicates. Before Oct 16
  the authority PROMOTED its own preview into the building (graybox PreviewMesh + ghost MID carried over), which gave host-only green boxes.
- The aim trace from the shoulder camera must reach the ground within reach: before Oct 15 it never did within 10 m, so the ghost
  floated invalid, and its tint wrote a "Color" parameter `M_BuildPreview` does not have (invalid looked green). **The co-op build test
  passed throughout because it places through a server call, not the ghost: test the real input path.**
- Residual audit note (July): `ServerPlaceBuilding` trusted the client FTransform with no reach / collision recheck (H19 residual);
  recheck against current code before closing (BACKLOG).

## 3. How a building looks

`AMOBuildableActor::RefreshBuildStateVisual` is the one look per build state: every mesh in the ghost MID is tinted `PlannedColor` /
`PlannedGlow` until Complete (Wes, Oct 16: pale-white ghosts stay). Unbuilt floors used to look finished, so "fell through the floor"
reports were unbuilt plans.

## 4. Using a finished station

E on a finished station opens its station menu: the server checks the client's claimed target with
`UMOInteractionSubsystem::IsWithinInteractReach` (one reach rule for E and right-click). Before Oct 15 the server re-derived the target
with its own 12 cm sweep vs the client's 30 cm and refused. Interaction targeting in general: `Docs/Coop_And_Networking.md` §5.

## 5. First campfire, end to end (verified Oct 15)

Place -> leave -> walk up -> Add Materials -> Build -> 30 s; then light it (`Docs/Crafting_And_Progression.md` §2). The site context
menu's kit spec is `Content/Python/ui_specs/kit_build_context.py`.

## 6. Saves

Loaded buildings repopulate their recipe BuildParts (a July fix: restore once left materials vacuously satisfied = free build).
Deposited build materials are part of the save payload. Persistence patterns: `Docs/Persistence.md`.
