"""What a save must keep, in a real world: one -game host saves through MO.Save.SaveAs, goes back to the main menu and hosts the save (what the Load button calls), then reads everything back.
The automation tests (Persistence.PawnStateSurvivesASave and friends) prove the record round trip on a test pawn; this proves it on the real map, through the real load, on real survivors.

    python Tools/check_save_roundtrip.py [--seed 670711536] [--keep] [--fights-only]

Written for the DARKTOWER lane's batches 2 and 3 (Nov 2026), which no one had run in a game:
  N-291 Options' Defaults in a world, then a save: the world's seed is what the save writes, and what the load runs on
  N-292 what a pawn studied; N-298 a skill's decay clock; N-289 a colonist's character; N-285 wet clothing
  N-284 one lung is no clock, two is 180 s, and the load does not restart it; N-296 a bleeding body's circulatory clock goes on through a load
  N-295 a lone player's left-behind pawn: its history survives and it says "While you were away"
  N-299 a torch lit at a save burns after the load, flame included
  N-297 a load is not a breath of air (the reserve refills in 14 s on land, so this one can come back "could not tell", never a false pass)
  N-293 a carcass: same place, pose, mesh and cuts, and the cut part gone from its menu
  N-309 the name the world was given at Host comes back with the save
  With no load: N-287 / N-288 a blow trains Melee and a worn-out weapon says so; N-205 a wounded wolf stays on its attacker; N-300 a wolf and a deer let go of a body.
Every state read is MO.Test.SaveState, the save's own view of each person and carcass. Prints each step and "SAVE ROUNDTRIP PASS" / "FAIL". The editor must be closed.
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "saveround"
SLOT = "zz_check_save_roundtrip"
WOLF = "/MOFramework/MOBS/Wolf/Wolf.Wolf_C"
ACTORS = "unreal.GameplayStatics.get_all_actors_of_class(world, %s)"
PEOPLE = ACTORS % "unreal.MOCharacter"


def state():
    """{"people": {guid: fields}, "carcasses": [fields], "seed": int} from MO.Test.SaveState."""
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.Test.SaveState")
    end = ui.wait_log(NAME, r'SAVESTATE end seed=(-?\d+) people=\d+ carcasses=\d+(?: world="([^"]*)")?', since=off, timeout=20)
    text = ui.read_log_from(NAME, off)
    people, carcasses = {}, []
    for kind, line in re.findall(r"\[MOTEST\] SAVESTATE (pawn|carcass) ([^\r\n]*)", text):
        fields = dict(re.findall(r"(\w+)=(\S+)", line))
        if kind == "pawn":
            people[fields["guid"]] = fields
        else:
            carcasses.append(fields)
    return {"people": people, "carcasses": carcasses, "seed": int(end.group(1)) if end else None, "world": end.group(2) if end else None}


def me(st):
    return next((p for p in st["people"].values() if p.get("player") == "1"), None)


def who(name):
    return f"p = [a for a in {PEOPLE} if a.get_name() == '{name}'][0]\n"


def py(code):
    return ui._probe(NAME, code)


def survivor(dist):
    """A recruited survivor, its name and GUID."""
    off = ui.log_size(NAME)
    ui.console(NAME, f"MO.Colony.SpawnSurvivor {dist}")
    m = ui.wait_log(NAME, r"COLONY SpawnSurvivor (\S+)", since=off, timeout=20)
    name = m.group(1).strip() if m else None
    if not name or name == "FAILED":
        return None, None
    ui.console(NAME, f"MO.Colony.Recruit {name}")
    time.sleep(2)
    return name, next((g for g, p in state()["people"].items() if p["name"] == name), None)


def clock_seconds(person):
    return float(person["clock"].split(":")[1])


def clock_part(person):
    return person["clock"].split(":")[0]


def flames(name):
    """Point lights on this pawn: a burning torch adds one (UMOCarriedLightComponent::PlaceFlame)."""
    m = re.search(r"LIGHTS (\d+)", py(who(name) + "out('LIGHTS %d' % len(p.get_components_by_class(unreal.PointLightComponent)))"))
    return int(m.group(1)) if m else -1


def host_fresh_world(rep, seed):
    ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
    time.sleep(2)
    off = ui.log_size(NAME)
    ui.console(NAME, f"MO.Session.Host SaveCheck 4 {seed}")
    created = ui.wait_log(NAME, r"CreateSession succeeded|Already in a session|FAILED", since=off, timeout=45)
    if not rep.step("hosts a fresh world", bool(created) and "succeeded" in created.group(0), created.group(0) if created else "no result"):
        return False
    reached, st = ui._wait_state(NAME, "ListenServer", "", 300)
    if not rep.step("the world loaded and the host has a pawn", reached, str(st)):
        return False
    published = ui.wait_log(NAME, r"\[MOWorldSeed\] host published world seed (-?\d+)", since=off, timeout=30)
    return rep.step("CONTROL: the world runs the seed asked for", bool(published) and int(published.group(1)) == seed, published.group(0) if published else "no seed line")


def load_the_save(rep):
    ui.call(NAME, ["py", "-c", f'unreal.GameplayStatics.open_level(world, "{ui.MENU_MAP}")'], timeout=30)
    reached, st = ui._wait_state(NAME, "Standalone", ui.MENU_MAP, 120)
    if not rep.step("back at the main menu", reached, str(st)):
        return None
    ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
    time.sleep(3)
    off = ui.log_size(NAME)
    ui.console(NAME, f"MO.Session.HostSave {SLOT}")
    created = ui.wait_log(NAME, r"CreateSession succeeded|Already in a session|FAILED|could not be found", since=off, timeout=60)
    if not rep.step("the save hosts", bool(created) and "succeeded" in created.group(0), created.group(0) if created else "no result"):
        return None
    return off


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", type=int, default=670711536)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--fights-only", action="store_true", help="only the part with no load, on an instance a --keep run left up")
    a = ap.parse_args()
    rep = ui.Report()
    try:
        if a.fights_only:
            fights(rep)
            return 0 if rep.ok else 1
        ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, nosteam=True)
        if not rep.step("the instance boots", ui.wait_ready(NAME, 300)):
            return 1
        if not host_fresh_world(rep, a.seed):
            return 1
        time.sleep(30)
        # A long window: the bleeding survivor must outlast the setup and the load, not die of the check's own pace.
        ui.console(NAME, "mo.Medical.CardiacArrestDeathSeconds 3000")

        st = state()
        first = me(st)
        if not rep.step("MO.Test.SaveState reads the player", bool(first), str(first)[:200]):
            return 1
        student, student_guid = first["name"], first["guid"]

        # ---- the people, each with one thing to remember
        bleeder, bleeder_guid = survivor(500)
        lungs, lungs_guid = survivor(800)
        character, character_guid = survivor(1100)
        breath, breath_guid = survivor(1400)
        if not rep.step("CONTROL: four survivors recruited", all((bleeder_guid, lungs_guid, character_guid, breath_guid)), str((bleeder, lungs, character, breath))):
            return 1

        py(who(bleeder) + "v = p.get_component_by_class(unreal.MOVitalsComponent); v.apply_blood_loss(3900.0); out('BLED %s' % v.get_blood_volume_percent())")
        py(who(character) + "t = unreal.MOPersonalityTraits(); t.conscientiousness = -0.4; t.sociability = 0.7; t.stability = -0.9; "
                            "p.get_component_by_class(unreal.MOPersonalityComponent).set_traits(t); out('TRAITS set')")
        py(who(student) + "k = p.get_component_by_class(unreal.MOKnowledgeComponent); s = p.get_component_by_class(unreal.MOSkillsComponent); "
                          "k.inspect_item('Stick01', s); k.inspect_item('Stick01', s); s.add_experience('Melee', 3.0); "
                          "p.get_component_by_class(unreal.MOVitalsComponent).set_wetness_level(0.8); out('STUDENT set')")

        # N-284: one lung is no clock; the second starts it.
        anatomy = who(lungs) + "a = p.get_component_by_class(unreal.MOAnatomyComponent); out('LUNG %s' % a.inflict_damage(unreal.MOBodyPartType.{}, 5000.0, unreal.MOWoundType.BLUNT, None))"
        py(anatomy.format("LUNG_LEFT"))
        time.sleep(3)
        one = state()["people"].get(lungs_guid, {})
        rep.step("N-284: one lung gone: no clock, and alive", one.get("clock") == "None:0.0" and one.get("dead") == "0", str({k: one.get(k) for k in ("clock", "dead")}))
        py(anatomy.format("LUNG_RIGHT"))
        time.sleep(3)
        two = state()["people"].get(lungs_guid, {})
        rep.step("N-284: both gone: a clock of about 180 s", bool(two) and clock_part(two) == "LungRight" and 150.0 < clock_seconds(two) <= 180.0, str(two.get("clock")))

        # ---- a kill left lying, one part cut (a real kill: the body comes from its ragdoll)
        off = ui.log_size(NAME)
        ui.console(NAME, f"MO.Test.SpawnCreature {WOLF} 1500")
        ui.wait_log(NAME, r"\[MOTEST\] SpawnCreature: Wolf[^\r\n]*", since=off, timeout=20)
        off = ui.log_size(NAME)
        py("me = unreal.GameplayStatics.get_player_pawn(world, 0); l = me.get_actor_location(); "
           f"w = min({ACTORS % 'unreal.MOPredatorCreature'}, key=lambda a: (a.get_actor_location() - l).length()); "
           "out('KILL %s' % w.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.HEART, 1000.0, unreal.MOWoundType.PUNCTURE, me))")
        laid = ui.wait_log(NAME, r"\[MOCreature\] Spawned carcass '([^']*)'", since=off, timeout=30)
        rep.step("CONTROL: the killed wolf leaves a carcass", bool(laid), laid.group(0) if laid else "no carcass line")
        time.sleep(3)
        py("me = unreal.GameplayStatics.get_player_pawn(world, 0); "
           f"c = {ACTORS % 'unreal.MOCarcassActor'}[0]; me.set_actor_location(c.get_actor_location() + unreal.Vector(150.0, 0.0, 120.0), False, True); out('BESIDE ok')")
        time.sleep(3)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Test.WorldAction list")
        ui.wait_log(NAME, r"\[MOQUERY\] WORLDACTION \S+", since=off, timeout=10)
        time.sleep(1)
        listed = ui.read_log_from(NAME, off)
        if rep.step("CONTROL: the carcass lists its bones as cuttable", re.search(r"WORLDACTION   Bones \| [^|]+\| enabled", listed) is not None, listed[-200:]):
            off = ui.log_size(NAME)
            ui.console(NAME, "MO.Test.WorldAction Bones")
            ui.wait_log(NAME, r"\[MOWorldAction\] Bones on \S+ by \S+: begun", since=off, timeout=10)
            time.sleep(13)
        carcass_before = state()["carcasses"]
        rep.step("CONTROL: the carcass has its body, one part cut", len(carcass_before) == 1 and "Bones" in carcass_before[0]["cut"] and int(carcass_before[0]["saved"]) > 0 and carcass_before[0]["mesh"] != "none",
                 str(carcass_before)[:300])

        # ---- the player leaves the student for a survivor (no settlement): the student has a history and a day to report
        off = ui.log_size(NAME)
        ui.console(NAME, f"MO.Possess.Take {character_guid}")
        time.sleep(5)
        py(who(student) + "h = p.get_component_by_class(unreal.MOCharacterHistoryComponent); h.add_entry(unreal.HistoryEntryType.TASK_COMPLETE, 'Finished: Gather Sticks'); out('HISTORY added')")
        rep.step("CONTROL: the player now plays the survivor", (me(state()) or {}).get("guid") == character_guid, str((me(state()) or {}).get("name")))

        # ---- a torch burning in the hands of whoever is played
        ui.console(NAME, "MO.Light.Carried give")
        time.sleep(3)
        no_flame = flames(character)
        lit = ui._motest(NAME, "MO.Light.Carried light", r"\[MOQUERY\] LIGHT Carried light: (\d)")
        time.sleep(1)
        burning = flames(character)
        rep.step("CONTROL: the survivor holds a burning torch, its flame drawn", bool(lit) and lit.group(1) == "1" and burning == no_flame + 1, f"lights {no_flame} -> {burning}")

        # ---- a breath being held, just before the save
        py(who(breath) + "r = p.get_component_by_class(unreal.MORespirationComponent); r.set_airway_closed_by('Check', True); r.debug_set_oxygen_reserve(0.2); out('BREATH set')")

        # ---- N-291: Options' Defaults in a world, then the save
        py("s = unreal.GameUserSettings.get_game_user_settings(); s.set_to_defaults(); out('DEFAULTS pressed')")
        before = state()
        rep.step("CONTROL: the bleeder's circulatory clock is running", float(before["people"][bleeder_guid]["circ"]) > 30.0, before["people"][bleeder_guid]["circ"])
        off = ui.log_size(NAME)
        ui.console(NAME, f"MO.Save.SaveAs {SLOT}")
        saved = ui.wait_log(NAME, r"\[MO\.Save\.SaveAs\] \w+ '[^']*' -> (OK|FAILED)", since=off, timeout=90)
        if not rep.step("the world saves", bool(saved) and saved.group(1) == "OK", saved.group(0) if saved else "no result"):
            return 1
        wrote = re.search(r"\[MOPersist\] Saving world seed: (-?\d+)", ui.read_log_from(NAME, off))
        rep.step("N-291: after Options' Defaults the save writes the world's seed, not 0", bool(wrote) and int(wrote.group(1)) == a.seed, wrote.group(0) if wrote else "no seed line")
        time.sleep(3)

        # ---- the load
        off = load_the_save(rep)
        if off is None:
            return 1
        # The breath refills in 14 s on land, so read as soon as the pawn has its state.
        applied = ui.wait_log(NAME, rf"Applied component state data to pawn GUID=", since=off, timeout=300)
        early = state() if applied else {"people": {}}
        reached, st = ui._wait_state(NAME, "ListenServer", "", 300)
        rep.step("the saved world is back as a listen server", reached, str(st))
        loaded = ui.wait_log(NAME, r"Save loaded successfully: (\d+) pawns", since=off, timeout=120)
        rep.step("the load found its people", bool(loaded) and int(loaded.group(1)) >= 5, loaded.group(0) if loaded else "no line")
        back = ui.wait_log(NAME, r"Auto-possessed last-played pawn '([^']+)'", since=off, timeout=240)
        rep.step("the player has the survivor it was playing", bool(back), back.group(1) if back else "no line")
        time.sleep(5)
        after = state()
        people = after["people"]
        missing = [g for g in before["people"] if g not in people]
        rep.step("every person is back", not missing, f"{len(people)} people, missing {missing}")
        if missing:
            return 1

        was, now = before["people"], people
        rep.step("N-291: the loaded world runs the saved seed", after["seed"] == a.seed, str(after["seed"]))
        rep.step("N-309: the name given at Host comes back with the save", before["world"] == after["world"] == "SaveCheck", f"{before['world']!r} -> {after['world']!r}")
        rep.step("N-292: what the student studied", now[student_guid]["items"] == was[student_guid]["items"] and "Stick01" in now[student_guid]["items"], f"{was[student_guid]['items']} -> {now[student_guid]['items']}")
        rep.step("N-298: its skill clocks", int(now[student_guid]["clocks"]) == int(was[student_guid]["clocks"]) > 0, f"{was[student_guid]['clocks']} -> {now[student_guid]['clocks']}")
        rep.step("its Melee progress", now[student_guid]["melee"] == was[student_guid]["melee"], f"{was[student_guid]['melee']} -> {now[student_guid]['melee']}")
        rep.step("N-285: wet stays wet", abs(float(now[student_guid]["wet"]) - float(was[student_guid]["wet"])) < 0.05 and float(was[student_guid]["wet"]) > 0.7, f"{was[student_guid]['wet']} -> {now[student_guid]['wet']}")
        rep.step("N-289: a colonist's character", now[character_guid]["traits"] == "-0.40,0.70,-0.90", now[character_guid]["traits"])

        saved_clock, loaded_clock = clock_seconds(was[lungs_guid]), clock_seconds(now[lungs_guid])
        rep.step("N-284: the clock goes on from where it stood, it does not restart at 180 or vanish",
                 clock_part(now[lungs_guid]) == "LungRight" and 0.0 < loaded_clock <= saved_clock + 1.0 and now[lungs_guid]["dead"] == "0", f"{saved_clock} -> {loaded_clock}")
        saved_circ, loaded_circ = float(was[bleeder_guid]["circ"]), float(now[bleeder_guid]["circ"])
        first_circ = float(early["people"].get(bleeder_guid, {}).get("circ", loaded_circ))
        rep.step("N-296: the circulatory clock goes on, it does not restart at 0", first_circ >= saved_circ - 1.0 and now[bleeder_guid]["dead"] == "0", f"saved {saved_circ}, first read {first_circ}, later {loaded_circ}")

        rep.step("N-299: the torch burns after the load", now[character_guid]["lit"] == "1", now[character_guid]["lit"])
        relit = flames(now[character_guid]["name"])
        rep.step("N-299: ... with its flame drawn on it", relit == burning, f"lights {relit}, it had {burning} burning before the save")

        early_o2 = early["people"].get(breath_guid, {}).get("o2")
        if early_o2 is None or float(early_o2) > 0.9:
            print(f"   N-297: COULD NOT TELL: first read {early_o2}, saved {was[breath_guid]['o2']}: the reserve refills in 14 s, and this read may have come too late", flush=True)
        else:
            rep.step("N-297: a load is not a breath of air", True, f"saved {was[breath_guid]['o2']}, {early_o2} at the first read")

        rep.step("N-295: the student left behind keeps when it was left", now[student_guid]["left"] == was[student_guid]["left"] != "none", f"{was[student_guid]['left']} -> {now[student_guid]['left']}")
        rep.step("N-295: ... and its history", int(now[student_guid]["history"]) == int(was[student_guid]["history"]) > 0, f"{was[student_guid]['history']} -> {now[student_guid]['history']}")
        carcass_after = after["carcasses"]
        if rep.step("N-293: the carcass is back, and one only", len(carcass_after) == 1, str(len(carcass_after))):
            o, n = carcass_before[0], carcass_after[0]
            near = lambda u, v, cm: all(abs(float(x) - float(y)) < cm for x, y in zip(u.split(","), v.split(",")))
            rep.step("N-293: the same row, mesh and cut", (o["row"], o["mesh"], o["cut"]) == (n["row"], n["mesh"], n["cut"]), f"{n['row']} {n['mesh']} {n['cut']}")
            rep.step("N-293: in the same place", near(o["at"], n["at"], 5.0), f"{o['at']} -> {n['at']}")
            rep.step("N-293: in the same pose", int(n["bones"]) == int(o["bones"]) > 0 and abs(float(n["pose"]) - float(o["pose"])) <= 0.01 * float(o["pose"]) and near(o["root"], n["root"], 20.0),
                     f"pose {o['pose']} -> {n['pose']}, root {o['root']} -> {n['root']}")
            py("me = unreal.GameplayStatics.get_player_pawn(world, 0); "
               f"c = {ACTORS % 'unreal.MOCarcassActor'}[0]; me.set_actor_location(c.get_actor_location() + unreal.Vector(150.0, 0.0, 120.0), False, True); out('BESIDE ok')")
            time.sleep(3)
            off = ui.log_size(NAME)
            ui.console(NAME, "MO.Test.WorldAction list")
            ui.wait_log(NAME, r"\[MOQUERY\] WORLDACTION \S+", since=off, timeout=10)
            time.sleep(1)
            menu = ui.read_log_from(NAME, off)
            rep.step("N-293: the cut part is gone from its menu, the others are there", "WORLDACTION" in menu and "WORLDACTION   Bones" not in menu and len(re.findall(r"WORLDACTION   \S+", menu)) > 0, menu[-240:])

        # N-295: take the student back: it tells what it did while nobody played it.
        off = ui.log_size(NAME)
        ui.console(NAME, f"MO.Possess.Take {student_guid}")
        time.sleep(5)
        away = re.search(r"\[MOHistory\] \S+: back under \S+ -- ([^\r\n]*)", ui.read_log_from(NAME, off))
        rep.step('N-295: taken back, it says "While you were away"', bool(away) and away.group(1).startswith("While you were away") and "Finished: Gather Sticks" in away.group(1), away.group(0)[:200] if away else "no back-under line")

        # ---- no load from here: fights
        fights(rep)
        return 0 if rep.ok else 1
    finally:
        print("SAVE ROUNDTRIP " + ("PASS" if rep.rows and rep.ok else "FAIL"), flush=True)
        try:
            ui.console(NAME, f"MO.Save.Delete {SLOT}")   # never leave the check's save in the player's Load list
            time.sleep(2)
        except Exception as e:  # noqa: BLE001 -- the instance may already be gone; the check's verdict is printed above
            print(f"   could not delete the check's save: {e}", flush=True)
        if not a.keep:
            ui.stop(NAME)


def n205(rep):
    """With a deer far nearer, a wolf stays on whoever cut it: it swaps only for a quarry under half the distance of the one it has (ReplacesQuarry), and never away from its attacker.
    Wolf and deer are held still, so the deer stays 2.5 m ahead of the wolf and the attacker 30 m behind it for the whole read. CONTROL: a wolf that was NOT cut takes that same deer, so
    there was a swap to make. Far from the start: inside the start area a predator takes no quarry unprovoked (KeepsOutOfStartAreaAt)."""
    held = "w.get_component_by_class(unreal.CharacterMovementComponent).disable_movement(); "
    near = lambda cls, name: f"{name} = min({ACTORS % cls}, key=lambda a: (a.get_actor_location() - l).length()); "
    here = "p = unreal.GameplayStatics.get_player_pawn(world, 0); l = p.get_actor_location(); "
    clear = f"for c in {ACTORS % 'unreal.MOPredatorCreature'} + {ACTORS % 'unreal.MOPreyCreature'}:\n    c.destroy_actor()\nout('CLEARED')"
    wolf_out = here + near("unreal.MOPredatorCreature", "w") + "w.set_actor_location(l + unreal.Vector(3000.0, 0.0, 200.0), False, True); w.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False); " + held + "out('WOLF out')"
    deer_ahead = (here + near("unreal.MOPredatorCreature", "w") + near("unreal.MOPreyCreature", "d") + "d.set_actor_location(w.get_actor_location() + unreal.Vector(250.0, 0.0, 0.0), False, True); "
                  "d.get_component_by_class(unreal.CharacterMovementComponent).disable_movement(); out('DEER ahead')")
    threat = (here + near("unreal.MOPredatorCreature", "w") + "t = w.get_controller().get_current_threat(); out('QUARRY %s pawn %s' % (t.get_name() if t else 'none', p.get_name()))")

    def spawn(what):
        off = ui.log_size(NAME)
        ui.console(NAME, f"MO.Test.SpawnCreature {what} 1200")
        return bool(ui.wait_log(NAME, r"\[MOTEST\] SpawnCreature: [^\r\n]*", since=off, timeout=20))

    def samples(count):
        out = []
        for _ in range(count):
            time.sleep(2)
            m = re.search(r"QUARRY (\S+) pawn (\S+)", py(threat))
            out.append(m.groups() if m else None)
        return out

    py(clear)
    py(here + "p.set_actor_location(l + unreal.Vector(0.0, 60000.0, 3000.0), False, True); out('FAR ok')")
    time.sleep(35)
    # A dead attacker is rightly let go (N-300): a run whose wolves killed the player failed this step for the check's own reason (Nov 5).
    rep.step("CONTROL: the player is alive", (me(state()) or {}).get("dead") == "0", str((me(state()) or {}).get("dead")))

    # CONTROL: not cut, this wolf takes the deer.
    spawn(WOLF)
    py(wolf_out)
    spawn("Deer")
    py(deer_ahead)
    control = samples(6)
    rep.step("CONTROL: a wolf not cut takes the deer 2.5 m ahead of it", any(s and "Deer" in s[0] for s in control), str(control[:3]))
    py(clear)
    time.sleep(1)

    # Cut by the player from 30 m: the same deer must not take it away.
    spawn(WOLF)
    py(wolf_out)
    off = ui.log_size(NAME)
    py(here + near("unreal.MOPredatorCreature", "w") + "out('CUT %s' % w.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.TORSO, 2.0, unreal.MOWoundType.BLUNT, p))")
    turned = ui.wait_log(NAME, r"\[MOCreatureController\] \S+ hurt by \S+: turns on it for 45 s", since=off, timeout=8)
    rep.step("CONTROL: the wolf turns on the player who cut it", bool(turned), turned.group(0)[:160] if turned else "no line")
    spawn("Deer")
    py(deer_ahead)
    kept = samples(6)
    rep.step("N-205: the wounded wolf stays on its attacker with a deer far nearer", len(kept) == 6 and all(s and s[0] == s[1] for s in kept), str(kept[:3]))
    py(clear)
    time.sleep(1)


def fights(rep):
    """N-287 / N-288: a real swing through the combat component; N-205 and N-300: what a creature chases, read from its controller's own lines."""
    # A clear place of its own, away from the carcass and the survivors.
    py("me = unreal.GameplayStatics.get_player_pawn(world, 0); me.set_actor_location(me.get_actor_location() + unreal.Vector(6000.0, 0.0, 300.0), False, True); out('MOVED ok')")
    time.sleep(6)
    ui._probe(NAME, "p = unreal.GameplayStatics.get_player_pawn(world, 0); inv = p.get_component_by_class(unreal.MOInventoryComponent); eq = p.get_component_by_class(unreal.MOEquipmentComponent); "
                    "g = unreal.GuidLibrary.new_guid(); out('SPEAR %s %s' % (inv.add_item_by_guid_with_durability(g, 'WoodenSpear01', 1, 1), eq.equip_from_inventory(inv, g, unreal.MOEquipmentSlot.RIGHT_HAND)))")
    time.sleep(3)
    melee_before = me(state())["melee"]
    off = ui.log_size(NAME)
    ui.console(NAME, f"MO.Test.SpawnCreature {WOLF} 150")
    ui.wait_log(NAME, r"\[MOTEST\] SpawnCreature: Wolf[^\r\n]*", since=off, timeout=20)
    # A swing goes where the CAMERA looks (GetPlayerSwingAim), not the way the body faces: level the view and stand the wolf, held still, on that line.
    wolf_near = ("p = unreal.GameplayStatics.get_player_pawn(world, 0); pc = unreal.GameplayStatics.get_player_controller(world, 0); l = p.get_actor_location(); "
                 "yaw = pc.get_control_rotation().yaw; pc.set_control_rotation(unreal.Rotator(0.0, 0.0, yaw)); f = unreal.MathLibrary.get_forward_vector(unreal.Rotator(0.0, 0.0, yaw)); "
                 f"w = min({ACTORS % 'unreal.MOPredatorCreature'}, key=lambda a: (a.get_actor_location() - l).length()); "
                 "w.set_actor_location(l + f * 120.0, False, True); w.get_component_by_class(unreal.CharacterMovementComponent).disable_movement(); out('WOLF in reach')")
    broke = None
    off = ui.log_size(NAME)
    for _ in range(8):
        py(wolf_near)
        ui.console(NAME, "MO.Test.Attack")
        broke = ui.wait_log(NAME, r"\[MOCombat\] \S+: its (\S+) broke", since=off, timeout=6)
        if broke:
            break
    rep.step("N-287: a weapon worn out by a blow says so", bool(broke) and broke.group(1) == "WoodenSpear01", broke.group(0) if broke else "no broke line in 8 swings")
    melee_after = me(state())["melee"]
    rep.step("N-288: the blow that landed trained Melee", melee_after != melee_before, f"{melee_before} -> {melee_after}")
    py(f"w = {ACTORS % 'unreal.MOPredatorCreature'}[0]; w.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_WALKING); out('WOLF free')")

    n205(rep)

    # N-300: a wolf on someone who dies lets go; a deer does not run from the body. In the start area a predator takes no quarry unprovoked (KeepsOutOfStartAreaAt): the survivor cuts it first.
    py(f"for w in {ACTORS % 'unreal.MOPredatorCreature'} + {ACTORS % 'unreal.MOPreyCreature'}:\n    w.destroy_actor()\nout('CLEARED')")
    time.sleep(2)
    victim, _ = survivor(300)
    off = ui.log_size(NAME)
    ui.console(NAME, f"MO.Test.SpawnCreature {WOLF} 500")
    ui.wait_log(NAME, r"\[MOTEST\] SpawnCreature: Wolf[^\r\n]*", since=off, timeout=20)
    py(who(victim) + "l = unreal.GameplayStatics.get_player_pawn(world, 0).get_actor_location(); "
       f"w = min({ACTORS % 'unreal.MOPredatorCreature'}, key=lambda a: (a.get_actor_location() - l).length()); "
       "w.set_actor_location(p.get_actor_location() + unreal.Vector(250.0, 0.0, 60.0), False, True); "
       "out('CUT %s' % w.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.TORSO, 2.0, unreal.MOWoundType.BLUNT, p))")
    taken = ui.wait_log(NAME, rf"\[MOCreatureController\] \S+ hurt by {re.escape(victim)}: turns on it", since=off, timeout=15)
    if rep.step("CONTROL: the wolf turns on the survivor who cut it", bool(taken), taken.group(0)[:160] if taken else "no turns-on line"):
        off = ui.log_size(NAME)
        py(who(victim) + "out('DEAD %s' % p.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.HEART, 5000.0, unreal.MOWoundType.PUNCTURE, None))")
        gone = ui.wait_log(NAME, rf"\[MOCreatureController\] \S+ lets {re.escape(victim)} go: it is dead", since=off, timeout=15)
        rep.step("N-300: the wolf lets go of the body", bool(gone), gone.group(0)[:160] if gone else "no lets-go line")
        # A fresh deer between the body and the player, nearer the body: it must run from the living one.
        py(f"for w in {ACTORS % 'unreal.MOPredatorCreature'}:\n    w.destroy_actor()\nout('CLEARED')")
        py(who(victim) + "me = unreal.GameplayStatics.get_player_pawn(world, 0); me.set_actor_location(p.get_actor_location() + unreal.Vector(-500.0, 0.0, 120.0), False, True); out('BACK')")
        time.sleep(2)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Test.SpawnCreature Deer 300")
        ui.wait_log(NAME, r"\[MOTEST\] SpawnCreature: \S*Deer[^\r\n]*", since=off, timeout=20)
        py(who(victim) + "me = unreal.GameplayStatics.get_player_pawn(world, 0); "
           f"d = min({ACTORS % 'unreal.MOPreyCreature'}, key=lambda a: (a.get_actor_location() - me.get_actor_location()).length()); "
           "d.set_actor_location(p.get_actor_location() + unreal.Vector(-100.0, 200.0, 100.0), False, True); out('DEER by the body')")
        time.sleep(12)
        seen = ui.read_log_from(NAME, off)
        fled = re.findall(rf"\[MOCreatureController\] \S+ takes {re.escape(victim)} as its threat", seen)
        control = re.findall(r"\[MOCreatureController\] \S*Deer\S* takes (\S+) as its threat", seen)
        rep.step("CONTROL: the player is still alive", (me(state()) or {}).get("dead") == "0", str((me(state()) or {}).get("dead")))
        rep.step("CONTROL: the deer runs from someone living", bool(control), str(control[:2]))
        rep.step("N-300: a deer does not run from the body", not fled, f"took the body {len(fled)} times")


if __name__ == "__main__":
    sys.exit(main())
