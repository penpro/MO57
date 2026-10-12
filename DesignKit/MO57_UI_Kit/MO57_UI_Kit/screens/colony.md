# Colony

Layer: Menu · Input: menu

Grid: Panel 1600x880; roster 560 | character card 968; task queue below roster

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Panel | WBP_MOPanel(elev1) | 160 | 100 | 1600 | 880 | Center | - | bg: color.surface.1; edge: color.border.subtle |  |
| Tabs | WBP_TabBar | 184 | 116 | 1552 | 56 | Center | 1 | tabs: Roster / Tasks / Relationships |  |
| Roster | WBP_PawnRoster | 184 | 188 | 560 | 768 | Center | 2 | row: 72: portrait 56, name, mood, activity, health bar, alerts |  |
| CharacterCard | WBP_CharacterCard(elev2) | 768 | 188 | 968 | 768 | Center | 3 |  |  |
