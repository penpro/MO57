# Item Folder Structure

Each item in DT_Items has a matching folder here for its assets.

## Folder Contents

```
ItemName01/
├── BP_ItemName01.uasset      # World item Blueprint (AMOWorldItem subclass)
├── ItemName01Icon.uasset     # 256x256 icon texture (or PNG source)
├── SM_ItemName01.uasset      # Static mesh (if custom)
└── [FabAssetFolder]/         # Downloaded Fab/Megascans asset
    ├── Materials/
    ├── StaticMeshes/
    └── Textures/
```

## Naming Conventions

| Asset Type | Pattern | Example |
|------------|---------|---------|
| Folder | `{ItemId}/` | `BronzeSword01/` |
| Blueprint | `BP_{ItemId}.uasset` | `BP_BronzeSword01.uasset` |
| Icon | `{ItemId}Icon.uasset` | `BronzeSword01Icon.uasset` |
| Static Mesh | `SM_{ItemId}.uasset` | `SM_BronzeSword01.uasset` |

## Fab Asset Integration

When downloading from Fab:
1. Download directly to this item's folder
2. The Fab folder name (e.g., `tgzoahbpa_tier_2`) can stay as-is
3. Reference the mesh in BP_ItemName01 or DT_Items.WorldVisual

## DataTable References

In DT_Items, the WorldVisual column should reference:
- Simple items: `/MOFramework/Items/{ItemId}/{FabFolder}/StaticMeshes/{MeshName}`
- Complex items: `/MOFramework/Items/{ItemId}/BP_{ItemId}`

## Empty Folders

Empty folders are placeholders. Fill with:
1. Fab asset (browse in-editor: Window > Fab)
2. Generate icon using Icon Studio (coming soon)
3. Update DT_Items.WorldVisual path
