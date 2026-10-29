"""The result of check_body_jiggle.py: prints its "JIGGLE CHECK" lines and puts the editor back on the level it had open (or the
editor's startup map, if that was an untitled one)."""
import json
import os

import unreal

_path = os.path.join(unreal.Paths.project_saved_dir(), "body_jiggle_check.json")
if not os.path.exists(_path):
    out("JIGGLE CHECK not finished yet")
else:
    _r = json.load(open(_path))
    for _case, _bones in _r["max_cm"].items():
        out("JIGGLE CHECK %s (post-process %s): %s" % (_case, _r["post_process"].get(_case), ", ".join("%s %.2f cm" % (b, d) for b, d in _bones.items())))
    os.remove(_path)
    _back = _r["back_to"]
    if _back.startswith("/Temp/"):   # an untitled map (a run before this one): the editor's startup map instead
        _back = str(unreal.get_default_object(unreal.GameMapsSettings).get_editor_property("editor_startup_map").export_text()).split(".")[0]
    unreal.EditorLoadingAndSavingUtils.load_map(_back)
    out("JIGGLE CHECK level back to %s" % unreal.EditorLevelLibrary.get_editor_world().get_path_name())
