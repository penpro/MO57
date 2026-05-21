---
name: ue-datatable-json
description: Read or modify Unreal Engine DataTable JSON exports (Recipes, Items, Skills, etc.) using the project's Tools/ue_json_utils.py utility. Use whenever you need to add a row to a DataTable, update a field, or look up an existing row.
allowed-tools:
  - Bash
  - Read
  - Write
---

# UE DataTable JSON edits

This project's DataTables (Recipes, Items, Skills, Resources, etc.) are
authored as `.uasset` files but maintained via **JSON exports** sitting
beside them in `Plugins/MOFramework/Content/Data/`.

## When to use this skill

- Adding a new recipe / item / resource definition
- Bulk renaming or field updates
- Looking up the schema of an existing row before authoring a new one
- Verifying a row exists before referencing it from code

## When NOT to use this skill

- Complex struct array edits (`TArray<FMOToolRequirement>`, build parts with
  nested floats, etc.) — those are fragile via text edit. Author in the
  DataTable Editor inside Unreal, then re-export the JSON.
- Anything that touches Localization (NSLOCTEXT() wrapping). For new rows,
  pass plain strings as DisplayName/Description — UE will localize them on
  first save.

## The tool

`Tools/ue_json_utils.py` — Python 3, no third-party deps. Run from project root:

```
python Tools/ue_json_utils.py <command> <file> [...]
```

### Subcommands

| Command | Purpose |
|---|---|
| `list FILE` | Print every row's Name (paginate with shell tools if huge) |
| `get FILE ROWNAME` | Pretty-print one row as JSON |
| `has FILE ROWNAME` | Exit 0 if row exists, 1 otherwise. Use in scripts. |
| `add FILE 'JSON_BLOB'` | Insert a new row from an inline JSON string |
| `add-file FILE PAYLOAD.json` | Insert from a payload file (cleaner for large rows) |
| `delete FILE ROWNAME` | Remove a row by Name |
| `update FILE ROWNAME FIELD VALUE` | Set one field. VALUE is parsed as JSON (use `5`, `true`, `'"text"'`, etc.) |

### File format guarantees

- UTF-8 (no BOM), CRLF line endings, tab indentation — same as UE's exporter
- `add` uses text-mode insertion so existing rows keep their original
  formatting byte-for-byte. Only the new row uses our serializer.
- `delete` and `update` parse + reserialize the whole file. Result is valid
  JSON UE re-imports correctly, but the diff against the original will show
  whitespace changes on every row. Use sparingly. For routine maintenance,
  use the DataTable Editor in UE and re-export.

## Workflow

1. **Identify the file**: `Plugins/MOFramework/Content/Data/Recipes_JSON.json`
   (or `Items_JSON.json`, `Resources_JSON.json`, etc.)
2. **Look up an existing row** as a template: `get FILE EXISTING_NAME`
3. **Author the new row JSON** with all required fields filled in
4. **Insert**: `add FILE '<json>'` or `add-file FILE payload.json`
5. **In UE Editor**: right-click the `.uasset` next to the JSON → Reimport JSON
6. **Verify** the row shows up in the DataTable Editor

## Common files

| File | Row struct | Notes |
|---|---|---|
| `Recipes_JSON.json` | FMORecipeDefinitionRow | Crafting + building recipes. `bIsBuilding=true` for placeables. |
| `Items_JSON.json` | FMOItemDefinitionRow | All inventory items |
| `Skills.csv` (no JSON) | FMOSkillDefinitionRow | CSV path; use `Tools/ue_csv_utils.py` instead |
| `Resources_JSON.json` | FMOResourceNodeDefinitionRow | Harvestable nodes (trees, rocks, etc.) |
| `Weapons_JSON.json` | FMOWeaponDefinitionRow | Weapon profiles |
| `Carcasses_JSON.json` | (specific) | Animal carcass definitions |

## Examples

```bash
# What recipes exist?
python Tools/ue_json_utils.py list Plugins/MOFramework/Content/Data/Recipes_JSON.json

# Look up the campfire recipe as a template
python Tools/ue_json_utils.py get Plugins/MOFramework/Content/Data/Recipes_JSON.json BuildCampfire

# Check if a row exists before referencing
python Tools/ue_json_utils.py has Plugins/MOFramework/Content/Data/Items_JSON.json Stick01 && echo "OK"

# Add a new building recipe from a prepared payload file
python Tools/ue_json_utils.py add-file \
    Plugins/MOFramework/Content/Data/Recipes_JSON.json \
    /tmp/new_recipe.json

# Bump the build time on an existing recipe
python Tools/ue_json_utils.py update \
    Plugins/MOFramework/Content/Data/Recipes_JSON.json \
    BuildCampfire TotalBuildTime 45

# Remove a recipe entirely
python Tools/ue_json_utils.py delete \
    Plugins/MOFramework/Content/Data/Recipes_JSON.json \
    BuildOldThing
```

## Gotchas

- **Always look up an existing row first** with `get` to copy the exact schema.
  UE's row structs have ~40 fields each; missing one or misnaming one
  silently produces incorrect imports.
- **`bIsBuilding=true` rows** need `PlacementData.BuildableActorClass` pointing
  at a Blueprint generated class path:
  `/Script/Engine.BlueprintGeneratedClass'/MOFramework/BP_Foo.BP_Foo_C'`
- **After editing**, the user must reimport in UE — JSON changes don't apply
  until `.uasset` is regenerated.
- **Don't try to edit struct arrays** (BuildParts, RequiredTools) with
  `update` — pass the whole array as the value, or do it in the DataTable Editor.
