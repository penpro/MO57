"""The character creator's survivor bases, as a player meets them (Oct 27; MOSurvivorBase.h, Docs/UI.md): one -game instance with the
creator open over a new world (seed 777). Real presses where a player clicks -- Slate's hit test on the Base arrows and a build slider, a
click on them -- and the body read back after each (MO.Test.BodyShape): Lillian's meshes at her own body, the build slider's shape, the
simulated body fat and muscle on body and face; then Begin, her own hair kept on its strands in game (the Blueprint's LOD bias, read back,
through a round trip to the femme: bald in third person without it), the merged animals, a save and a reload that keep the base and the
build. Screenshots in Saved/Screenshots/creator_bases/ for Wes's eye (6_in_game and 7_femme_round_trip: hair at the third-person distance).

    python Tools/check_creator_bases.py [--keep]

Prints each step and "CREATOR BASES PASS" / "FAIL". The editor must be closed (the instance builds its own world).
"""
import glob
import os
import re
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "crebases"
SHOTS_OUT = os.path.join(ue.ROOT, "Saved", "Screenshots", "creator_bases")


def screenshot(label):
    shots = os.path.join(ue.ROOT, "Saved", "Screenshots")
    before = set(glob.glob(os.path.join(shots, "**", "*.png"), recursive=True))
    ui.console(NAME, "shot showui")
    deadline = time.time() + 30
    new = []
    while time.time() < deadline and not new:
        new = sorted(set(glob.glob(os.path.join(shots, "**", "*.png"), recursive=True)) - before)
        time.sleep(0.5)
    if not new:
        return None
    time.sleep(1.0)
    os.makedirs(SHOTS_OUT, exist_ok=True)
    path = os.path.join(SHOTS_OUT, label + ".png")
    shutil.move(new[-1], path)
    return path


def query(verb, pattern, timeout=8):
    off = ui.log_size(NAME)
    ui.console(NAME, verb)
    m = ui.wait_log(NAME, pattern, since=off, timeout=timeout)
    return m.group(0) if m else ""


FRAMING_RE = re.compile(r"\[MOCharacterCreator\] Framing \S+ face: looks along (-?[\d.]+) deg \(actor (-?[\d.]+)\).*")


def framing(since):
    """(the face yaw the creator last framed, the preview camera's yaw, the framing log line) -- the camera sits in front of the face when
    its yaw is the face's + 180."""
    lines = FRAMING_RE.findall(ui.read_log_from(NAME, since))
    code, out = ui.call(NAME, ["py", "-c", "pc = unreal.GameplayStatics.get_player_controller(world, 0); v = pc.get_view_target(); "
                               "out('CAMYAW %.1f %s' % (v.get_actor_rotation().yaw, v.get_class().get_name()))"], timeout=30)
    cam = re.search(r"CAMYAW (-?[\d.]+) (\S+)", out)
    face = float(lines[-1][0]) if lines else None
    return face, (float(cam.group(1)) if cam else None), (cam.group(2) if cam else "")


def faces_camera(face, cam):
    return face is not None and cam is not None and abs((cam - (face + 180.0) + 180.0) % 360.0 - 180.0) < 5.0


def hair_state():
    """The player's Hair groom: (groom name, the LOD bias of each of its groups, its materials' own names)."""
    code, out = ui.call(NAME, ["py", "-c", (
        "p = unreal.GameplayStatics.get_player_pawn(world, 0); h = [c for c in p.get_components_by_class(unreal.GroomComponent) if c.get_name() == 'Hair'][0]; "
        "out('HAIRSTATE %s|%s|%s' % (h.groom_asset.get_name(), ','.join(str(d.get_editor_property('lod_bias')) for d in h.get_editor_property('groom_groups_desc')), "
        "','.join((m.get_editor_property('parent') or m).get_name() for m in h.get_materials() if m)))")], timeout=60)
    line = next((l for l in out.splitlines() if l.startswith("HAIRSTATE ")), "HAIRSTATE ||")
    groom, bias, mats = line[len("HAIRSTATE "):].split("|", 2)
    return groom, [float(b) for b in bias.split(",") if b], mats


def main(keep=False):
    rep = ui.Report()
    try:
        ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1600, 900), nosteam=True)
        if not rep.step("the instance boots", ui.wait_ready(NAME, 300)):
            return False
        ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.NewGame 777")   # the dev verb for a new world (`boot` drives the editor's Play, not an instance)
        opened = ui.wait_log(NAME, r"Character creator opened", since=off, timeout=420)
        if not rep.step("a new game opens the character creator", bool(opened)):
            return False
        time.sleep(5)
        mine = ui._bodyshape(NAME, True)
        rep.step("the survivor starts on its Blueprint's own body (no base)", bool(mine) and mine[0] in ("", "None") and "SKM_MH_F_1_BodyMesh" in mine[3], str(mine)[:300])
        screenshot("1_start")

        hit = query("MO.Test.HitTest BaseNextButton", r"HITTEST '[^']*BaseNextButton[^']*' at .*")
        rep.step("a real click on the Base '>' reaches it", "REACHES it" in hit, hit[:200])
        off = ui.log_size(NAME)
        query("MO.Test.ClickWidget BaseNextButton", r"ClickWidget: '[^']*BaseNextButton")
        time.sleep(4)   # her meshes load, the grooms swap
        # Wes, Oct 27: a swap "sometimes pivots the camera to the back of the character" -- every swap re-finds the face.
        face, cam, kind = framing(off)
        rep.step("after the swap the camera re-finds her face (framed again, in front of where the face looks)",
                 kind == "CameraActor" and faces_camera(face, cam), f"face {face} camera {cam} ({kind})")
        mine = ui._bodyshape(NAME, True)
        rep.step("'>' puts Lillian on: her body and face, at her own body (12.4 % / 28.5 kg), no shape on",
                 bool(mine) and mine[0] == "Lillian" and abs(mine[1] - 12.4) < 0.2 and abs(mine[2] - 28.5) < 0.2 and "SKM_MH_Lillian_Base_BodyMesh" in mine[3]
                 and "SKM_MH_Lillian_Base_FaceMesh" in mine[3] and ui._weight(mine[3], "CharacterMesh0", "Heavy") == 0.0
                 and ui._weight(mine[3], "CharacterMesh0", "Thin") == 0.0, str(mine)[:400])
        screenshot("2_lillian")
        # The Hair style row names her own updo at once (Oct 27: it named the femme's braids, still drawn while her updo loaded).
        label = query("MO.Test.FindWidget HairStyleText", r"\[MOQUERY\] WIDGET 'HairStyleText' .*")
        rep.step("the Hair style row names her own updo ('Updo (short)'), not the groom it replaces", "text='Updo (short)'" in label, label[:200])

        # The survivor turned round behind the framing's back (a load's re-grounding, a new pose): the camera follows the face within a second.
        off = ui.log_size(NAME)
        ui.call(NAME, ["py", "-c", "p = unreal.GameplayStatics.get_player_pawn(world, 0); pc = unreal.GameplayStatics.get_player_controller(world, 0); "
                       "r = p.get_actor_rotation(); p.set_actor_rotation(unreal.Rotator(0.0, r.yaw + 180.0, 0.0), False); "
                       "c = pc.get_control_rotation(); pc.set_control_rotation(unreal.Rotator(c.pitch, c.yaw + 180.0, 0.0)); out('TURNED')"], timeout=30)
        time.sleep(2.0)
        turned, cam, kind = framing(off)
        rep.step("the survivor turned round: the camera comes back in front of her face",
                 turned is not None and face is not None and abs((turned - face + 180.0) % 360.0 - 180.0) > 90.0 and faces_camera(turned, cam),
                 f"face {face} -> {turned}, camera {cam}")
        screenshot("2b_turned_round")

        hit = query("MO.Test.HitTest BustSlider", r"HITTEST '[^']*BustSlider[^']*' at .*")
        rep.step("a real click on the Bust slider reaches it", "REACHES it" in hit, hit[:200])
        query("MO.Test.ClickWidget BustSlider at=0.9", r"ClickWidget: '[^']*BustSlider")
        time.sleep(1.5)
        mine = ui._bodyshape(NAME, True)
        bust = ui._weight(mine[3], "CharacterMesh0", "BustLarge") if mine else -1
        rep.step("a press 90 % along the Bust slider (-1..1) grows the bust (BustLarge ~0.8)", 0.6 < bust <= 1.0, f"BustLarge {bust:.2f}")
        screenshot("3_bust")

        query("MO.Test.BodyComposition 40", r"\[MOTEST\] BodyComposition.*")
        time.sleep(3.5)
        mine = ui._bodyshape(NAME, True)
        rep.step("40 % body fat: the heavy shape full on, body and face", bool(mine) and ui._weight(mine[3], "CharacterMesh0", "Heavy") == 1.0
                 and ui._weight(mine[3], "FaceMesh", "Heavy") == 1.0, str(mine)[:400])
        screenshot("4_heavy_40")
        query("MO.Test.BodyComposition 8 20", r"\[MOTEST\] BodyComposition.*")
        time.sleep(3.5)
        mine = ui._bodyshape(NAME, True)
        rep.step("8 % and 20 kg of muscle: thin and wasted full on", bool(mine) and ui._weight(mine[3], "CharacterMesh0", "Thin") == 1.0
                 and ui._weight(mine[3], "CharacterMesh0", "Wasted") == 1.0, str(mine)[:400])
        screenshot("5_thin_8")
        query("MO.Test.BodyComposition 12.4 28.5", r"\[MOTEST\] BodyComposition.*")
        time.sleep(3.5)

        query("MO.Test.ClickWidget BeginButton", r"ClickWidget: '[^']*BeginButton")
        began = ui.wait_log(NAME, r"\[MOCharacterCreator\] Begin: .*", since=0, timeout=20)
        rep.step("Begin closes the creator with the base and the build", bool(began) and "base 'Lillian'" in began.group(0)
                 and bool(re.search(r"Bust \+(0\.[6-9]\d|1\.00)", began.group(0))),
                 began.group(0)[:300] if began else "no Begin line")
        time.sleep(6)
        screenshot("6_in_game")
        # Her own updo stays on its strands at the third-person distance: the survivor Blueprint's LOD bias (-7 on each group) carried onto
        # its 2 groups (Oct 27: the swap reset it to 0, and a Creator character's own groom draws nothing on its cards LODs -- bald in game).
        groom, bias, mats = hair_state()
        rep.step("in game her own updo keeps the Blueprint's strands at every distance (LOD bias -7 on each of its groups), its own materials on",
                 groom == "Hair_S_Updo" and len(bias) == 2 and all(b == -7.0 for b in bias) and "MI_WI_Hair_S_Updo_Hair_Cards" in mats, f"{groom} {bias} {mats}"[:300])
        # A round trip through the femme and back: each swap keeps it (the femme's 3-group braids went bald after one, Oct 27).
        query("MO.Test.Base Femme", r"\[MOTEST\] Base.*")
        time.sleep(6)
        screenshot("7_femme_round_trip")
        groom, bias, mats = hair_state()
        rep.step("back to the femme: her braids on the strands at every distance too (LOD bias -7 on each of 3 groups)",
                 groom == "Hair_S_UpdoBraids" and len(bias) == 3 and all(b == -7.0 for b in bias), f"{groom} {bias}"[:300])
        query("MO.Test.Base Lillian", r"\[MOTEST\] Base.*")
        time.sleep(6)

        # The species merged from DARKTOWER (N-221): while the player still has a pawn (the in-place reload below leaves it without one).
        for species in ("Boar", "Bear", "Rabbit", "Goat", "Sheep", "Cow", "Chicken"):
            spawned = query(f"MO.Test.SpawnCreature {species} 900", r"\[MOTEST\] SpawnCreature: .*")
            rep.step(f"the merged {species} spawns", "FAILED" not in spawned and species in spawned, spawned.strip()[:200])

        saved = query("MO.Save.SaveAs CreatorBasesCheck", r"\[MO\.Save\.SaveAs\] .*", timeout=60).strip()
        rep.step("the world saves", saved.endswith("OK"), saved[:200])
        time.sleep(3)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Save.LoadFrom CreatorBasesCheck")
        loaded = ui.wait_log(NAME, r"\[MO\.Save\.LoadFrom\] .*-> OK", since=off, timeout=180)
        time.sleep(15)
        # MO.Save.LoadFrom reloads in place and does not run the post-load handoff (no re-possession: the survivor comes back under its AI
        # controller), so this reads the survivor wearing Lillian, whoever controls it: the base and the build are what the save must keep.
        back = ui._bodyshape(NAME, None, "Lillian")
        rep.step("after a reload: the survivor is still Lillian with the build (BustLarge ~0.8)", bool(loaded) and bool(back) and back[0] == "Lillian"
                 and "SKM_MH_Lillian_Base_BodyMesh" in back[3] and 0.6 < ui._weight(back[3], "CharacterMesh0", "BustLarge") <= 1.0, str(back)[:400])
        return rep.ok
    finally:
        print("CREATOR BASES " + ("PASS" if rep.rows and rep.ok else "FAIL"), flush=True)
        print("screenshots: " + SHOTS_OUT, flush=True)
        if not keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(0 if main(keep="--keep" in sys.argv) else 1)
