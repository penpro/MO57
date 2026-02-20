# MO57 Patch Notes

This file tracks changes, bug fixes, and new features. Updated incrementally to preserve history across development sessions.

---

## [2026-02-19] Terrain-Aware Spawning & PCG Optimization Tools

### New Features

**Beach Spawn System**
- Characters now spawn on beaches instead of mountain peaks
- Configurable height range filters spawn locations to terrain between 100-3000cm above water level
- Algorithm searches expanding rings to find the lowest valid terrain
- Fallback system prefers lower elevations when no ideal beach is found

**PCG HISM Tools** *(Editor/PIE only)*
- Added `UMOHISMCullingSubsystem` - World subsystem that periodically refreshes tagged PCG actors
- Added `MO Force HISM Tree Build` PCG node - Forces HISM tree rebuilding after mesh spawning
- Tag-based filtering allows selective refresh of specific PCG volumes (e.g., "FarTreesPCG")

### Improvements

**MOGameMode Configuration**
- New `MaxSpawnHeightAboveWater` property (default 3000cm) - defines beach ceiling
- New `MinSpawnHeightAboveWater` property (default 100cm) - defines beach floor
- Improved spawn search logging for debugging terrain detection
- Voxel seed integration documented for procedural world generation

### Bug Fixes

- Fixed spawn algorithm preferring highest terrain instead of lowest
- Fixed search loop exiting on first land hit instead of continuing to find beaches
- Fixed fallback spawn using mountain peaks instead of lowest available terrain
- Fixed crafting menu retaining station name and fuel display after closing (now properly resets to "Hand Crafting")
- Fixed voxel terrain not regenerating when loading a saved game (world seed now persisted and applied on load)
- Added `GetWorldSeedAsVoxelString()` Blueprint function for Voxel graphs (returns 8-char A-Z format)

### Technical Notes

- PCG HISM refresh features use editor-only APIs and will not function in packaged builds
- For runtime distance culling, use Voxel Plugin's scatter system with RenderDistance nodes or UE's built-in shadow distance settings
- `InstanceMinDrawDistance` on HISM components does not work as documented - this is a known UE limitation

---

## [2026-02-18] Critical Audit Fixes

### Bug Fixes

- Fixed logging and debug code issues identified in code audit
- Removed marketplace assets and build artifacts from git tracking
- Enabled Git LFS for large binary files
- Added large asset directories to .gitignore

---

## Template for Future Entries

```
## [YYYY-MM-DD] Title

### New Features
- Feature description

### Improvements
- Improvement description

### Bug Fixes
- Bug fix description

### Technical Notes
- Any important technical context
```
