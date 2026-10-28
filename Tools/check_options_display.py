"""The Options panel's Apply and the display, and saved volumes across a restart (a Steam player, Oct 28 2026: "after hitting apply the game
changed my screen resolution ... the apply button was off screen", and "the music that i had turned down was once again really loud even though
the % was still turned almost off").

One -game instance, twice:
  1. The player's state: borderless fullscreen (the default) with a stale stored size bigger than the screen (theirs: 2560 x 1440 on 1080p).
     Main menu -> Options (real clicks) -> Apply. The display must stay borderless at the desktop's size (the old Apply made it EXCLUSIVE
     fullscreen at the stale size), and the settings saved must say so too (the panel reads the live settings when shown). Then the music
     is saved at 10 % (as the slider would; a harness click lands nowhere while the audio rows are scrolled out of view).
  2. A fresh start: the saved music volume is on the music sound class at boot, and still after a garbage collection.

    python Tools/check_options_display.py [--keep]

Prints each step and "OPTIONS DISPLAY PASS" / "FAIL". Restores the GameUserSettings.ini it found (the instance shares the editor's).
The screen goes borderless for a few seconds in step 1.
"""
import os
import re
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "optdisplay"
INI = os.path.join(ue.ROOT, "Saved", "Config", "WindowsEditor", "GameUserSettings.ini")
MUSIC_CLASS = "/MOFramework/Audio/Mix/SC_Music"
STATE = ("s = unreal.GameUserSettings.get_game_user_settings(); r = s.get_screen_resolution(); d = s.get_desktop_resolution(); "
         "vp = unreal.WidgetLayoutLibrary.get_viewport_size(world); sc = unreal.load_asset('" + MUSIC_CLASS + "'); "
         "out('DISPLAY mode=%s res=%dx%d desktop=%dx%d viewport=%dx%d music=%.3f classvol=%.3f' % (s.get_fullscreen_mode(), r.x, r.y, d.x, d.y, "
         "vp.x, vp.y, s.get_editor_property('music_volume'), sc.get_editor_property('properties').volume))")
STATE_RE = re.compile(r"DISPLAY mode=(.+?) res=(\d+)x(\d+) desktop=(\d+)x(\d+) viewport=([\d.]+)x([\d.]+) music=([\d.]+) classvol=([\d.]+)")


def state():
    _, out = ui.call(NAME, ["py", "-c", STATE], timeout=30)
    m = STATE_RE.search(out)
    return m.groups() if m else None


def boot(rep, label):
    ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, res=(1600, 900), nosteam=True)
    if not rep.step(f"{label}: the instance boots", ui.wait_ready(NAME, 300)):
        return False
    ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
    time.sleep(6)   # the main menu and its music
    return True


def click(widget, extra=""):
    off = ui.log_size(NAME)
    ui.console(NAME, f"MO.Test.ClickWidget {widget}{extra}")
    return ui.wait_log(NAME, r"ClickWidget: '[^']*" + widget + r"[^\r\n]*", since=off, timeout=10)


def main(keep=False):
    rep = ui.Report()
    backup = INI + ".optcheck_backup"
    had_ini = os.path.exists(INI)
    if had_ini:
        shutil.copy2(INI, backup)
    try:
        if not boot(rep, "1"):
            return False
        # The player's state: borderless, a stale stored size larger than this screen
        ui.call(NAME, ["py", "-c", "s = unreal.GameUserSettings.get_game_user_settings(); d = s.get_desktop_resolution(); "
                       "s.set_fullscreen_mode(unreal.WindowMode.WINDOWED_FULLSCREEN); s.set_screen_resolution(d); s.apply_resolution_settings(False); "
                       "s.set_screen_resolution(unreal.IntPoint(d.x + 1280, d.y + 720)); out('STALE')"], timeout=30)
        time.sleep(3)
        before = state()
        rep.step("the player's state: borderless at the desktop's size, a stale larger size stored", bool(before) and "WINDOWED_FULLSCREEN" in before[0]
                 and int(before[1]) > int(before[3]), str(before))
        rep.step("Options opens (a real click on the main menu's Options)", bool(click("OptionsButton")))
        time.sleep(2)
        off = ui.log_size(NAME)
        rep.step("a real click on Apply", bool(click("ApplyButton")))
        applied = ui.wait_log(NAME, r"\[MOOptionsPanel\] Settings applied and saved - display \w+[^\r\n]*", since=off, timeout=10)
        time.sleep(3)
        after = state()
        rep.step("Apply leaves the display alone (logged unchanged; still borderless, the viewport at the desktop's size)",
                 bool(applied) and "display unchanged" in applied.group(0) and bool(after) and "WINDOWED_FULLSCREEN" in after[0]
                 and abs(float(after[5]) - float(before[5])) < 2 and abs(float(after[6]) - float(before[6])) < 2,
                 f"{applied.group(0) if applied else 'no Apply line'} | before {before} | after {after}")
        rep.step("... and what it saved is the live display: borderless at the desktop's size (the stale size gone)",
                 bool(after) and "WINDOWED_FULLSCREEN" in after[0] and after[1:3] == after[3:5], str(after))
        ui.call(NAME, ["py", "-c", "s = unreal.GameUserSettings.get_game_user_settings(); s.set_editor_property('music_volume', 0.1); "
                       "s.save_settings(); out('SAVED')"], timeout=30)
        ui.stop(NAME)
        time.sleep(3)

        if not boot(rep, "2 (a fresh start)"):
            return False
        fresh = state()
        rep.step("after a restart the saved music volume is on the music class at boot", bool(fresh) and abs(float(fresh[7]) - 0.1) < 0.03
                 and abs(float(fresh[8]) - float(fresh[7])) < 0.01, str(fresh))
        ui.console(NAME, "obj gc")
        time.sleep(3)
        gc = state()
        rep.step("... and still after a garbage collection (the class is held: a reloaded class would be back at full volume)",
                 bool(gc) and abs(float(gc[8]) - float(gc[7])) < 0.01, str(gc))
        return rep.ok
    finally:
        print("OPTIONS DISPLAY " + ("PASS" if rep.rows and rep.ok else "FAIL"), flush=True)
        if not keep:
            ui.stop(NAME)
        time.sleep(2)
        if had_ini:
            shutil.copy2(backup, INI)
            os.remove(backup)
        elif os.path.exists(INI):
            os.remove(INI)


if __name__ == "__main__":
    sys.exit(0 if main(keep="--keep" in sys.argv) else 1)
