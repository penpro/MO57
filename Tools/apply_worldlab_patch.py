"""Apply a World look lab export to MO57 (the lab: D:/UEProjects/MO_WorldLab, its "Export for MO57" button). MO57's editor open.

    python Tools/apply_worldlab_patch.py <worldlab_patch.json> [--dry-run]
    (the lab writes D:/UEProjects/MO_WorldLab/Saved/WorldLabExport/worldlab_patch_latest.json)

The lab never writes into MO57; this does, each part through MO57's own path for it:
- vegetation: per category a density and a size MULTIPLIER (1 = unchanged) and the HISMTags it covers. The live DT_Biomes row of the
  export's biome is read (`rows get`), every species whose HISMTag is listed and that is not a landmark gets DensityPerHectare x density
  and Min/MaxScale x size, and the row's whole species array is written back (`rows set`, readback-verified; Docs/TOOLING.md section 4).
  The far stand-ins (MOFar_*) are in the tags, so the far forest follows the near one.
- terrain_surfaces: absolute values of surface material instance parameters (TextureSize, NormalStrength, HeightMultiplier, MacroAmount),
  merged into Content/Python/terrain_surface_overrides.json, then applied and saved by terrain_surface_overrides.py (which
  upgrade_terrain_material.py also runs last, so a rebuild of the terrain material keeps them).
- wind: absolute scalar values of the foliage wind collection (MPC_GlobalFoliageActor), set as its defaults and saved.
--dry-run prints what would change and writes nothing. Prints APPLY lines and "WORLDLAB PATCH APPLIED" / "FAILED".
"""
import ast
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue  # noqa: E402

TABLE = "/MOFramework/Data/DT_Biomes.DT_Biomes"
OVERRIDES = os.path.join(ue.ROOT, "Content", "Python", "terrain_surface_overrides.json")


def vegetation(patch, dry):
    cats = patch.get("vegetation") or {}
    if not cats:
        return True
    biome = patch["biome"]
    rows = ue.rows_get(TABLE, [biome])
    if not isinstance(rows, dict) or biome not in rows:
        print("APPLY vegetation FAILED: no live row %s (%s)" % (biome, str(rows)[:200]))
        return False
    species = rows[biome]["species"]
    changed = 0
    for name, c in cats.items():
        tags, density, size = set(c.get("tags", [])), float(c.get("density", 1.0)), float(c.get("size", 1.0))
        hit = [e for e in species if e.get("HISMTag") in tags and not e.get("bLandmark")]
        for e in hit:
            e["DensityPerHectare"] = round(e["DensityPerHectare"] * density, 3)
            e["MinScale"] = round(e["MinScale"] * size, 3)
            e["MaxScale"] = round(e["MaxScale"] * size, 3)
        changed += len(hit)
        print("APPLY vegetation %s %s: density x%.2f, size x%.2f on %d species (tags %s)" % (biome, name, density, size, len(hit), sorted(tags)))
    if dry or not changed:
        return True
    ok, report = ue.rows_set_safe(TABLE, {biome: {"species": species}})
    for line in report:
        print("   " + line)
    return ok == 1


def editor_py(code, what, lines_out=None):
    ok, lines, _ = ue.bridge_run([ue._wrap_py(code)], timeout=300, want_log=True)
    for line in lines:
        print("   " + line)
    if lines_out is not None:
        lines_out.extend(lines)
    if not ok:
        print("APPLY %s FAILED: bridge not responding" % what)
    return ok


def terrain(patch, dry):
    new = patch.get("terrain_surfaces") or {}
    if not new:
        return True
    data = json.load(open(OVERRIDES, encoding="utf-8")) if os.path.exists(OVERRIDES) else {}
    for surface, values in new.items():
        for k, v in values.items():
            print("APPLY terrain %s %s: %s -> %s" % (surface, k, data.get(surface, {}).get(k, "(the instance's own)"), v))
            data.setdefault(surface, {})[k] = v
    if dry:
        return True
    with open(OVERRIDES, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=1, sort_keys=True)
        f.write("\n")
    return editor_py('exec(open(r"%s", encoding="utf-8").read())' % os.path.join(ue.ROOT, "Content", "Python", "terrain_surface_overrides.py").replace("\\", "/"),
                     "terrain")


def wind(patch, dry):
    w = patch.get("wind") or {}
    scalars = w.get("scalars") or {}
    if not scalars:
        return True
    for k, v in scalars.items():
        print("APPLY wind %s %s = %s" % (w["collection"], k, v))
    if dry:
        return True
    # Each element read out of the array is a COPY of the struct: change it, put it back by index, then write the array (Oct 28: changed
    # in place, the copies were dropped and the collection kept its old wind while the save said True).
    code = ("c = unreal.load_asset(%r); ps = c.get_editor_property('scalar_parameters'); want = %r\n"
            "for i in range(len(ps)):\n"
            "    p = ps[i]; n = str(p.get_editor_property('parameter_name'))\n"
            "    if n in want: p.set_editor_property('default_value', float(want[n])); ps[i] = p; out('WIND %%s = %%s' %% (n, want[n]))\n"
            "c.set_editor_property('scalar_parameters', ps)\n"
            "got = dict((str(p.get_editor_property('parameter_name')), p.get_editor_property('default_value')) for p in c.get_editor_property('scalar_parameters'))\n"
            "out('WIND readback %%s' %% dict((n, got.get(n)) for n in want))\n"
            "out('WIND saved %%s' %% unreal.EditorAssetLibrary.save_loaded_asset(c, False))") % (w["collection"], scalars)   # False: setting the array does not mark the package dirty, and a clean package is skipped
    lines = []
    if not editor_py(code, "wind", lines):
        return False
    back = next((l for l in lines if "WIND readback" in l), "")
    try:
        got = ast.literal_eval(back.split("WIND readback", 1)[1].strip())
    except (IndexError, ValueError, SyntaxError):
        got = {}
    ok = all(abs(float(got.get(k, float("nan"))) - float(v)) < 1e-4 for k, v in scalars.items())
    if not ok:
        print("APPLY wind FAILED: the collection reads back %s, wanted %s" % (got, scalars))
    return ok


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        sys.exit(__doc__)
    patch = json.load(open(args[0], encoding="utf-8-sig"))
    if patch.get("format") != "mo-worldlab-patch/1":
        sys.exit("not a World look lab export (format %r)" % patch.get("format"))
    dry = "--dry-run" in sys.argv
    if not dry and not ue.editor_running():
        sys.exit("MO57's editor is not running: python Tools/ue.py editor start (or --dry-run)")
    print("WorldLab patch %s (%s, made %s)%s" % (args[0], patch.get("biome"), patch.get("made"), " -- DRY RUN" if dry else ""))
    ok = vegetation(patch, dry) & terrain(patch, dry) & wind(patch, dry)
    print("WORLDLAB PATCH " + ("CHECKED (dry run)" if dry and ok else "APPLIED" if ok else "FAILED"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
