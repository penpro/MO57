"""The survivor bases' body jiggle (ABP_MO_Body_PP), wired through the editor MCP BlueprintTools.

    python Tools/ue.py editor start   (PIE off)
    python Tools/abp_body_jiggle.py           # show the AnimGraph's chain
    python Tools/abp_body_jiggle.py apply     # wire it (re-runnable), compile, save

Why (Wes, Oct 28: "restore jiggle physics to the new models"): the original femme (MH_F_1) jiggled through her body mesh's post-process AnimBP,
ABP_MH_F_PP: four spring controllers on the helper joints clavicle_pec_l / _r (the chest) and thigh_bck_lwr_l / _r (the lower glutes). A base
built in the MetaHuman Creator carries the Creator's post-process instead, ABP_Body_PostProcess (RigLogic and Control Rig: the body's
corrective joints, those helpers among them), so its body had no springs. ABP_MO_Body_PP is ABP_MH_F_PP's copy with the Creator's post-process
run first as a linked anim graph (the correctives place the helper joints) and the springs after it (they jiggle where the correctives put
them). ABP_MH_F_PP's two Modify Bone nodes go: they scaled the chest 1.35 / 1.52 / 1.09 and moved it, a fixed shape the new bases' Bust
slider and body shapes replace. Content/Python/ensure_body_jiggle.py then puts ABP_MO_Body_PP on the bases' body meshes.
"""
import os
import sys
import importlib.util

_spec = importlib.util.spec_from_file_location("uecli", os.path.join(os.path.dirname(os.path.abspath(__file__)), "ue.py"))
ue = importlib.util.module_from_spec(_spec)
_argv, sys.argv = sys.argv, ["ue.py"]
_spec.loader.exec_module(ue)
sys.argv = _argv

BPT = "editor_toolset.toolsets.blueprint.BlueprintTools"
ABP = "/MOFramework/Characters/MetaHumans/Body/ABP_MO_Body_PP.ABP_MO_Body_PP"
GRAPH = {"refPath": ABP + ":AnimGraph"}
LINKED = "Animation|LinkedAnimGraphs|ABP_Body_PostProcess-LinkedAnimGraph"   # the palette's entry; a placed node reads without "-LinkedAnimGraph"
PLACED = "Animation|LinkedAnimGraphs|ABP_Body_PostProcess"
# The linked node exposes these of ABP_Body_PostProcess; an unconnected pin passes its own value (false / false / 0: the correctives OFF), so
# each takes the Creator's own default (its CDO, Oct 28: true / true / -1, every LOD).
CREATOR_PINS = {"Enable Body Correctives": "true", "Enable Head Movement IK": "true", "RigLogic LOD Threshold": "-1"}


def call(tool, **args):
    out = ue.mcp_call(BPT, tool, args)
    if isinstance(out, dict) and out.get("error"):
        raise RuntimeError("%s: %s" % (tool, str(out.get("message", out))[:600]))
    return out.get("returnValue", out) if isinstance(out, dict) and "returnValue" in out else out


def nodes():
    refs = call("find_nodes", graph=GRAPH, title="", node_class=None, entry_points_only=False)
    return call("get_node_infos", nodes=refs) if refs else []


def info(node):
    return call("get_node_infos", nodes=[node])[0]


def short(ref):
    return ref["refPath"].split(".")[-1]


def by_type(all_nodes, prefix):
    return [n for n in all_nodes if n["type_id"].startswith(prefix)]


def pose_pin(node_info, direction):
    side = node_info["input_pins"] if direction == "in" else node_info["output_pins"]
    return next(p for p in side if "Pose Link" in (p.get("type_id") or ""))


def ensure(tool, out_pin, in_pin):
    """connect / break, tolerating the bridge's retry trap (the first attempt may have applied before the retry errored)."""
    try:
        call(tool, output_pin=out_pin, input_pin=in_pin)
    except RuntimeError as e:
        if "retried" not in str(e):
            raise
        print("  (%s: retry trap, reading back)" % tool)


def chain():
    """The pose chain from the output back to the input pose, as node names (the linked post-process with its settings)."""
    all_nodes = nodes()
    node = by_type(all_nodes, "Misc.|OutputPose")[0]
    names = []
    while node:
        settings = ["%s=%s" % (p["name"], p.get("value")) for p in node["input_pins"] if p["name"] in CREATOR_PINS]
        names.append(short(node["node"]) + " (" + node["type_id"].split("|")[-1] + (": " + ", ".join(settings) if settings else "") + ")")
        ins = [p for p in node["input_pins"] if "Pose Link" in (p.get("type_id") or "")]
        node = info(ins[0]["connected_pins"][0]["node"]) if ins and ins[0]["connected_pins"] else None
    return list(reversed(names))


SOURCE = "/MOFramework/Characters/MetaHumans/Female/Animations/Animations/ABP_MH_F_PP"


def ensure_asset():
    """ABP_MO_Body_PP lives beside ABP_MH_F_PP in Plugins/MOFramework/Content/Characters/MetaHumans, which git does not carry (.gitignore):
    on a machine without it, it is made again as ABP_MH_F_PP's copy."""
    dst = ABP.split(".")[0]
    code = ('eal = unreal.EditorAssetLibrary\n'
            'if not eal.does_asset_exist("%s"):\n'
            '    eal.duplicate_asset("%s", "%s")\n'
            '    eal.save_asset("%s", only_if_is_dirty=False)\n'
            '    out("made %s from ABP_MH_F_PP")\n'
            'out("asset %%s" %% eal.does_asset_exist("%s"))\n') % (dst, SOURCE, dst, dst, dst, dst)
    ok, lines, _ = ue.bridge_run([ue._wrap_py(code)], timeout=120)
    print("  " + " | ".join(l for l in lines if l.startswith(("made", "asset", "[py-err]"))))
    if not ok or not any(l == "asset True" for l in lines):
        raise RuntimeError("no %s (bridge: %s)" % (dst, lines[-3:]))


def apply():
    ensure_asset()
    all_nodes = nodes()
    for stale in by_type(all_nodes, "Animation|SkeletalControls|Transform(Modify)Bone") + [n for n in all_nodes if n["type_id"] in (
            "|GetBoobScale", "|GetLeftBoobTranslate", "|GetRightBoobTranslate")]:
        print("  deleting", short(stale["node"]), stale["type_id"])
        call("delete_node", node=stale["node"])
    all_nodes = nodes()
    input_pose = by_type(all_nodes, "Animation|LinkedAnimGraphs|InputPose")[0]
    to_component = by_type(all_nodes, "Animation|ConvertSpaces|LocalToComponent")[0]
    first_spring = next(n for n in all_nodes if n["type_id"].endswith("Bone:clavicle_pec_l"))
    src = lambda pin: [short(c["node"]) for c in pin["connected_pins"]]
    placed = [n for n in all_nodes if n["type_id"] == PLACED]
    wired = [n for n in placed if short(n["node"]) in src(pose_pin(to_component, "in"))]
    for stray in [n for n in placed if n not in wired][: len(placed) - 1 if not wired else None]:   # keep one if none is wired yet
        print("  deleting", short(stray["node"]), "(a linked post-process not in the chain)")
        call("delete_node", node=stray["node"])
    placed = [n for n in nodes() if n["type_id"] == PLACED]
    linked = info((placed or [{"node": call("create_node", graph=GRAPH, type_id=LINKED, pos={"x": -1300, "y": -420}, declaring_class=None)}])[0]["node"])
    for p in linked["input_pins"]:
        if p["name"] in CREATOR_PINS and p.get("value") != CREATOR_PINS[p["name"]]:
            call("set_pin_value", pin=p["pin_id"], value=CREATOR_PINS[p["name"]])
            print("  %s: %s -> %s" % (p["name"], p.get("value"), CREATOR_PINS[p["name"]]))
    linked = info(linked["node"])
    # input pose -> the Creator's post-process -> local to component -> the springs
    if short(linked["node"]) not in src(pose_pin(info(to_component["node"]), "in")):
        for c in pose_pin(info(to_component["node"]), "in")["connected_pins"]:
            ensure("break_pins", pose_pin(info(c["node"]), "out")["pin_id"], pose_pin(info(to_component["node"]), "in")["pin_id"])
        ensure("connect_pins", pose_pin(linked, "out")["pin_id"], pose_pin(info(to_component["node"]), "in")["pin_id"])
    if short(input_pose["node"]) not in src(pose_pin(info(linked["node"]), "in")):
        ensure("connect_pins", pose_pin(info(input_pose["node"]), "out")["pin_id"], pose_pin(info(linked["node"]), "in")["pin_id"])
    if short(to_component["node"]) not in src(pose_pin(info(first_spring["node"]), "in")):
        ensure("connect_pins", pose_pin(info(to_component["node"]), "out")["pin_id"], pose_pin(info(first_spring["node"]), "in")["pin_id"])
    call("compile_blueprint", blueprint={"refPath": ABP}, warnings_as_errors=False)
    print(ue.mcp_call("editor_toolset.toolsets.asset.AssetTools", "save_assets", {"asset_paths": [ABP.split(".")[0]]}))


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "apply":
        apply()
    print("chain:", " -> ".join(chain()))
