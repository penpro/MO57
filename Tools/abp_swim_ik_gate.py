"""Turn the MetaHuman body's foot IK off while swimming (ABP_MH_F AnimGraph), via the editor MCP BlueprintTools.

    python Tools/ue.py ...            (editor up, PIE off)
    python Tools/abp_swim_ik_gate.py           # show the IK gate's wiring
    python Tools/abp_swim_ik_gate.py apply     # rewire it (idempotent), compile, save

Why (2026-10-14): swim poses now play as a montage in the blueprint's DefaultSlot (bug #13, so entering / leaving the water
cross-fades). The AnimGraph is StateMachine -> Slot 'DefaultSlot' -> Control Rig (foot IK) -> Output, and the rig's
ShouldDoIKTrace pin was NOT IsFalling. Swimming is not falling, so the foot IK ran on the swim pose and pulled both feet down
toward the lake bed: legs hanging straight under a swimming torso. The old single-node swap bypassed the blueprint, so it
never showed. Feet have nothing to plant on in water: ShouldDoIKTrace = NOT (IsFalling OR MovementComponent.IsSwimming).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util

_spec = importlib.util.spec_from_file_location("uecli", os.path.join(os.path.dirname(os.path.abspath(__file__)), "ue.py"))
ue = importlib.util.module_from_spec(_spec)
_argv, sys.argv = sys.argv, ["ue.py"]
_spec.loader.exec_module(ue)
sys.argv = _argv

BPT = "editor_toolset.toolsets.blueprint.BlueprintTools"
ABP = "/MOFramework/Characters/MetaHumans/Female/Animations/Animations/ABP_MH_F.ABP_MH_F"
GRAPH = {"refPath": ABP + ":AnimGraph"}
RIG = {"refPath": ABP + ":AnimGraph.AnimGraphNode_ControlRig_0"}


def call(tool, **args):
    out = ue.mcp_call(BPT, tool, args)
    if isinstance(out, dict) and out.get("error"):
        raise RuntimeError("%s: %s" % (tool, out.get("message", out)[:600]))
    return out.get("returnValue", out) if isinstance(out, dict) and "returnValue" in out else out


def info(node):
    return call("get_node_infos", nodes=[node])[0]


def pin(node_info, name, direction):
    side = node_info["input_pins"] if direction == "in" else node_info["output_pins"]
    for p in side:
        if p["name"] == name:
            return p
    raise KeyError("%s has no %s pin %r (has %s)" % (node_info["node"]["refPath"], direction, name, [p["name"] for p in side]))


def upstream(node_info, pin_name):
    p = pin(node_info, pin_name, "in")
    return [c["node"] for c in p["connected_pins"]]


def show():
    rig = info(RIG)
    should = pin(rig, "ShouldDoIKTrace", "in")
    print("ShouldDoIKTrace <-", [c["node"]["refPath"].split(".")[-1] for c in should["connected_pins"]])
    for n in upstream(rig, "ShouldDoIKTrace"):
        ni = info(n)
        print("  %s %s inputs=%s" % (n["refPath"].split(".")[-1], ni["type_id"],
                                      [(p["name"], [c["node"]["refPath"].split(".")[-1] for c in p["connected_pins"]]) for p in ni["input_pins"]]))
    return rig


def make(type_id, x, y):
    return call("create_node", graph=GRAPH, type_id=type_id, pos={"x": x, "y": y}, declaring_class=None)


EVENT_GRAPH = {"refPath": ABP + ":EventGraph"}


def all_nodes(graph=GRAPH):
    refs = call("find_nodes", graph=graph, title="", node_class=None, entry_points_only=False)
    return call("get_node_infos", nodes=refs) if refs else []


def ensure(tool, out_pin, in_pin):
    """connect / break, tolerating the bridge's retry trap (the first attempt may have applied before the retry errored)."""
    try:
        call(tool, output_pin=out_pin, input_pin=in_pin)
    except RuntimeError as e:
        if "retried" not in str(e):
            raise
        print("  (%s: retry trap, reading back)" % tool)


def src(p):
    return [c["node"]["refPath"] for c in p["connected_pins"]]


def apply():
    """State-based (re-runnable after a partial run). The swimming flag is computed in the EVENT graph next to IsFalling (same
    component, same place), and the AnimGraph reads the variable: a function call on MovementComponent inside the AnimGraph runs
    in the editor's preview instances too, where there is no character ("Accessed None ... MovementComponent" on every save)."""
    if "IsSwimming" not in call("list_variables", blueprint={"refPath": ABP}, graph=None):
        call("add_variable", blueprint={"refPath": ABP}, name="IsSwimming", type_name="bool", graph=None, container_type=None)
        print("  added variable IsSwimming")

    # Event graph: ... SetIsFalling -> SetIsSwimming(IsSwimming(MovementComponent))
    ev = all_nodes(EVENT_GRAPH)
    by = lambda nodes, t: [n for n in nodes if n["type_id"] == t]
    set_falling = by(ev, "|SetIsFalling")[0]
    set_swim = (by(ev, "|SetIsSwimming") or [info(call("create_node", graph=EVENT_GRAPH, type_id="Variables|Default|SetIsSwimming", pos={"x": 1700, "y": 600}, declaring_class=None))])[0]
    calls = [n for n in by(ev, "AI|Components|NavMovement|IsSwimming")]
    swim_call = (calls or [info(call("create_node", graph=EVENT_GRAPH, type_id="AI|Components|NavMovement|IsSwimming", pos={"x": 1450, "y": 700}, declaring_class=None))])[0]
    then = pin(set_falling, "then", "out")
    exec_in = [p for p in set_swim["input_pins"] if p["type_id"] == "Exec"][0]
    if not src(exec_in):
        ensure("connect_pins", then["pin_id"], exec_in["pin_id"])
    target = [p for p in swim_call["input_pins"] if p["type_id"] != "Boolean"][0]
    if not src(target):
        mc = info(call("create_node", graph=EVENT_GRAPH, type_id="Variables|Default|GetMovementComponent", pos={"x": 1250, "y": 700}, declaring_class=None))
        ensure("connect_pins", mc["output_pins"][0]["pin_id"], target["pin_id"])
    value_in = [p for p in set_swim["input_pins"] if p["type_id"] == "Boolean"][0]
    if not src(value_in):
        ensure("connect_pins", [p for p in swim_call["output_pins"] if p["type_id"] == "Boolean"][0]["pin_id"], value_in["pin_id"])

    # AnimGraph: ShouldDoIKTrace = NOT (IsFalling OR IsSwimming), both variables
    rig = show()
    nodes = all_nodes()
    not_info = info(upstream(rig, "ShouldDoIKTrace")[0])
    if "NOTBoolean" not in not_info["type_id"]:
        raise RuntimeError("ShouldDoIKTrace is not fed by a NOT node: %s" % not_info["type_id"])
    falling = by(nodes, "|GetIsFalling")[0]
    or_info = (by(nodes, "Math|Boolean|ORBoolean") or [info(call("create_node", graph=GRAPH, type_id="Math|Boolean|ORBoolean", pos={"x": -480, "y": 300}, declaring_class=None))])[0]
    get_swim = (by(nodes, "|GetIsSwimming") or [info(call("create_node", graph=GRAPH, type_id="Variables|Default|GetIsSwimming", pos={"x": -700, "y": 420}, declaring_class=None))])[0]
    for stale in by(nodes, "AI|Components|NavMovement|IsSwimming") + by(nodes, "|GetMovementComponent"):   # the first version's in-graph call
        print("  deleting", stale["node"]["refPath"].split(".")[-1], stale["type_id"])
        call("delete_node", node=stale["node"])
    or_info = info(or_info["node"])
    falling_out = pin(falling, "IsFalling", "out")["pin_id"]
    not_a = pin(not_info, "A", "in")
    or_ins = [p for p in or_info["input_pins"] if p["type_id"] == "Boolean"]
    if falling["node"]["refPath"] in src(not_a):
        ensure("break_pins", falling_out, not_a["pin_id"])
    if not src(or_ins[0]):
        ensure("connect_pins", falling_out, or_ins[0]["pin_id"])
    if not src(or_ins[1]):
        ensure("connect_pins", pin(get_swim, "IsSwimming", "out")["pin_id"], or_ins[1]["pin_id"])
    if or_info["node"]["refPath"] not in src(pin(info(not_info["node"]), "A", "in")):
        ensure("connect_pins", [p for p in or_info["output_pins"] if p["type_id"] == "Boolean"][0]["pin_id"], not_a["pin_id"])

    # ... and the rig itself OFF in the water (Alpha = IsSwimming ? 0 : 1). Gating only the trace was not enough (measured: the legs still hung straight):
    # the foot rig solves the feet toward the ik_foot_* bones, which the retargeted swim clips leave at the standing reference pose.
    rig = info(RIG)
    alpha = pin(rig, "Alpha", "in")
    if not src(alpha):
        sel = info(call("create_node", graph=GRAPH, type_id="Math|Float|SelectFloat", pos={"x": -480, "y": 160}, declaring_class=None))
        a, b, pick = pin(sel, "A", "in"), pin(sel, "B", "in"), pin(sel, "bPickA", "in")
        call("set_pin_value", pin=a["pin_id"], value="0.0")
        call("set_pin_value", pin=b["pin_id"], value="1.0")
        get_swim2 = info(call("create_node", graph=GRAPH, type_id="Variables|Default|GetIsSwimming", pos={"x": -700, "y": 180}, declaring_class=None))
        ensure("connect_pins", pin(get_swim2, "IsSwimming", "out")["pin_id"], pick["pin_id"])
        ensure("connect_pins", sel["output_pins"][0]["pin_id"], alpha["pin_id"])

    call("compile_blueprint", blueprint={"refPath": ABP}, warnings_as_errors=False)
    print(ue.mcp_call("editor_toolset.toolsets.asset.AssetTools", "save_assets", {"asset_paths": [ABP.split(".")[0]]}))
    show()
    print("  rig Alpha <-", [c["node"]["refPath"].split(".")[-1] for c in pin(info(RIG), "Alpha", "in")["connected_pins"]])
    print("  OR inputs:", [(p["name"], [c["node"]["refPath"].split(".")[-1] for c in p["connected_pins"]]) for p in info(or_info["node"])["input_pins"]])
    sw = by(all_nodes(EVENT_GRAPH), "|SetIsSwimming")[0]
    print("  SetIsSwimming inputs:", [(p["name"], [c["node"]["refPath"].split(".")[-1] for c in p["connected_pins"]]) for p in sw["input_pins"]])


if __name__ == "__main__":
    apply() if len(sys.argv) > 1 and sys.argv[1] == "apply" else show()
