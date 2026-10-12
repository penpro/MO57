# Journal: Quests

Layer: Menu · Input: menu

Grid: Panel 1600x880; list 520 | detail 1008 (paper)

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Panel | WBP_MOPanel(elev1) | 160 | 100 | 1600 | 880 | Center | - | bg: color.surface.1; edge: color.border.subtle |  |
| Tabs | WBP_TabBar | 184 | 116 | 1552 | 56 | Center | 1 |  |  |
| QuestList | WBP_QuestList | 184 | 188 | 520 | 768 | Center | 2 | row: 56; groups: Active / Completed / Failed |  |
| QuestDetail | WBP_QuestDetail(paper) | 728 | 188 | 1008 | 680 | Center | 3 |  |  |
| Actions | WBP_ButtonRow | 728 | 884 | 1008 | 72 | Center | 4 | buttons: Abandon (danger tertiary) | Track (primary) |  |
