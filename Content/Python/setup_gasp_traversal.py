"""GASP phase 2 (Oct 29 2026; Wes: "can we use the hanging/grabbing animation for climbing instead of them just walking up the cliff";
Docs/Animation_GASP.md section 5): the survivor Blueprint's traversal animations.

Sets BP_MOMetaHuman_Female's AMOCharacter HopUpAnim / PullUpAnim / VaultAnim (FMOTraversalAnim) to GASP's traversal sequences (imported by
Tools/import_gasp.py), each one's rise MEASURED here from its root curve rather than typed: where the body leaves the ground (the root 3 % of
the way up) to where it is up on top (97 %), or for a vault, back down on the far side (the last moment the root is 3 % up). (The climb on a
face is Titan Climbing's set on the body, C++ defaults: AMOCharacter ClimbIdleAnim etc.) Compiles and saves the Blueprint. Re-runnable.

    python Tools/ue.py py --file Content/Python/setup_gasp_traversal.py

Prints SETUP_TRAV ok / FAIL lines and every measurement.
"""
import unreal

SURVIVOR = "/MOFramework/Characters/BP_MOMetaHuman_Female"
T = "/Game/Characters/UEFN_Mannequin/Animations/Traversal/"
HOP_UP = T + "Mantle/M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot"
PULL_UP = T + "Climb/M_Neutral_Traversal_Climb_Start_2_5_stand_F_Lfoot"
VAULT = T + "Vault/M_Neutral_Traversal_Vault_1_0_stand_F_Lfoot"
STEPS = 150


def say(ok, msg):
    line = "SETUP_TRAV " + ("ok " if ok else "FAIL ") + msg
    unreal.log_warning(line)
    try:
        out(line)   # the bridge's printer, when run through it
    except NameError:
        pass


def length(seq):
    return seq.get_editor_property("sequence_length") if hasattr(seq, "get_editor_property") else unreal.AnimationLibrary.get_sequence_length(seq)


def root_curve(seq):
    n = length(seq)
    ts = [n * i / STEPS for i in range(STEPS + 1)]
    return ts, [unreal.AnimationLibrary.get_bone_pose_for_time(seq, "root", t, False).translation for t in ts]


def rise(seq, over):
    """The move's span. Up onto something (a hop up, a pull-up): from leaving the ground to the end of the root's whole travel, up AND
    forward -- the height alone is done long before the body is over the edge (GASP's mantle: up in 0.21 s, on top ~1 s later). A vault:
    from leaving the ground to landing on the far side."""
    ts, ps = root_curve(seq)
    zs = [p.z for p in ps]
    lo, hi = zs[0], max(zs)
    if hi - lo < 5.0:
        return None, "the root never rises (%.1f cm): no root motion to measure" % (hi - lo)
    start = next(t for t, z in zip(ts, zs) if z > lo + 0.03 * (hi - lo))
    if over:
        end = [t for t, z in zip(ts, zs) if z > lo + 0.03 * (hi - lo)][-1]   # up and back down on the far side
    else:
        travel = [(p - ps[0]).length() for p in ps]
        end = next(t for t, d in zip(ts, travel) if d >= 0.97 * travel[-1])
    return (start, end), "length %.2f s, root rises %.0f cm, travels %.0f cm, span %.2f -> %.2f s" % (
        ts[-1], hi - lo, (ps[-1] - ps[0]).length(), start, end)


def main():
    bp = unreal.load_asset(SURVIVOR)
    if not bp:
        say(False, "no survivor " + SURVIVOR)
        return
    cdo = unreal.get_default_object(bp.generated_class())
    for prop, path, over in (("hop_up_anim", HOP_UP, False), ("pull_up_anim", PULL_UP, False), ("vault_anim", VAULT, True)):
        seq = unreal.load_asset(path)
        if not seq:
            say(False, prop + ": missing " + path + " (run Tools/import_gasp.py)")
            continue
        span, note = rise(seq, over)
        if not span:
            say(False, prop + ": " + note)
            continue
        anim = unreal.MOTraversalAnim()
        anim.set_editor_property("sequence", seq)
        anim.set_editor_property("rise_start_seconds", span[0])
        anim.set_editor_property("rise_end_seconds", span[1])
        cdo.set_editor_property(prop, anim)
        say(True, "%s: %s: %s" % (prop, path.rsplit("/", 1)[-1], note))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, False):
        say(False, "save failed")
        return
    say(True, "saved " + SURVIVOR)


main()
