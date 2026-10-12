# Crafting / Building

Layer: Menu · Input: menu

Grid: Panel 1600x880; header 80; list 520 | detail 1008; queue strip 72

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Panel | WBP_MOPanel(elev1) | 160 | 100 | 1600 | 880 | Center | - | bg: color.surface.1; edge: color.border.subtle |  |
| StationHeader | WBP_StationHeader | 184 | 116 | 1552 | 72 | Center | - | name: type.title; fuel: progress 8 + type.numeric 'Fuel 1:42:10' |  |
| Search | WBP_SearchField | 184 | 204 | 400 | 44 | Center | 2 | input: input 44 |  |
| Filter | WBP_IconButton | 592 | 204 | 44 | 44 | Center | 3 |  |  |
| Sort | WBP_IconButton | 644 | 204 | 44 | 44 | Center | 4 |  |  |
| RecipeList | WBP_RecipeList | 184 | 260 | 520 | 600 | Center | 1 | row: 56: icon 32, name body.strong, state caption; locked: text.disabled + lock | initial focus on first craftable |
| Detail | WBP_RecipeDetail(elev2) | 728 | 204 | 1008 | 656 | Center | - | name: type.title; icon: 96; desc: type.body; ingredients: row 44: have/need type.numeric coloured ok/critical + shape |  |
| AmountSpinner | WBP_SpinBox | 1272 | 788 | 200 | 56 | Center | 5 |  |  |
| CraftButton | WBP_MOButton(Primary) | 1488 | 788 | 224 | 56 | Center | 6 |  |  |
| WhyNot | WBP_InlineMessage | 752 | 744 | 960 | 32 | Center | - | icon: sev_warning; text: type.body severity.warning |  |
| Queue | WBP_CraftQueue | 184 | 884 | 1552 | 72 | Center | 7 | item: 64 slot + radial-free linear bar under it; time: type.numeric.small |  |
| PlacementBar | WBP_PlacementContextBar | 560 | 984 | 800 | 56 | BottomCenter | - | bg: hud.plate; keys: keycap 32 + type.hud | Building mode only (HUD layer) |
