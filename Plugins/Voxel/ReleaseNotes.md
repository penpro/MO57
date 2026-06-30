# Release Notes

## Unreleased

- [4a23b78fa] Scatter meshes: instance transforms are now stored relative to their chunk, keeping float precision (no jitter) for meshes scattered far from the world origin.

- [04ec90e58] World origin rebasing: PCG nodes (query, sample, projection, scatter, call graph) now sample the voxel layers in absolute space, so they stay aligned with the terrain after a shift. (Runtime *partitioned* PCG generation is still subject to UE's own origin-shift limitations.)

- [9a3a1d018] World origin rebasing: voxel debug draws (sculpt bounds, stamp/layer trees) now render in the correct place after a shift.

- [c37b69f15] Fixed a crash when changing maps with a Voxel Scatter actor present (scatter manager lookup on an already-torn-down world).

- [e05fcb7ac] World origin rebasing: fixed sculpting (volume & height) and stamp rendering being offset after a world origin shift. Stamps now refresh their cached transform when the world origin moves, and the sculpt tools and data pipeline run in absolute (origin-independent) space, so edits stay aligned with the terrain.

- [2f6742ad0] Fixed a crash in the Voxel Sculpt mode panel when the selected sculpt actor was garbage-collected (the actor picker could read freed memory).

- [b8581c9b9] World origin rebasing: fixed the voxel world, stamps and scatter breaking when the world origin shifts. Terrain/stamp generation and scatter now run in absolute (origin-independent) space so they stay stable across shifts, and voxel components are refreshed on a shift so chunks are no longer incorrectly culled until they re-mesh.

- [53b59b063] Fixed a TShadowDepthPSCombined shader map crash on UE 5.8 when using a MegaMaterial (eg one with no materials). MegaMaterials will recompile once.

- [3c3bc2c3e] Metadata overrides: fixed the value field disappearing after picking a metadata asset.

- [d104d03b2] Fixed Voxel forcing the legacy transform gizmo on UE 5.8: the always-on selection mode no longer disables the new TRS gizmo.

- [56050342c] Fixed voxel meshes not showing up in ray tracing / Lumen (hardware ray tracing) on UE 5.8.

- [60686762b] VoxelCharacter: fix spurious "should inherit from VoxelCharacter" error on UE 5.8.

- [7f1fb26d9] Debug draws: fixed an unbounded accumulation of empty debug draw groups (one per computed state) that could reach hundreds of thousands on long-running servers, stalling the game thread on the debug drawer lock. Empty groups are now pruned as states advance.

- [392b523a3] VoxelStampComponentBase: added a Blueprint-callable `GetStampBounds(bWorldSpace, OutBounds)` that returns the stamp bounds in local or world space. If the stamp runtime isn't already resolved it is created on the fly.

- [6c24e4997] VoxelFunctionLibrary: replaced the C# UnrealHeaderTool plugin with a template-based `VOXEL_REGISTER_FUNCTION(Class, Function)` macro. Every `UFUNCTION` on a `UVoxelFunctionLibrary` subclass must now have a matching `VOXEL_REGISTER_FUNCTION` line in its `.cpp`; a non-shipping startup check logs and ensures any missing ones.

- [54b524535] Surface types: when a save load references a surface type that isn't loaded, the error now names the specific asset path so it's clear which asset to hard-reference (e.g. from your GameInstance). Existing surface type assets must be re-saved once for the AssetRegistry GUID tag to be available.

- [247f7bcb8] Metadata: same actionable error message when a save load references a metadata asset that isn't loaded. Existing metadata assets must be re-saved once for the AssetRegistry GUID tag to be available.

- [aeb8a82d7] Mesher: snap dual-contouring vertex to the saddle face on 6:2 face-diagonal cells, removing sliver fin triangles.

  ![Saddle fin fix](ReleaseNotes/saddle-fin-fix.png)

- [56c5aa3fa] Tools: new right-click → Debug meshing viewer.

  ![Debug meshing](ReleaseNotes/debug-meshing.png)
