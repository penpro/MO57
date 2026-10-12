# Inventory

Layer: Menu · Input: menu

Grid: Panel 1600x880 centred (160,100); 24px padding; 3 columns 400 / 568 / 536, 24 gutters

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Panel | WBP_MOPanel(elev1) | 160 | 100 | 1600 | 880 | Center | - | bg: color.surface.1; edge: color.border.subtle |  |
| Header | WBP_PanelHeader | 184 | 116 | 1552 | 56 | Center | - | title: type.title; weight: type.numeric; close: icon button 44 |  |
| PaperDoll | WBP_EquipmentDoll | 184 | 196 | 400 | 760 | Center | 2 | slot: slot 64; silhouette: color.surface.3 line art | 12 slots: head, face, neck, torso-inner, torso-outer, back, hands, belt, legs, feet, L hand, R hand |
| BackpackHeader | WBP_SectionHeader | 608 | 196 | 568 | 40 | Center | - | text: type.label; capacity: type.numeric.small |  |
| BackpackGrid | WBP_InventoryGrid | 608 | 244 | 568 | 424 | Center | 1 | slot: 64 gap 8; cols: 8; rows: 6 | initial focus |
| NearbyHeader | WBP_SectionHeader+Actions | 608 | 692 | 568 | 44 | Center | 3 | actions: Take all / Deposit matching (secondary, h44) |  |
| NearbyList | WBP_ItemList | 608 | 740 | 568 | 216 | Center | 4 | row: row.height 48; count: type.numeric |  |
| ItemInfo | WBP_ItemInfoPanel(elev2) | 1200 | 196 | 536 | 680 | Center | 5 | name: type.heading; stats: type.body + type.numeric; condition: progress segmented |  |
| ItemActions | WBP_ButtonRow | 1200 | 892 | 536 | 56 | Center | 6 | primary: Use (56); secondary: Split / Drop |  |
| ContextMenu | WBP_ContextMenu | 0 | 0 | 280 | auto | Cursor | - | row: 44; bg: surface.3 + shadow | spawns at slot; gamepad: face_west |
| DragVisual | WBP_DragVisual | 0 | 0 | 64 | 64 | Cursor | - | opacity: 0.85; count: type.numeric.small |  |
