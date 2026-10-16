# Upstream report draft — hair strands missing from scene depth when Single Layer Water is in view (UE 5.8.3)

Ready to paste. Post it in the forum category **Issues and Bug Reporting (Unreal Engine)**
(<https://forums.unrealengine.com/c/general/issues-and-bug-reporting-unreal-engine/243>) and file the same text through Epic's
**Unreal Engine Bug Submission Form** (linked from <https://www.unrealengine.com/support/report-a-bug>), which is what Epic tracks.
Attach `0001-evidence-creator-probe.png`, `0001-evidence-prepass-off-restores-hair-depth.png` and
`0001-hair-depth-with-single-layer-water.patch` from this folder.

---

**Title:** [5.8.3] Groom hair strands are missing from SceneDepth whenever Single Layer Water is in view (DOF blurs hair like the background) — cause + one-line fix

**Engine:** 5.8.3 (CL 58210709), Windows, D3D12 SM6, Substrate on, Virtual Shadow Maps on (so `r.Water.SingleLayer.DepthPrepass` is active).

**Symptom.** Any groom (MetaHuman hair) seen while a Single Layer Water primitive is relevant to the view has no depth in the scene depth buffer that
post-processing reads. With cinematic depth of field, strands outside the head silhouette (buns, loose hair) take the depth of whatever is behind them
and blur exactly as much as the far background, while hair in front of the scalp looks fine (the scalp is at nearly the same depth). Changing the
aperture does not help; `DepthOfFieldUseHairDepth` does not help. Underwater, the same gap makes strands over water read the far water depth so the
water fog replaces them.

**Repro.**
1. Level with any Single Layer Water body (an ocean or lake far in the background is enough) and a MetaHuman with a strands groom.
2. Camera on the character at ~2.6 m, cinematic DOF focused on the character (f/2–f/3.2).
3. Observe hair outside the head silhouette fully blurred. `r.DepthOfFieldQuality 0` → sharp.
4. `ShowFlag.VisualizeBuffer 1` + `r.BufferVisualizationTarget SceneDepth` → the character shows as a bald silhouette: no hair depth at all.
5. Restart with `r.Water.SingleLayer.DepthPrepass=0` (ini): the hair now appears in SceneDepth and DOF keeps it sharp — so the SLW depth prepass
   path is what drops it (attached: prepass on vs off, DOF and SceneDepth).

**Cause (5.8.3 source).**
- `SingleLayerWaterRendering.cpp`, `RenderSingleLayerWater`: when a SLW depth prepass exists, `RenderHairCompositionForSLW` composes hair with
  `EHairSLWCompositionType::All` and writes hair depth into `SceneDepthTexture` (the original scene depth).
- `DeferredShadingRenderer.cpp`, right after `RenderSingleLayerWater`: `SceneTextures.Depth = SingleLayerWaterPrePassResult->DepthPrepassTexture;`
  — the prepass texture was rendered before any hair, so the hair depth written above is dropped.
- `HairStrandsComposition.cpp`, `AddHairVisibilityComposeSamplePass` (main composition, `InFrontOfWater`): `bReadSLWDepth` is true, and
  `const bool bWriteDepth = !bReadSLWDepth;` skips the depth write on the assumption ("the hair depth has already be written into the scene depth")
  that no longer holds after the swap.

**Fix (one line, `HairStrandsComposition.cpp` ~line 349):** `const bool bWriteDepth = true;` — the main composition then writes hair depth into the
post-swap depth with the same `CF_DepthNearOrEqual` + write state it uses when no water is in view. Hair behind the water is still excluded: the
`PERMUTATION_READ_SLW_DEPTH` path discards samples behind the SLW depth before anything is written, and the depth test runs against the prepass
depth, which contains the water. The `ReadSLWDepth + WriteDepth` permutation already exists (no `ShouldCompilePermutation` filter). Patch attached.

We have not been able to build-test the fix ourselves (launcher build); the diagnosis is verified by the SceneDepth visualization above.
