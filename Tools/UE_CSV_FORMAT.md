# UE DataTable CSV Format Reference

## Schema Changes Warning

**When you modify a DataTable row struct in C++ (e.g., `FMOItemDefinitionRow`):**

1. The existing `items.db` SQLite database will be OUT OF SYNC
2. You MUST re-import the CSV after UE exports the new schema
3. **USE `import-safe` to preserve manually-entered visual data!**

### Safe Import (Preserves UI/WorldVisual)
```bash
# RECOMMENDED: Preserves icons, meshes, materials automatically
python Tools/ue_csv_utils.py import-safe Items.csv items.db items

# Add extra fields to preserve
python Tools/ue_csv_utils.py import-safe Items.csv items.db items Nutrition
```

### Manual Backup/Restore
```bash
# Backup specific fields to JSON before risky operations
python Tools/ue_csv_utils.py backup-fields items.db items visuals_backup.json UI WorldVisual

# ... do dangerous stuff ...

# Restore from backup
python Tools/ue_csv_utils.py restore-fields items.db items visuals_backup.json
```

### Check for Schema Drift
```bash
python Tools/ue_csv_utils.py check Items.csv items.db items
```

### Adding New Columns (When Struct Gains Fields)
When you add new fields to a DataTable row struct, add them to the database:
```bash
# Add a single column
python Tools/ue_csv_utils.py add-column recipes.db recipes bIsBuilding False

# Add multiple columns from JSON
python Tools/ue_csv_utils.py add-columns-json recipes.db recipes new_columns.json

# Then export back to CSV
python Tools/ue_csv_utils.py export recipes.db Recipes.csv recipes
```

### Protected Fields (Items.csv)
These fields contain manually-entered asset paths - ALWAYS preserve them:
- **UI**: `IconSmall`, `IconLarge`, `Tint`
- **WorldVisual**: `StaticMesh`, `MaterialOverride`, `WorldActorClass`, `RelativeTransform`

---

## Critical Rules (Read This First!)

1. **NEVER manipulate CSV files directly** - Use `Tools/ue_csv_utils.py` instead
2. **Encoding**: Files are usually `utf-8-sig` (UTF-8 with BOM). Check first!
3. **Field Size**: Must set `csv.field_size_limit(sys.maxsize)` in Python
4. **Quoting**: UE uses `QUOTE_ALL` - every field is wrapped in quotes
5. **Escaped Quotes**: Quotes INSIDE quoted fields are DOUBLED (`""`)

## The Quote Escaping Trap

This is where I keep messing up. Here's how it works:

### What you write in Python:
```python
# Single quotes in your string
value = '(Id="Foraging",XPAmount=5.0)'
```

### What the CSV writer outputs to file:
```
"(Id=""Foraging"",XPAmount=5.0)"
```

### What UE reads it as:
```
(Id="Foraging",XPAmount=5.0)
```

**KEY INSIGHT**: Let Python's `csv.DictWriter(quoting=csv.QUOTE_ALL)` handle the doubling!
- You write: `"` (single quote)
- CSV writer outputs: `""` (doubled)
- UE reads: `"` (single quote)

If you manually double quotes AND let CSV writer quote them, you get `""""` (quadrupled) = BROKEN

## Using the Database Utility

```bash
# Import CSV to SQLite (do this once)
python Tools/ue_csv_utils.py import Plugins/MOFramework/Content/Data/Items.csv items.db

# Query items
python Tools/ue_csv_utils.py query items.db "SELECT ItemId, DisplayName FROM items WHERE Rarity='Rare'"

# Update items
python Tools/ue_csv_utils.py update items.db "UPDATE items SET MaxStackSize=50 WHERE ItemType='Resource'"

# Export back to CSV
python Tools/ue_csv_utils.py export items.db Plugins/MOFramework/Content/Data/Items.csv
```

### Python API:
```python
from Tools.ue_csv_utils import UEDatabase, build_inspection_grants

db = UEDatabase('items.db')
db.import_csv('Items.csv', 'items')

# Get a single item
item = db.get_item('items', 'Apple01')
print(item['DisplayName'])

# Update specific fields
db.update_item('items', 'Apple01', MaxStackSize='50', Weight='0.5')

# Bulk update with SQL
db.execute("UPDATE items SET Inspection = ? WHERE ItemId = ?", (new_value, 'Apple01'))

# Export
db.export_csv('items', 'Items.csv')
```

## UE Struct Format

### Simple struct:
```
(Field1=Value,Field2=123,Field3=True)
```

### Struct with string values (quotes needed):
```
(Name="MyItem",Description="A cool item")
```

### Array of values:
```
(Value1,Value2,Value3)
```

### Array of structs:
```
((Id="A",Value=1),(Id="B",Value=2))
```

### Nested structs:
```
(Outer=(Inner1=1,Inner2=2),Other=Value)
```

## Common Data Tables

| File | Database | Primary Key | Notes |
|------|----------|-------------|-------|
| Items.csv | Tools/items.db | --- (row name) | Item definitions (200 items) |
| Recipes.csv | Tools/recipes.db | --- (row name) | Crafting recipes (92 recipes) |
| Skills.csv | - | --- (row name) | Skill definitions |

## Inspection Field Format (Current)

```
(Grants=((Id="SkillName",bIsKnowledge=False,XPAmount=5.0,MaxLevel=3),(Id="KnowledgeName",bIsKnowledge=True,XPAmount=100.0,MaxLevel=3)))
```

Use the helper:
```python
from Tools.ue_csv_utils import build_inspection_grants

value = build_inspection_grants(
    skill_grants=[('Foraging', 5.0, 3), ('Herbalism', 3.0, 5)],
    knowledge_grants=[('WildFruits', 100.0, 3)]
)
# Returns: (Grants=((Id="Foraging",bIsKnowledge=False,XPAmount=5.0,MaxLevel=3),...))
```

## Recipe Field Format

```
(Ingredients=((ItemId="Stone01",Quantity=2),(ItemId="Stick01",Quantity=1)),CraftingStation=None,CraftLevel=0,CraftTime=5.0)
```

## Troubleshooting

### "Row has more cells than properties"
- Cause: Unescaped commas inside quoted fields
- Fix: Ensure quotes inside structs are properly doubled

### UnicodeDecodeError
- Cause: Wrong encoding
- Fix: Try encodings in order: utf-8-sig, utf-8, utf-16, utf-16-le

### Field too large
- Cause: Python CSV module has default limit
- Fix: `csv.field_size_limit(sys.maxsize)`

### Quotes showing in UE
- Cause: Triple or quadruple escaped quotes
- Fix: Use single quotes in your string, let CSV writer double them

## Backup Strategy

Always back up before modifying:
```bash
copy Items.csv Items_backup.csv
# or use git
git stash
```

The utility creates automatic backups with `import_csv()`.
