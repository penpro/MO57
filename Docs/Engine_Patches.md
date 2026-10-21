# Engine patches

Changes MO57 needs inside Unreal Engine itself, with the evidence, the exact diff, and how to apply / verify / revert / upstream each one.
Patch files and evidence live in `Docs/EnginePatches/`.

## Read this first: the engine at `D:\UnrealEngine\UE_5.8` cannot be patched

That install is the **Epic Games Launcher build** (an *installed* engine): `Engine/Build/InstalledBuild.txt` exists and the root has no
`Setup.bat` / `GenerateProjectFiles.bat`. It ships engine `.cpp` files for reference, but UnrealBuildTool treats installed engine modules as
precompiled and read-only: editing them changes nothing, because `UnrealEditor-Renderer.dll` (and the libraries a packaged game links) are never
rebuilt. (CLAUDE.md called it a "source build" until the 2026-10-17 consolidation fixed it; it is not, as of Oct 16 2026.) Launcher updates (e.g. 5.8.1 -> 5.8.3 on Oct 16) also replace files.

So every patch below is **staged, not applied**. Applying one needs a real source engine:

1. Clone `EpicGames/UnrealEngine` (needs the Epic account linked to GitHub) at the tag matching the launcher version (`5.8.3-release` today).
2. `Setup.bat`, `GenerateProjectFiles.bat`, build `UnrealEditor` Development Win64 (hours, ~100+ GB on disk).
3. `git apply Docs/EnginePatches/<patch>.patch` from the engine root (each patch's header names the file version and hash it was made against).
4. Switch MO57's engine association to the source engine (right-click `MO57.uproject` -> Switch Unreal Engine version), rebuild the project and
   the vendored plugins (Voxel), and update the build path in CLAUDE.md / `Tools/ue.py` config.
5. After EVERY engine update: re-check each patch (`git apply --check`); a hotfix may fix it upstream (then drop it here) or move the code.

## Index

| # | Patch | Status | Upstream |
|---|-------|--------|----------|
| 1 | Hair strands keep their scene depth when Single Layer Water is in view | Staged (no source engine) | Draft ready: `EnginePatches/0001-upstream-report.md`; not yet posted |
| 2 | Nanite shading pipelines outlive a material's shader map swap (uncooked game / editor; N-185) | Design only (no source engine); project workaround `MOMaterialKeepAlive.cpp` in place | Not reported |

---

## #1 — Hair strands missing from scene depth when Single Layer Water is in view

**Files:** `Docs/EnginePatches/0001-hair-depth-with-single-layer-water.patch` (against UE 5.8.3 CL 58210709,
`HairStrandsComposition.cpp` sha256 `19256ea0...d82`), evidence images `0001-evidence-*.png`, upstream text `0001-upstream-report.md`.
No engine file was modified; the original is untouched in the launcher install (the hash above identifies it).

### What the player sees

- **Character creator (Oct 16):** the camera's depth of field blurs the MetaHuman's buns exactly as much as the far background, while the face,
  fringe and crown are sharp. (`0001-evidence-aperture-does-not-fix.png`: f/2 left, f/3.2 right — the aperture only sharpens the crown.)
- **Underwater (Oct 11, memory `water-trace-channel-underwater-and-hair`):** strands over the water read the far water depth and the water fog
  replaces them — the buns "vanish".
- Anything else that reads scene depth behind hair (height fog, translucency depth tests) is wrong the same way whenever water is in view.

### Evidence (`0001-evidence-creator-probe.png`)

One creator instance on seed 26364 (`scratchpad hair_dof_probe.py`), four screenshots of the same frame:
A baseline — buns blurred; **B `r.DepthOfFieldQuality 0`** — buns sharp and solid (the blur is depth of field);
**C `ShowFlag.VisualizeBuffer 1` + `r.BufferVisualizationTarget SceneDepth`** — a *bald* head silhouette: the hair has no depth at all, so the
pixels where the buns are drawn carry the background's depth; D `r.HairStrands.Strands 0` — bald (the hair is strands, not cards, at this distance).

### Cause (UE 5.8.3 source)

1. A Single Layer Water primitive relevant to the view (our Oceanology ocean and lakes, the Electric Dreams mud puddle material) makes the renderer run
   the SLW depth prepass (`r.Water.SingleLayer.DepthPrepass` = 1, required for Virtual Shadow Maps on water — `RenderUtils.cpp`
   `IsSingleLayerWaterDepthPrepassEnabled`).
2. `SingleLayerWaterRendering.cpp` ~1440, `RenderSingleLayerWater`: `RenderHairCompositionForSLW(..., SceneDepthTexture)` composes hair
   (`EHairSLWCompositionType::All`) and writes hair depth into the **original** scene depth.
3. `DeferredShadingRenderer.cpp` ~3883, right after: `SceneTextures.Depth = SingleLayerWaterPrePassResult->DepthPrepassTexture;` — the prepass
   texture was rendered before any hair. The hair depth from step 2 is gone.
4. `HairStrandsComposition.cpp` 346-349, `AddHairVisibilityComposeSamplePass`, main composition (`InFrontOfWater`): `bReadSLWDepth` is true and
   `const bool bWriteDepth = !bReadSLWDepth;` skips the depth write, on the assumption in the comment above it ("the hair depth has already be
   written into the scene depth") — true for the texture step 2 wrote, false for the one post-processing reads after step 3.

### The fix

```cpp
// HairStrandsComposition.cpp, AddHairVisibilityComposeSamplePass
const bool bWriteDepth = true; // was: !bReadSLWDepth
```

Why it is safe: with `bWriteDepth` the pass uses the depth state it already uses when no water is in view (`CF_DepthNearOrEqual` + write,
`InternalCommonDrawPass`), now against the post-swap prepass depth, which holds the opaque scene *and* the water. Hair behind the water writes nothing:
the `PERMUTATION_READ_SLW_DEPTH` path discards those samples (`bSkipComposition`) before any output, and the depth test would reject them anyway.
The `ReadSLWDepth + WriteDepth` shader permutation already exists (no `ShouldCompilePermutation` filter), so no new shader code.
Not build-tested (no source engine) — the diagnosis is verified by the SceneDepth visualization; the fix is reasoned from the code.

### How to verify once applied

0. Turn the creator's depth of field back on first: `bPreviewDepthOfField = true` (MOCharacterCreatorPanel.h default, or the WBP class default).
1. `python Tools/ue.py inst start x --creator`, `MO.NewGame 26364`, wait for readiness Ready.
2. Screenshot: the buns are as sharp as the crown (compare `0001-evidence-creator-probe.png` A).
3. `ShowFlag.VisualizeBuffer 1` + `r.BufferVisualizationTarget SceneDepth`: the silhouette includes the hair.
4. Hair behind water stays hidden: stand the pawn chest-deep and view from the shore — no strands drawn over the water surface they are behind.
5. The Oct 11 underwater case: dive with the camera under water — the buns stay visible.
6. `python Tools/ue.py auto --filter MOFramework` all pass; package once (the game target links the patched Renderer).

### How to revert

`git apply -R Docs/EnginePatches/0001-hair-depth-with-single-layer-water.patch` in the engine root and rebuild `UnrealEditor` (or re-sync the file
from the matching engine tag).

### Workarounds tried without an engine change (none fixes it)

| Tried | Result |
|-------|--------|
| **Creator depth of field OFF (`UMOCharacterCreatorPanel::bPreviewDepthOfField = false`) — CURRENT STATE (Wes, Oct 16)** | Buns sharp (no blur at all); the world building behind the creator is visible, which Wes accepted. When Epic fixes the bug (or patch #1 is applied), set `bPreviewDepthOfField` true and check the buns |
| Creator aperture f/2 -> f/3.2 (`PreviewFStop`, kept for when DOF returns) | Crown and fringe sharper; buns unchanged (they take the background's depth, not a near one) |
| `DepthOfFieldUseHairDepth` on the creator camera | No change; reverted. NB its tooltip is inverted vs the 5.8.3 code: ON writes the coverage-lerped hair/scene depth (`DOFDepthPS`), OFF writes the hair depth |
| `r.HairStrands.ComposeAfterTranslucency` | Changes draw order only, not the depth write |
| `r.HairStrands.DOFDepth 0` (forum advice) | Does not exist in 5.8 |
| `r.Water.SingleLayer.DepthPrepass 0` | **Creator DOF case: FIXES it** (Oct 16, `0001-evidence-prepass-off-restores-hair-depth.png`: the buns appear in SceneDepth and stay sharp) — which also proves the cause is the prepass swap. Underwater case: did not help (Oct 11). Cost: read-only cvar (ini, restart), and the engine says the prepass is "necessary for proper Virtual Shadow Maps support" on water. **REJECTED (Wes, Oct 16): with it off the Oceanology water "looks horrible, it completely turns off the water motion"** — seen live on the main-menu flyover. The prepass stays on |
| Groom custom depth, SceneDepthWithoutWater copy, hair cards, HW raster | Underwater case: did not help (Oct 11) |

### Upstream

Nobody online has identified this (searched Oct 16: forum threads report "DOF doesn't work properly on MetaHuman hair" without a cause; no Epic
issue, no 5.8.x hotfix note). Report text: `EnginePatches/0001-upstream-report.md`. Post it in the forum category *Issues and Bug Reporting
(Unreal Engine)* <https://forums.unrealengine.com/c/general/issues-and-bug-reporting-unreal-engine/243> and file it through the Bug Submission Form
linked from <https://www.unrealengine.com/support/report-a-bug>. Record the thread / issue number here when filed.

---

## #2 — Nanite shading pipelines outlive a material's shader map swap (uncooked game and editor)

**Files:** none yet: a staged design, no diff (no source engine to build or test it). The workaround that runs today is project code:
`Plugins/MOFramework/Source/MOFramework/Private/MOMaterialKeepAlive.cpp` (`mo.Debug.MaterialKeepAlive`, BACKLOG N-185).

### What we see

Only in UNCOOKED game processes (the harness instances: `UnrealEditor.exe MO57.uproject -game`): the joiner or the host dies in
`Nanite::BasePass` within about a second of new content spawning for the first time in that process (rock spires 9 km out, a cliff
assembly 3 km out). Three signatures, one cause: an access violation EXECUTING a heap address (ending a24e00 / 224e00) with D3D12RHI frames
(the parallel translate); with `r.RHICmd.ParallelTranslate.Enable 0` the ensure "Invalid uniform buffer <FLocalVertexFactoryUniformShaderParameters
or FViewUniformShaderParameters> bound on ComputeShader at index 3" (D3D12Commands.cpp:52, the bound buffer's memory reused by a newer
buffer of any type) then a fatal in RHIUniformBufferUtilities.h:167; once an access violation reading 0x4 in a Renderer task. About 60 % of
the 20-minute repro runs crashed (Oct 20-21). A cooked build (Wes's Shipping playtests) has never shown it.

### Cause (UE 5.8.3 source)

1. `r.ShaderCompiler.JobCacheDDC` (default on, read-only) makes an uncooked game skip shader compilation on material PostLoad and compile
   on demand (`ShaderCompilerJobCache.cpp` `IsShaderJobCacheDDCEnabled`, true for `IsRunningGame()`). Materials first rendered late in the
   session (the spires' foliage and rock instances, the voxel terrain's layer materials) get their shaders in pieces over the next seconds.
2. Each arrival: `FShaderCompilingManager::ProcessCompiledShaderMaps` -> `FMaterial::SetShaderMapsOnMaterialResources` ->
   `SetShaderMapsOnMaterialResources_RenderThread` (MaterialShared.cpp ~5730): the material's rendering-thread shader map is replaced (the
   old one is released) and every proxy using it runs `CacheUniformExpressions(RHICmdList, true)`, which drops the old uniform buffer
   (`InvalidateUniformExpressionCache(true)`: "only ... if the calling code is using FMaterialUpdateContext to recreate the rendering state
   of primitives using this material ... cached mesh commands also cache uniform buffer pointers").
3. The game thread then marks the primitives it finds for re-creation (`PropagateMaterialChangesToPrimitives` ->
   `FObjectCacheContext::GetPrimitivesAffectedByMaterials`, which skips a component whose render state is not created at that moment).
4. Nanite builds a primitive's shading pipeline once, when the primitive is added (`NaniteDrawList.cpp` ~360,
   `ShadingPipelines.Register(MaterialSectionData.TriangleShadingPipeline)`), with RAW pointers: the compute shader of the shader map of
   that moment, its shader bindings, `FNaniteShadingPipeline::MaterialUB` (NaniteShading.cpp ~1188). A primitive missed in step 3 keeps a
   pipeline whose shaders and uniform buffer were freed in step 2, and the next base pass binds / executes them.

Evidence that such pipelines exist: the Voxel plugin's material selection found two registered Nanite shading pipelines with different
compute shaders for ONE material proxy (Oct 21, its ensure in `VoxelNaniteMaterialRendererImpl.cpp`). The engine already has a validator for
the uniform-buffer half: `r.Nanite.Debug.ValidateShadingBinUB` ("stale MaterialUB"), bindless shading only.

### The fix (design)

In `SetShaderMapsOnMaterialResources_RenderThread`, after the proxies are updated, re-cache the Nanite shading pipelines of EVERY scene
primitive that uses an updated material, on the render thread, instead of relying on the game thread finding them: for each `FScene`, for
each `FPrimitiveSceneInfo` whose Nanite material sections reference a proxy whose `GetMaterialNoFallback()` is in `MaterialsToUpdate`,
remove and re-add its Nanite draw commands (the path `FScene::UpdateCachedRenderStates` takes). Cheaper alternative: keep the old shader
map and uniform buffer referenced by the pipeline (a `TRefCountPtr<FMaterialShaderMap>` and an `FUniformBufferRHIRef` in
`FNaniteShadingPipeline`) so a stale pipeline renders with the old shaders until its primitive is re-created; that is what the project
workaround does from outside.

### Workaround in place (project code)

`MOMaterialKeepAlive.cpp`: in an uncooked game process only (never the editor or PIE, never cooked), a render-thread scan every frame holds
a reference to every material proxy's uniform buffers and every FMaterial's rendering-thread shader map, and keeps each replaced one alive
for the rest of the session (released at engine pre-exit). Cost measured on the 20-minute repro: ~1 500 shader maps and ~5 000 buffers
kept, instance memory unchanged (~14-15 GB either way), scan worst 1.9 ms. Evidence (interleaved A/B on one binary, Oct 21, the Voxel
plugin's selection pass already fixed in both arms): keep-alive ON 4 of 4 runs clean, OFF 1 of 4 crashed with the original signature; before either fix ~60 % of runs crashed (10 of 16).

### How to verify once applied

`-ini:Engine:[ConsoleVariables]:mo.Debug.MaterialKeepAlive=0` on both harness instances (the workaround off), then the N-185 repro (the
`nettest actions` cases up to `_far_remote_pawn_9km`, seed 670711536) 8 times: no crash folder. With the engine fixed, delete the workaround.

### Upstream

Not reported yet. The report needs: uncooked `-game`, Nanite content first rendered late, on-demand shader compile; the voxel ensure as the
direct evidence of two pipelines per material.
