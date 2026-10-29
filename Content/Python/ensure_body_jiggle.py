"""The survivor bases' body jiggle (Oct 28 2026; Wes: "restore jiggle physics to the new models").

A body mesh's post-process AnimBP runs after the survivor's animation. The original femme's (MH_F_1) is ABP_MH_F_PP: four spring controllers on
the chest and lower-glute helper joints. A base built in the MetaHuman Creator comes with the Creator's ABP_Body_PostProcess instead (the
body's corrective joints, no springs), so it did not jiggle. ABP_MO_Body_PP runs the Creator's post-process and then the springs
(Tools/abp_body_jiggle.py wires it). This puts ABP_MO_Body_PP on every base body (MOSurvivorBaseSettings, Config/DefaultGame.ini) whose
post-process is still the Creator's, and leaves any other alone (the femme keeps ABP_MH_F_PP). Copying a base from MO_Tools brings the
Creator's back: run it after Tools/copy_metahuman_base.py (and on a new base):

    python Tools/ue.py py --file Content/Python/ensure_body_jiggle.py

Prints a "JIGGLE <base>: ..." line per base (set / kept / skipped and why) and saves the meshes it changes. The meshes live in
Content/MetaHumans, which git does not carry: the change stays on this machine's content.
"""
import os
import re

import unreal

JIGGLE = "/MOFramework/Characters/MetaHumans/Body/ABP_MO_Body_PP.ABP_MO_Body_PP_C"
CREATOR = "ABP_Body_PostProcess_C"


def bases():
    """(id, body mesh path) of each +Bases line in Config/DefaultGame.ini (MOSurvivorBaseSettings is not exposed to editor Python)."""
    ini = os.path.join(unreal.Paths.project_config_dir(), "DefaultGame.ini")
    found = []
    for line in open(ini, encoding="utf-8"):
        match = re.match(r'\+Bases=\(Id="([^"]+)".*?Body="([^"]+)"', line.strip())
        if match:
            found.append((match.group(1), match.group(2)))
    return found


def _emit(msg):
    try:
        out(msg)  # noqa: F821 -- the bridge's output, when run through it
    except NameError:
        unreal.log(msg)


def main():
    jiggle = unreal.load_object(None, JIGGLE)
    if not jiggle:
        _emit("JIGGLE FAILED: no AnimBP class at " + JIGGLE + " (Tools/abp_body_jiggle.py apply)")
        return
    for name, path in bases():
        mesh = unreal.load_asset(path.split(".")[0])
        if not mesh:
            _emit("JIGGLE %s: skipped, no body mesh at %s" % (name, path))
            continue
        current = mesh.get_editor_property("post_process_anim_blueprint")
        current_name = current.get_name() if current else "none"
        if current == jiggle:
            _emit("JIGGLE %s: kept, %s already runs %s" % (name, mesh.get_name(), current_name))
            continue
        if current_name != CREATOR:
            _emit("JIGGLE %s: left alone, %s runs its own %s" % (name, mesh.get_name(), current_name))
            continue
        mesh.set_editor_property("post_process_anim_blueprint", jiggle)
        saved = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        _emit("JIGGLE %s: set, %s %s -> %s (saved %s)" % (name, mesh.get_name(), current_name, jiggle.get_name(), saved))


main()
