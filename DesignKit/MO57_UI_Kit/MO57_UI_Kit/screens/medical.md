# Character status (medical)

Layer: Menu · Input: menu

Grid: Panel 1600x880; summary strip 96; body 456 | wounds 560 | vitals 488; paper inserts

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Panel | WBP_MOPanel(elev1) | 160 | 100 | 1600 | 880 | Center | - | bg: color.surface.1; edge: color.border.subtle |  |
| Summary | WBP_StatusSummary | 184 | 116 | 1552 | 96 | Center | - | state: type.heading + severity shape; chips: consciousness / shock / blood loss / temperature |  |
| BodyToggle | WBP_SegmentedTabs | 184 | 228 | 456 | 44 | Center | 2 | tabs: Front / Back |  |
| BodyDiagram | WBP_BodyDiagram | 184 | 284 | 456 | 672 | Center | 1 | part: fill by worst wound severity; hatched = bandaged; outline = selected | ~55 parts; gamepad moves between parts by spatial nav |
| WoundList | WBP_WoundList(paper) | 664 | 228 | 560 | 728 | Center | 3 | bg: color.paper.surface; text: color.paper.ink; row: 56: part, wound type, state tags, time since |  |
| Vitals | WBP_VitalsPanel(paper) | 1248 | 228 | 488 | 480 | Center | 4 | value: type.numeric.large; range: type.caption 'normal 60-100'; trend: trend_up/down/flat 24 |  |
| Nutrition | WBP_NutritionPanel | 1248 | 724 | 488 | 232 | Center | 5 | bars: segmented progress with threshold markers |  |
