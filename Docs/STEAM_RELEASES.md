# Steam releases (patch-note anchor)

> **What this is:** the date, time and commit of each Steam patch, so the next patch notes can be generated from `git log <last>..HEAD`.
> Wes, 2026-10-17: "so long as you can extract steam update patch notes from git commits we don't need it [PATCH_NOTES.md], might just need
> to note when the last steam patch was every time i ask for steam patch notes so you have a date/time though". Procedure:
> `Docs/TOOLING.md` §10. The old `PATCH_NOTES.md` (frozen at 2026-06-30) is archived at `Docs/Archive/PATCH_NOTES.md`.

## Steam patches (newest last)

| Date / time (PT) | Commit | Notes generated from | Who recorded |
|---|---|---|---|
| 2026-10-16 (built 00:21; upload time not recorded) | `0573ab31` | (before this log) | Claude, from Wes 2026-10-17: "Yes, the last Steam upload was 0573ab31 (Oct 16)" |
| 2026-10-17 22:59 (built 22:49; recorded Oct 28 from SteamPipe's `depot_build_1602811.log` and the build stamp in the uploaded exe) | `afb9aabc` | `0573ab31..2ea93fc9` (the draft below) | Claude, Oct 28. A DEVELOPMENT build, and it carried this machine's `MO57\Saved` (settings at 2560 x 1440, a save, a log: TRAPS §17) |
| 2026-10-28 21:38-21:40 (BuildID 26289075; the Shipping exe built 16:42; recorded Oct 30 from SteamPipe's `app_build_1602810.log` / `depot_build_1602811.log` and the staged package's dates) | `11bc6111` (the exe's own stamp, `Build.Commit` in its Oct 30 bug reports; first recorded as `2121b981`, which was committed at 17:04, after the 16:42 exe build, so its save carry-over may or may not be in it) | `afb9aabc..2121b981` (no draft written) | Claude, Oct 30, after Wes asked about the fire drill and Harvey's climbing on Steam: this build has neither the fire drill fix nor the climbing animation (both Oct 29), nor anything after `2121b981` |

## Known Shipping builds Wes played over Steam (not confirmed as Steam uploads; for orientation only)

| Build commit | Commit time (PT) | Evidence |
|---|---|---|
| `ef1c84d9` | 2026-10-13 19:37 | the Oct 14 playtest (29 reports): `Docs/Archive/BugReports_2026-10-14.md` |
| `0573ab31` | 2026-10-16 00:21 | the Oct 16 Steam co-op session (21 reports): `Docs/Archive/BugReports_2026-10-16.md`. **Confirmed as the last Steam upload** (Wes, Oct 17) |

When Wes next asks for Steam patch notes, ask which commit the last Steam patch was built from if it is not recorded above, then record it here.

## How MO57 goes to Steam (Oct 28)

App 1602810, depot 1602811 (SteamPipe GUI, `D:\SteamWorksSDK\sdk\tools\SteamPipeGUI`; scripts in `...\ContentBuilder\scripts`): the depot's
content root is `D:\CurrentMO57SteamPackage`, whose `Windows64content` folder is the staged `Windows` folder of a package, with the
launcher stub also there as `Origins.exe`. The root also still holds an older `Windows` folder (Sep 20) that goes up with every build.
Package Shipping fresh (`BuildCookRun -clientconfig=Shipping ... -stagingdirectory=Saved\StagedBuilds_Ship`), copy it into
`Windows64content` with no `Saved` folder, and never run the game from there. Before the cook: the Voxel shader hooks in place (TRAPS §2, Oct 28); while today's Fab update
breaks editor compiles, add `-nocompileeditor` (or `-skipbuild` when the binaries are current) and keep Fab enabled.

## Draft: Oct 17 patch (shipped with `afb9aabc`)

- Generated from `git log 0573ab31..2ea93fc9` (feat / fix commits; docs, tests and dev tooling left out).
- **A build packaged from the working tree also contains uncommitted work** (Codex's batches A-F, BACKLOG N-001; torch wear,
  N-002), which these notes do not cover. Commit those first, or add their lines to the notes.
- When it is uploaded: add a row above with the upload's date / time and the commit it was built from.

```text
[h2]Co-op fixes, real caves and geology, torches, safer saves[/h2]

[h3]Existing worlds[/h3]
[list]
[*]The world generator changed (new caves and geology). Worlds saved on the previous version load with the new terrain, so buildings and dug areas may not line up with the ground. Start a new world for the best experience.
[*]Each world now keeps a single save. Extra save slots from earlier versions show up as separate entries in the list.
[/list]

[h3]Co-op[/h3]
[list]
[*]Joining players can jog and sprint properly. The server was pulling them back to walking speed, which caused the heavy rubber banding.
[*]Inventory, dropping, picking up and butchering now work for joining players.
[*]Dropped items fall to the ground for everyone instead of hanging in the air.
[*]Interact (E) finds small items lying in grass or on slopes.
[*]Right-clicking another player's character no longer takes it away from them.
[*]The host can take over a character a departed player left behind.
[*]Finished stick floors and roofs no longer show glowing green boxes, and unfinished ones look unfinished.
[*]Joining players now wait on the loading screen, with a progress bar, while their world is built, instead of watching it appear.
[*]Bug reports now include connection statistics.
[/list]

[h3]World[/h3]
[list]
[*]Lakes are no longer frozen in place.
[*]Caves rebuilt: level galleries you enter from hillsides, found only in limestone, never flooded or near the shore, and dark inside.
[*]Digging follows real geology: bedrock, ore bodies and ore outcrops. What you get depends on the rock you dig, and any ore can be mined with stone tools.
[*]Harder rock and minerals take longer to dig.
[*]Large landmarks sit on the ground: no more floating rocks or trees, and no more white orbs.
[*]Fixed trees floating above the ground.
[*]Swimming starts and stops at about 1 m of water.
[/list]

[h3]Light[/h3]
[list]
[*]New: the torch. Craft it, light it at a fire and carry it at your side. It goes out in water.
[*]Your eyes adjust to the light of torches and campfires, and caves are properly dark.
[/list]

[h3]Survival and crafting[/h3]
[list]
[*]Eating takes time.
[*]Punches go where you aim.
[*]Discovery: inspecting materials teaches you new techniques, and the clay line leads to your first metal. 126 of 136 recipes can now be reached (was 55).
[*]Ten items have real models and icons: stone maul, birch-bark container, copper, tin and bronze ingots, wrought iron and steel bars, copper prills, birch tar and hide glue.
[*]New icons for coal and the bedrock stones.
[/list]

[h3]Saving[/h3]
[list]
[*]Leaving a world always saves it, whether you exit to the menu, quit, or close the window. There are no more "unsaved progress" prompts.
[*]Each world has one save. Save writes to your world's save, and Load lists your other worlds.
[*]Loading a world after your last survivor died gives you a new survivor instead of an empty camera.
[/list]

[h3]Audio[/h3]
[list]
[*]Fixed a random loud buzz, and silence until you clicked.
[/list]
```
