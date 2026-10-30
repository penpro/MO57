"""A new player's first minutes, in a real game (Wes, Oct 29 2026):

  "add a little notice screen at the beginning of every new game ... 'This is an early access game still in development. Please report
   all bugs via the bug report menu (press tab).'"
  "add a 'open the game menu by pressing tab' to the tutorial quests ... make sure there's a tutorial quest for every menu and prioritize
   building a wood drill too so we can actually start the fire"

One -game instance, a new world, through the character creator:
  1. the early-access notice comes up after Begin (one button); "Got it" closes it
  2. the tutorial: every popup step is walked (its own steps skipped like F1 does), and each menu step is done with its real key:
     game menu, inventory, status, skills, journal, crafting, build -- each completes on its menu opening
  3. the fire, from nothing (no Firemaking, no knife): inspect a dry branch -> the fire drill recipe; craft it; a campfire; lay fuel; light it
     (attempts shortened to 2 s by MO.Test.FireAttemptSeconds; the odds are the game's) -> the tutorial's FuelLaid / FireLit events
Screenshots in Saved/Screenshots/onboarding/.

Usage: python Tools/check_onboarding.py [--seed 777] [--keep]
"""
import argparse
import glob
import os
import re
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "onboard"
SHOTS = os.path.join(ue.ROOT, "Saved", "Screenshots", "onboarding")
RESULTS = []
MENU_KEYS = {   # tutorial quest -> the input action that opens its menu (and closes it again)
    "Tutorial_GameMenu": "IA_Back_Menu", "Tutorial_Inventory": "IA_Inventory", "Tutorial_Status": "IA_PlayerStatus",
    "Tutorial_Skills": "IA_Skills", "Tutorial_Journal": "IA_Journal", "Tutorial_Crafting": "IA_Craft", "Tutorial_BuildMenu": "IA_Build",
}
def q(code, key, timeout=120):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    m = re.search(key + r" ([^\r\n]*)", out)
    return m.group(1) if m else ""


def check(label, ok, detail=""):
    RESULTS.append((label, ok, detail))
    print("%s  %s  %s" % ("PASS" if ok else "FAIL", label, detail))
    return ok


def shot(label):
    shots = os.path.join(ue.ROOT, "Saved", "Screenshots")
    before = set(glob.glob(os.path.join(shots, "**", "*.png"), recursive=True))
    ui.console(NAME, "shot showui")
    deadline, new = time.time() + 20, []
    while time.time() < deadline and not new:
        new = sorted(set(glob.glob(os.path.join(shots, "**", "*.png"), recursive=True)) - before)
        time.sleep(0.3)
    if new:
        time.sleep(0.5)
        os.makedirs(SHOTS, exist_ok=True)
        shutil.move(new[-1], os.path.join(SHOTS, label + ".png"))


def menu_open():
    return q("import unreal\nui = unreal.GameplayStatics.get_player_controller(world, 0).get_component_by_class(unreal.MOUIManagerComponent)\n"
             "out('MENU %s' % ui.is_any_menu_open())", "MENU") == "True"


def walk_tutorial():
    """The tutorial: every popup walked as the player sees it; each menu step done with its key."""
    # 2. The tutorial: walk the popups as the player sees them (the hint widget's "Showing hint <quest>.<step>: <title>" lines); do every menu
    # step with its key; skip the rest with the skip key, as a player who knows them would.
    done, order, shown = [], [], []
    since = ui.log_size(NAME) - 200000 if ui.log_size(NAME) > 200000 else 0
    for _ in range(40):
        hints = re.findall(r"Showing hint (\S+?)\.(\S+): ([^\r\n]*)", ui.read_log_from(NAME, since))
        if not hints:
            break
        qid, step, title = hints[-1]
        hint = "%s.%s: %s" % (qid, step, title)
        if qid not in shown:
            shown.append(qid)
        if qid == "Tutorial_InspectTree":
            break
        off = ui.log_size(NAME)
        if qid in MENU_KEYS and qid not in done:
            shot("2_hint_" + qid)
            ui.console(NAME, "MO.Test.InjectInput %s 1 0 0.1" % MENU_KEYS[qid])
            ok = bool(ui.wait_log(NAME, r"Quest '%s' COMPLETED" % qid, since=off, timeout=8))
            check("%s completes when its key opens the menu (%s)" % (qid, MENU_KEYS[qid]), ok, hint[:160])
            done.append(qid)
            order.append(qid)
            time.sleep(0.6)
            if menu_open() or qid == "Tutorial_BuildMenu":
                ui.console(NAME, "MO.Test.InjectInput %s 1 0 0.1" % MENU_KEYS[qid])   # close it the same way
                time.sleep(0.8)
            if menu_open():
                ui.console(NAME, "MO.UI.Close")
                time.sleep(0.5)
        else:
            ui.console(NAME, "MO.Test.InjectInput IA_SkipTutorialItem 1 0 0.1")
        # the next popup (or none: the walk ends at the first step with no popup after it)
        if not ui.wait_log(NAME, r"Showing hint ", since=off, timeout=6):
            break
    check("every menu has its tutorial step, in order", order == list(MENU_KEYS), " -> ".join(order))
    print("popups seen:", " ".join(shown))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    try:
        ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, notice=True, res=(1280, 720), nosteam=True)
        if not ui.wait_ready(NAME, 300):
            print("FAIL: the instance did not boot")
            return 1
        ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.NewGame %d" % a.seed)
        if not ui.wait_log(NAME, r"Character creator opened", since=off, timeout=420):
            print("FAIL: no character creator")
            return 1
        time.sleep(5)

        # 1. The notice.
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Test.ClickWidget BeginButton")
        m = ui.wait_log(NAME, r"\[MOUI\] early-access notice shown: [^\r\n]*", since=off, timeout=60)
        text = m.group(0) if m else ""
        check("the early-access notice comes up after Begin", bool(m), text[:240])
        check("... it says early access, still in development, and how to report a bug",
              "early access" in text and "development" in text and "Bug Report" in text, "")
        time.sleep(40)   # the world finishes loading behind it
        shot("1_early_access_notice")
        check("... and holds the screen (a menu is open)", menu_open(), "")
        ui.console(NAME, "MO.Test.ClickWidget ConfirmButton")
        time.sleep(1.5)
        check("'Got it' closes it", not menu_open(), "")

        walk_tutorial()

        # 3. The fire, from nothing: no Firemaking, no knife.
        give = ('import unreal\np = unreal.GameplayStatics.get_player_pawn(world, 0)\ninv = p.get_component_by_class(unreal.MOInventoryComponent)\n'
                'for item, n in (("StraightBranch01", 1), ("Branch01", 4), ("Tinder01", 12)):\n    inv.add_item_by_guid(unreal.GuidLibrary.new_guid(), item, n)\n'
                'sk = p.get_component_by_class(unreal.MOSkillsComponent)\n'
                'out("GIVEN firemaking %d" % sk.get_skill_level("Firemaking"))')
        check("a new survivor: no Firemaking level", q(give, "GIVEN").endswith("0"), "")
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Test.Inspect Branch01")
        ui.wait_log(NAME, r"Inspected_Branch01|ItemInspected", since=off, timeout=120)
        time.sleep(2.0)
        know = q('import unreal\np = unreal.GameplayStatics.get_player_pawn(world, 0)\nk = p.get_component_by_class(unreal.MOKnowledgeComponent)\n'
                 'out("KNOW %s" % k.has_knowledge("HandDrill"))', "KNOW")
        check("inspecting a dry branch teaches the hand drill", know == "True", "HandDrill known: " + know)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Test.Craft MakeFireDrill")
        crafted = False
        deadline = time.time() + 150
        while time.time() < deadline and not crafted:
            time.sleep(5)
            crafted = q('import unreal\np = unreal.GameplayStatics.get_player_pawn(world, 0)\ninv = p.get_component_by_class(unreal.MOInventoryComponent)\n'
                        'out("DRILL %d" % inv.get_item_count_by_definition_id("FireDrill01"))', "DRILL") not in ("", "0")
        craft_log = re.findall(r"[^\r\n]*(?:MakeFireDrill|FireDrill01)[^\r\n]*", ui.read_log_from(NAME, off))
        check("the fire drill is made with no Firemaking level and no knife", crafted, (craft_log[-1:] or ["no craft line"])[0][-200:])
        ui.console(NAME, "MO.Colony.PlaceBuilding BuildCampfire 300")
        time.sleep(2.0)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Fire.Do AddFuel.Branch01")   # the nearest fire, through the real server request
        laid = ui.wait_log(NAME, r"Quest 'Tutorial_LightFire' objective 'LayFuel'|event EMOObjectiveType::Event FuelLaid", since=off, timeout=40)
        check("laying fuel reports FuelLaid to the tutorial", bool(laid), laid.group(0) if laid else "")
        ui.console(NAME, "MO.Test.FireAttemptSeconds 2")
        caught = None
        for attempt in range(12):
            off = ui.log_size(NAME)
            ui.console(NAME, "MO.Fire.Do Light")
            m = ui.wait_log(NAME, r"\[MOFire\][^\r\n]*lighting[^\r\n]*-> (CAUGHT|failed)", since=off, timeout=20)
            line = m.group(0) if m else "no attempt line"
            print("  attempt %d: %s" % (attempt + 1, line[-160:]))
            if m and m.group(1) == "CAUGHT":
                caught = ui.read_log_from(NAME, off)
                break
        check("the campfire lights with the fire drill", caught is not None, "")
        check("... and reports FireLit to the tutorial", bool(caught) and bool(re.search(r"FireLit", caught)), "")
        shot("3_fire_lit")

        failed = [r for r in RESULTS if not r[1]]
        print("RESULT %d of %d checks pass%s" % (len(RESULTS) - len(failed), len(RESULTS), "" if not failed else ": FAILED " + "; ".join(r[0] for r in failed)))
        return 0 if not failed else 1
    finally:
        if not a.keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
