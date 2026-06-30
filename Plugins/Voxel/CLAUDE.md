# Project Instructions

- When committing, use one liners (-m)
- Don't forget to add migrations as needed in BaseVoxel.ini
- Add migration notes for users in MigrationNotes.txt
- `VoxelMinimal.h` already includes everything under `VoxelMinimal/` — don't re-include those individually.
- After every commit, ask whether to add a `ReleaseNotes.md` entry (even if the user often ignores). Entries always start with `[commit hash]`, so they must be written after committing.
- In `switch` statements, put `default: check(false);` at the top (above the real cases), with no `return`/`break` — fall through into the first case for shipping safety.
- `pre_commit_check.py` enforces: no UTF-8 BOM in any file, and no non-ASCII bytes outside `.md`. Installed as `.git/hooks/pre-commit` by `claude.py`. Run `python pre_commit_check.py` to audit all tracked files.
- To build: resolve the host `.uproject` in `../../`, resolve the engine from its `EngineAssociation` (launcher `UE_<ver>` or `HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds\<guid>`), then run `<Engine>\Engine\Build\BatchFiles\Build.bat <ProjectName>Editor Win64 DebugGame -Project=<uproject> -WaitMutex`. Live Coding active = editor open; ask before killing it.

Project structure:

@CLAUDE.overview.generated.md