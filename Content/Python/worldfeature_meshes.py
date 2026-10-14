"""Which static meshes the world-feature compositions (DT_WorldFeatures) actually spawn -> Content/Python/data/worldfeature_meshes.json.

Why a PIE run: a composition's meshes are point attributes inside PCG data (Electric Dreams' PCGAsset elements, BP_PCG_*Assembly graphs); the asset registry does
not see them as dependencies, so the only complete list is what the PCG components build. fix_mesh_collision.py reads the manifest (PIE stopped: saving is
blocked in PIE) and gives those meshes collision.

    python Tools/ue.py boot --seed -1675528670
    python Tools/ue.py py -c "import worldfeature_meshes as w; w.spawn()"
    python Tools/ue.py py -c "import worldfeature_meshes as w; w.collect()"      (a few seconds later: the graphs generate over several frames)

Re-run after adding or changing a composition, commit the manifest. Output lines start with WFMESH.
"""
import json
import os
import re

import unreal

TAG = "WFMeshAudit"
MANIFEST = os.path.join(unreal.Paths.project_content_dir(), "Python", "data", "worldfeature_meshes.json")
FEATURE_ACTOR = "/Script/MOFramework.MOWorldFeatureActor"


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def _rows():
    """[(row, actor class path, [variant graph paths])] for every enabled row."""
    dt = unreal.load_asset("/MOFramework/Data/DT_WorldFeatures")
    lib = unreal.DataTableFunctionLibrary
    names = [str(n) for n in lib.get_data_table_row_names(dt)]
    cols = {c: lib.get_data_table_column_as_string(dt, c) for c in ("ActorClass", "ActorProperties", "bEnabled")}
    out = []
    for i, n in enumerate(names):
        if cols["bEnabled"][i] != "True":
            continue
        props = cols["ActorProperties"][i]
        variants = []
        m = re.search(r'"Variants",\s*"\((.*?)\)"', props)
        if m:
            variants = re.findall(r'(/[A-Za-z0-9_/]+\.[A-Za-z0-9_]+)', m.group(1))
        simple = {k: v for k, v in re.findall(r'\("([A-Za-z0-9_]+)", "([^"\(]*)"\)', props) if k != "Variants"}
        out.append((n, cols["ActorClass"][i], variants, simple))
    return out


def _apply(actor, props):
    """The row's plain ActorProperties (bools / numbers) on the summoned actor (the Electric Dreams knobs: EnableCliff ...)."""
    for k, v in props.items():
        value = True if v == "True" else False if v == "False" else v
        if isinstance(value, str):
            try:
                value = float(value)
            except ValueError:
                pass
        try:
            actor.set_editor_property(k, value)
        except Exception as exc:
            print("WFMESH could not set %s=%s on %s (%s)" % (k, v, actor.get_name(), str(exc)[:60]))


def _summon(world, pc, class_path):
    cls = unreal.load_class(None, class_path)
    if cls is None:
        return None
    before = set(a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls))
    unreal.SystemLibrary.execute_console_command(world, "summon " + class_path, pc)
    new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls) if a.get_name() not in before]
    return new[0] if new else None


def spawn():
    """Spawn one actor per row (one per variant for MOWorldFeatureActor rows) far from the player and generate it."""
    world = _world()
    if world is None:
        print("WFMESH no game world (boot PIE first)")
        return
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    for a in unreal.GameplayStatics.get_all_actors_with_tag(world, TAG):
        a.destroy_actor()
    slot = 0
    for row, cls, variants, props in _rows():
        for v in (variants or [None]):
            a = _summon(world, pc, cls)
            if a is None:
                print("WFMESH could not summon %s (%s)" % (cls, row))
                continue
            a.set_actor_location_and_rotation(unreal.Vector(9000000.0 + slot * 40000.0, 9000000.0, 400000.0), unreal.Rotator(), False, True)
            a.set_editor_property("tags", [TAG, row])
            _apply(a, props)
            for c in a.get_components_by_class(unreal.PCGComponent):
                if v:
                    c.set_graph(unreal.load_asset(v.split(".")[0]))
                c.generate_local(True)
            print("WFMESH spawned %s %s" % (row, v or cls))
            slot += 1


def collect():
    """Write {mesh path: [rows]} for every mesh the spawned compositions built, then remove them."""
    world = _world()
    if world is None:
        print("WFMESH no game world")
        return
    meshes = {}
    actors = unreal.GameplayStatics.get_all_actors_with_tag(world, TAG)
    for a in actors:
        row = str(a.tags[1])
        n = 0
        for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            m = c.get_editor_property("static_mesh")
            if m is not None and c.get_instance_count() > 0:
                meshes.setdefault(m.get_path_name(), set()).add(row)
                n += 1
        print("WFMESH %s: %d mesh components" % (row, n))
        a.destroy_actor()
    os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
    with open(MANIFEST, "w", encoding="utf-8") as f:
        json.dump({k: sorted(v) for k, v in sorted(meshes.items())}, f, indent=1)
    print("WFMESH wrote %d meshes from %d actors -> %s" % (len(meshes), len(actors), MANIFEST))
