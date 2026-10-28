"""GASP phase 1 (Oct 27 2026; Docs/Animation_GASP.md): the survivor Blueprint drives Epic's Game Animation Sample locomotion.

1. Points BP_MOMetaHuman_Female's locomotion assets (AMOCharacter: LocomotionDriverMesh / LocomotionDriverAnimClass / BodyRetargeter) at the
   imported GASP content: the UEFN mannequin, its CMC motion-matching blueprint, the mannequin -> MetaHuman retarget.
2. Implements GASP's pawn interface (BPI_SandboxCharacter_Pawn) and writes its Get_PropertiesForAnimation from our movement: GASP's own
   implementation (SandboxCharacter_CMC) with the gait, the landing and the floor read from AMOCharacter (MO|Animation getters) instead of its
   input-state variables.
3. Compiles and saves. Re-runnable: the graph is rewritten each time.

    python Tools/ue.py py --file Content/Python/setup_gasp_survivor.py

Prints SETUP_GASP ok / FAIL lines.
"""
import unreal

SURVIVOR = "/MOFramework/Characters/BP_MOMetaHuman_Female"
DRIVER_MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"
DRIVER_ABP = "/Game/Blueprints/SandboxCharacter_CMC_ABP.SandboxCharacter_CMC_ABP_C"
RETARGETER = "/Game/MetaHumans/Common/Common/Rigs/RTG_UEFN_to_Metahuman_nrw.RTG_UEFN_to_Metahuman_nrw"
PAWN_INTERFACE = "/Game/Blueprints/BPI_SandboxCharacter_Pawn.BPI_SandboxCharacter_Pawn_C"
HOLD_ANIM = "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop"   # the driver under a body montage

# S_CharacterPropertiesForAnimation / S_PlayerInputState pin names (user-defined struct members carry their GUIDs). Enum values are GASP's:
# E_MovementMode OnGround NewEnumerator4 / InAir NewEnumerator5 (falling and swimming, as the sample maps them); E_Stance stand 0 / crouch 1;
# E_RotationMode orient-to-movement 0 / strafe 1; E_Gait walk 0 / run 1 / sprint 2.
GRAPH = """
(fn Get_PropertiesForAnimation ()
  (bind move (Variables|Character|GetCharacterMovement))
  (bind sprint ({MO}IsAnimGaitSprint))
  (bind walk ({MO}IsAnimGaitWalk))
  (bind orient (Class|CharacterMovementComponent|GetOrientRotationtoMovement :self move))
  (bind crouched (AI|Components|NavMovement|IsCrouching :self move))
  (bind airborne (or (AI|Components|NavMovement|IsFalling :self move) (AI|Components|NavMovement|IsSwimming :self move)))
  (return (Utilities|Struct|MakeSCharacterPropertiesforAnimation
    :InputState_32_04D997924898849E2B907292D8F9311E (Utilities|Struct|MakeSPlayerInputState
      :WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936 sprint
      :WantsToWalk_3_78963B154975E3BDE244A3BE438473E3 walk
      :WantsToStrafe_5_64E842184F7691CD179DFCBA7158A4D9 (not orient)
      :WantsToCrouch_9_47D0791142F36BB93FFD9EA12F96F2B9 crouched)
    :MovementMode_2_5EAAF2DE40EEFFC0CEB680A055920D1F (select airborne (Utilities|Enum|LiteralenumE_MovementMode "NewEnumerator5") (Utilities|Enum|LiteralenumE_MovementMode "NewEnumerator4"))
    :Stance_9_1D5931DB4C5B00938B77A7B2C5B74DD6 (select crouched (Utilities|Enum|LiteralenumE_Stance "NewEnumerator1") (Utilities|Enum|LiteralenumE_Stance "NewEnumerator0"))
    :RotationMode_5_09FB56C54CBB02C7BA2238AF05967717 (select orient (Utilities|Enum|LiteralenumE_RotationMode "NewEnumerator0") (Utilities|Enum|LiteralenumE_RotationMode "NewEnumerator1"))
    :Gait_12_F5E602A54C3D796AEF369F97F8840DFA (select sprint (Utilities|Enum|LiteralenumE_Gait "NewEnumerator2") (select walk (Utilities|Enum|LiteralenumE_Gait "NewEnumerator0") (Utilities|Enum|LiteralenumE_Gait "NewEnumerator1")))
    :ActorTransform_16_F865FEEB42A03F0C9D1B7F8D29ACD19F (Transformation|GetActorTransform)
    :Velocity_19_E4BA1C9D46E4A6BAC5DD63A4A1A93BE0 (Transformation|GetVelocity)
    :InputAcceleration_21_5C93A8BB46763C31AC835FA9A3C9FE88 (Pawn|Components|CharacterMovement|GetCurrentAcceleration :self move)
    :CurrentMaxAcceleration_24_A51F19184F2BACEE0F3F85A780F122D6 (Pawn|Components|CharacterMovement|GetMaxAcceleration :self move)
    :CurrentMaxDeceleration_26_F675EA284CC1A5123C84449BAA351B9A (Class|CharacterMovementComponent|GetBrakingDecelerationWalking :self move)
    :OrientationIntent_49_38FB872B46ADBB0ED766758157174BB1 (Transformation|GetActorRotation)
    :AimingRotation_29_9817B3384AC40E67F1E5E39238ADEEEA (Pawn|GetBaseAimRotation)
    :JustLanded_35_E546D46D4B451FBD122127908CFF5899 ({MO}HasJustLanded)
    :LandVelocity_40_AD6ED73D49EE6F5998ECD7ACB2D5D7EB ({MO}GetLandVelocity)
    :GroundNormal_58_F7B6873B4C61B0EC85BAA2B8F1340DDA ({MO}GetAnimGroundNormal)
    :GroundLocation_62_08A0EA404C60B46793D1378218DC1180 ({MO}GetAnimGroundLocation))))
"""


def say(ok, msg):
    unreal.log_warning("SETUP_GASP " + ("ok " if ok else "FAIL ") + msg)
    try:
        out("SETUP_GASP " + ("ok " if ok else "FAIL ") + msg)   # the bridge's printer, when run through it
    except NameError:
        pass


def main():
    from editor_toolset.toolsets.blueprint import BlueprintTools as BT

    bp = unreal.load_asset(SURVIVOR)
    mesh = unreal.load_asset(DRIVER_MESH)
    abp = unreal.load_object(None, DRIVER_ABP)
    rtg = unreal.load_asset(RETARGETER)
    bpi = unreal.load_object(None, PAWN_INTERFACE)
    for name, obj in (("survivor", bp), ("driver mesh", mesh), ("driver blueprint", abp), ("retargeter", rtg), ("pawn interface", bpi)):
        if not obj:
            say(False, name + " missing (run Tools/import_gasp.py first)")
            return

    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("locomotion_driver_mesh", mesh)
    cdo.set_editor_property("locomotion_driver_anim_class", abp)
    cdo.set_editor_property("body_retargeter", rtg)
    hold = unreal.load_asset(HOLD_ANIM)
    if hold:
        cdo.set_editor_property("locomotion_driver_hold_anim", hold)
    else:
        say(False, "no hold anim " + HOLD_ANIM)
    say(True, "locomotion assets: " + str(cdo.get_editor_property("locomotion_driver_mesh")) + " / " + str(cdo.get_editor_property("locomotion_driver_anim_class"))
        + " / " + str(cdo.get_editor_property("body_retargeter")))

    if not unreal.MOEditorContentUtils.implement_interface(bp, bpi):
        say(False, "could not implement BPI_SandboxCharacter_Pawn")
        return
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)

    graph = BT.get_graph(bp, "Get_PropertiesForAnimation")
    if not graph:
        say(False, "no Get_PropertiesForAnimation graph after implementing the interface")
        return
    # The node type ids of our getters (their category path)
    eg = unreal.BlueprintEditorLibrary.find_event_graph(bp)
    ids = {}
    for fn in ("IsAnimGaitSprint", "IsAnimGaitWalk", "HasJustLanded", "GetLandVelocity", "GetAnimGroundNormal", "GetAnimGroundLocation"):
        found = [str(t) for t in BT.find_node_types(eg, fn)]
        exact = [t for t in found if t == "MO|Animation|" + fn]   # not GASP's own variable of the same name (Class|SandboxCharacterCMCABP|...)
        if not exact:
            say(False, "no node type for " + fn + ": " + str(found))
            return
        ids[fn] = exact[0]
    code = GRAPH
    for fn, tid in ids.items():
        code = code.replace("({MO}" + fn + ")", "(" + tid + ")")
    BT.write_graph_dsl(graph, code)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    readback = BT.read_graph_dsl(graph)
    if "SandboxCharacterCMCABP" in readback or "MovementAnalysis" in readback:
        say(False, "graph reads GASP's animation blueprint instead of the pawn: " + readback[:3000])
        return
    missing = [p for p in ("MakeSCharacterPropertiesforAnimation", "MakeSPlayerInputState", "IsAnimGaitSprint", "HasJustLanded", "GetAnimGroundNormal",
                           "GetCurrentAcceleration", "GetBaseAimRotation") if p not in readback]
    if missing:
        say(False, "graph readback lacks " + str(missing) + "\n" + readback[:3000])
        return
    say(True, "Get_PropertiesForAnimation written:\n" + readback[:6000])
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, False):
        say(False, "save failed")
        return
    say(True, "saved " + SURVIVOR)


main()
