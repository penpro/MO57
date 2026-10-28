"""The character creator's survivor bases, as a player meets them (Oct 27; MOSurvivorBase.h, Docs/UI.md): one -game instance with the
creator open over a new world (seed 777). Real presses where a player clicks -- Slate's hit test on the Base arrows and a build slider, a
click on them -- and the body read back after each (MO.Test.BodyShape): a new base's meshes at its own body, the build slider's shape, the
simulated body fat and muscle on body and face; then Begin, its own hair kept on its strands in game (the Blueprint's LOD bias, read back,
through a round trip to the femme: bald in third person without it), the merged animals, a save and a reload that keep the base and the
build. Screenshots in Saved/Screenshots/creator_bases/ for Wes's eye (6_in_game and 7_femme_round_trip: hair at the third-person distance).

    python Tools/check_creator_bases.py [--keep]

Prints each step and "CREATOR BASES PASS" / "FAIL". The editor must be closed (the instance builds its own world). The base it walks to
comes from the config (Tools/survivor_bases.py): the first after the femme whose hair is not hers, so the Hair row's name proves the swap.
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
import survivor_bases as sb  # noqa: E402

NAME = "crebases"
SHOTS_OUT = os.path.join(ue.ROOT, "Saved", "Screenshots", "creator_bases")
_BASES = sb.bases()
# The creator starts on the Blueprint's own body (the femme, first in the list): '>' pressed STEPS times reaches BASE.
STEPS, BASE = next(((i, b) for i, b in enumerate(_BASES) if b["id"] != "Femme" and b["hair"] != _BASES[0]["hair"]), (1, _BASES[1]))


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
        for _ in range(STEPS):
            off = ui.log_size(NAME)
            query("MO.Test.ClickWidget BaseNextButton", r"ClickWidget: '[^']*BaseNextButton")
            time.sleep(4)   # its meshes load, the grooms swap
        # Wes, Oct 27: a swap "sometimes pivots the camera to the back of the character" -- every swap re-finds the face.
        face, cam, kind = framing(off)
        rep.step("after the swap the camera re-finds her face (framed again, in front of where the face looks)",
                 kind == "CameraActor" and faces_camera(face, cam), f"face {face} camera {cam} ({kind})")
        mine = ui._bodyshape(NAME, True)
        rep.step(f"'>' x{STEPS} puts {BASE['id']} on: its body and face, at its own body ({BASE['fat']} % / {BASE['muscle']} kg), no shape on",
                 bool(mine) and mine[0] == BASE["id"] and abs(mine[1] - BASE["fat"]) < 0.2 and abs(mine[2] - BASE["muscle"]) < 0.2 and BASE["body"] in mine[3]
                 and BASE["face"] in mine[3] and ui._weight(mine[3], "CharacterMesh0", "Heavy") == 0.0
                 and ui._weight(mine[3], "CharacterMesh0", "Thin") == 0.0, str(mine)[:400])
        screenshot("2_" + BASE["id"].lower())
        # The Hair style row names the base's own hair at once (Oct 27: it named the femme's braids, still drawn while Lillian's updo loaded).
        label = query("MO.Test.FindWidget HairStyleText", r"\[MOQUERY\] WIDGET 'HairStyleText' .*")
        want = sb.groom_label(BASE["hair"])
        rep.step(f"the Hair style row names its own hair ('{want}'), not the groom it replaces", f"text='{want}'" in label, label[:200])

        # The survivor turned round behind the framing's back (a load's re-grounding, a new pose): the camera must stay in front of the face.
        # Before GASP the body snapped round with the actor and the camera re-found the face; since GASP (Oct 27) its root offset holds the
        # body's facing while she stands (orient-to-movement: she turns when she moves), so the face may not turn at all. Either way the
        # camera ends in front of where the face looks: the head bone's world yaw says whether it turned.
        head = "p = unreal.GameplayStatics.get_player_pawn(world, 0); out('HEADYAW %.1f' % p.get_editor_property('mesh').get_socket_rotation('head').yaw)"
        head0 = re.search(r"HEADYAW (-?[\d.]+)", ui.call(NAME, ["py", "-c", head], timeout=30)[1])
        off = ui.log_size(NAME)
        ui.call(NAME, ["py", "-c", "p = unreal.GameplayStatics.get_player_pawn(world, 0); pc = unreal.GameplayStatics.get_player_controller(world, 0); "
                       "r = p.get_actor_rotation(); p.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=r.yaw + 180.0), False); "
                       "c = pc.get_control_rotation(); pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=c.pitch, yaw=c.yaw + 180.0)); out('TURNED')"], timeout=30)
        turned, cam, kind = None, None, ""
        for _ in range(16):
            time.sleep(0.5)
            turned, cam, kind = framing(off)
            if turned is not None and faces_camera(turned, cam):
                break
        head1 = re.search(r"HEADYAW (-?[\d.]+)", ui.call(NAME, ["py", "-c", head], timeout=30)[1])
        head_turn = abs((float(head1.group(1)) - float(head0.group(1)) + 180.0) % 360.0 - 180.0) if head0 and head1 else None
        if turned is None:   # no re-framing: right only if the face did not turn and the camera still faces it
            ok = head_turn is not None and head_turn < 30.0 and faces_camera(face, cam if cam is not None else framing(0)[1])
        else:
            ok = faces_camera(turned, cam)
        rep.step("the survivor turned round: the camera stays in front of her face (re-found if the face turned)", ok,
                 f"face {face} -> {turned}, camera {cam}, head turned {head_turn}")
        screenshot("2b_turned_round")

        # Wes, Oct 27: "make sure we're not getting mauled by bears during character creation" -- a bear 6 m away (it hunts within 15 m)
        # leaves a survivor in the creator alone; killed again before Begin so the rest of the check is not a bear fight.
        state = ui.wait_log(NAME, r"\[MOCharacter\] \S+ is in the character creator.*", since=0, timeout=5)
        bear = query("MO.Test.SpawnCreature Bear 600", r"\[MOTEST\] SpawnCreature: .*")
        time.sleep(12)
        code, out = ui.call(NAME, ["py", "-c", "p = unreal.GameplayStatics.get_player_pawn(world, 0); "
                                   "out('WOUNDS %d' % len(p.get_component_by_class(unreal.MOAnatomyComponent).get_all_wounds()))"], timeout=30)
        wounds = re.search(r"WOUNDS (\d+)", out)
        rep.step("a bear 6 m away leaves the survivor in the creator alone (12 s, no wound)", bool(state) and "Bear" in bear and bool(wounds)
                 and wounds.group(1) == "0", f"{state.group(0)[:120] if state else 'no in-creator line'} | {bear.strip()[:100]} | wounds {wounds.group(1) if wounds else '?'}")
        ui.call(NAME, ["py", "-c", "B = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn) if a.get_class().get_name().startswith('BP_Bear')]; "
                       "[b.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.HEART, 1000.0, unreal.MOWoundType.PUNCTURE, None) for b in B]; "
                       "out('BEARS %d' % len(B))"], timeout=30)

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
        query(f"MO.Test.BodyComposition {BASE['fat']} {BASE['muscle']}", r"\[MOTEST\] BodyComposition.*")
        time.sleep(3.5)

        query("MO.Test.ClickWidget BeginButton", r"ClickWidget: '[^']*BeginButton")
        began = ui.wait_log(NAME, r"\[MOCharacterCreator\] Begin: .*", since=0, timeout=20)
        rep.step("Begin closes the creator with the base and the build", bool(began) and f"base '{BASE['id']}'" in began.group(0)
                 and bool(re.search(r"Bust \+(0\.[6-9]\d|1\.00)", began.group(0))),
                 began.group(0)[:300] if began else "no Begin line")
        left = ui.wait_log(NAME, r"\[MOCharacter\] \S+ left the character creator.*", since=0, timeout=10)
        rep.step("... and the survivor is in the world again (creatures may take it for quarry)", bool(left), left.group(0)[:160] if left else "no line")

        # Wes, Oct 27: every predator stays out of the start area (200 m round the site) unless attacked -- the same rule as a fire's. A bear 6 m
        # from the survivor at the start leaves it alone; hit it and it fights back. Killed after its first bite (the rest is not a bear fight).
        def wounds_now():
            c, o = ui.call(NAME, ["py", "-c", "p = unreal.GameplayStatics.get_player_pawn(world, 0); "
                                  "out('WOUNDS %d' % len(p.get_component_by_class(unreal.MOAnatomyComponent).get_all_wounds()))"], timeout=30)
            m = re.search(r"WOUNDS (\d+)", o)
            return int(m.group(1)) if m else -1
        bear = query("MO.Test.SpawnCreature Bear 600", r"\[MOTEST\] SpawnCreature: .*")
        w0 = wounds_now()
        time.sleep(15)
        w1 = wounds_now()
        rep.step("at the start a bear 6 m away leaves the survivor alone (15 s, no wound: the start area)", "Bear" in bear and w0 == w1 >= 0,
                 f"{bear.strip()[:100]} | wounds {w0} -> {w1}")
        ui.call(NAME, ["py", "-c", "p = unreal.GameplayStatics.get_player_pawn(world, 0); pl = p.get_actor_location(); "
                       "b = min([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn) if a.get_class().get_name().startswith('BP_Bear')], "
                       "key=lambda a: (a.get_actor_location() - pl).length()); "
                       "out('HIT %s' % b.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.TORSO, 2.0, unreal.MOWoundType.BLUNT, p))"], timeout=30)
        bitten, w2 = False, w1
        for _ in range(25):
            time.sleep(1.0)
            w2 = wounds_now()
            if w2 > w1:
                bitten = True
                break
        ui.call(NAME, ["py", "-c", "B = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn) if a.get_class().get_name().startswith('BP_Bear')]; "
                       "[b.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.HEART, 1000.0, unreal.MOWoundType.PUNCTURE, None) for b in B]; "
                       "out('BEARS %d' % len(B))"], timeout=30)
        rep.step("... hit it and it fights back (attacked: the start area no longer holds it off)", bitten, f"wounds {w1} -> {w2}")

        # Wes, Oct 27: a bear is a predator, "just a smaller agro range" (6 m; the wolf's is 20 m). With the start area off for the step, a bear
        # spawned 10 m away sees the survivor and takes it only once within 6 m (it wanders: it may come that close by itself), never from
        # farther; brought to 4 m it goes for it (ScanForQuarry takes someone seen walking in). The take is logged with its distance.
        ui.console(NAME, "MO.AI.PredatorFreeAreaM 0")
        aoff = ui.log_size(NAME)
        bear = query("MO.Test.SpawnCreature Bear 1000", r"\[MOTEST\] SpawnCreature: .*")
        name = (re.search(r"SpawnCreature: (\S+)", bear) or [None, "?"])[1]
        time.sleep(12)
        ui.call(NAME, ["py", "-c", "p = unreal.GameplayStatics.get_player_pawn(world, 0); pl = p.get_actor_location(); "
                       "b = min([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn) if a.get_class().get_name().startswith('BP_Bear')], "
                       "key=lambda a: (a.get_actor_location() - pl).length()); "
                       "b.set_actor_location(pl + p.get_actor_forward_vector() * 400.0 + unreal.Vector(0.0, 0.0, 120.0), False, True); "
                       # facing the survivor: a creature goes by what it sees, and 12 s of wandering may have turned it away
                       "r = unreal.MathLibrary.find_look_at_rotation(b.get_actor_location(), pl); b.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=r.yaw), False); out('BEAR_IN')"], timeout=30)
        took = ui.wait_log(NAME, rf"\[MOCreatureController\] {re.escape(name)} takes \S+ as its quarry at (\d+) cm \(aggro range (\d+) cm[^\r\n]*", since=aoff, timeout=20)
        ui.call(NAME, ["py", "-c", "B = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn) if a.get_class().get_name().startswith('BP_Bear')]; "
                       "[b.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.HEART, 1000.0, unreal.MOWoundType.PUNCTURE, None) for b in B]; "
                       "out('BEARS %d' % len(B))"], timeout=30)
        ui.console(NAME, "MO.AI.PredatorFreeAreaM -1")
        rep.step("a bear spawned 10 m off takes the survivor only from within its 6 m aggro range (by itself, or brought to 4 m)",
                 bool(took) and int(took.group(2)) == 600 and int(took.group(1)) <= 650, took.group(0)[:200] if took else f"{name} took no quarry")
        time.sleep(6)
        screenshot("6_in_game")
        # Its own hair stays on its strands at the third-person distance: the survivor Blueprint's LOD bias (-7 on each group) carried onto
        # its groups (Oct 27: the swap reset it to 0, and a Creator character's own groom draws nothing on its cards LODs -- bald in game).
        groom, bias, mats = hair_state()
        rep.step("in game its own hair keeps the Blueprint's strands at every distance (LOD bias -7 on each of its groups), its own materials on",
                 groom == BASE["hair"] and len(bias) >= 1 and all(b == -7.0 for b in bias) and f"MI_WI_{BASE['hair']}_Hair" in mats, f"{groom} {bias} {mats}"[:300])
        # A round trip through the femme and back: each swap keeps it (the femme's 3-group braids went bald after one, Oct 27).
        query("MO.Test.Base Femme", r"\[MOTEST\] Base.*")
        time.sleep(6)
        screenshot("7_femme_round_trip")
        groom, bias, mats = hair_state()
        rep.step("back to the femme: her braids on the strands at every distance too (LOD bias -7 on each of 3 groups)",
                 groom == "Hair_S_UpdoBraids" and len(bias) == 3 and all(b == -7.0 for b in bias), f"{groom} {bias}"[:300])
        query(f"MO.Test.Base {BASE['id']}", r"\[MOTEST\] Base.*")
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
        # controller), so this reads the survivor wearing the base, whoever controls it: the base and the build are what the save must keep.
        back = ui._bodyshape(NAME, None, BASE["id"])
        rep.step(f"after a reload: the survivor is still {BASE['id']} with the build (BustLarge ~0.8)", bool(loaded) and bool(back) and back[0] == BASE["id"]
                 and BASE["body"] in back[3] and 0.6 < ui._weight(back[3], "CharacterMesh0", "BustLarge") <= 1.0, str(back)[:400])
        return rep.ok
    finally:
        print("CREATOR BASES " + ("PASS" if rep.rows and rep.ok else "FAIL"), flush=True)
        print("screenshots: " + SHOTS_OUT, flush=True)
        if not keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(0 if main(keep="--keep" in sys.argv) else 1)
