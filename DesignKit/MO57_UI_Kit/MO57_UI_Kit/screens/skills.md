# Journal: Skills

Layer: Menu · Input: menu

Grid: Panel 1600x880; tabs; list 920 | knowledge 608 (paper)

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Panel | WBP_MOPanel(elev1) | 160 | 100 | 1600 | 880 | Center | - | bg: color.surface.1; edge: color.border.subtle |  |
| Tabs | WBP_TabBar | 184 | 116 | 1552 | 56 | Center | 1 | tabs: Skills / Knowledge / Quests |  |
| SkillList | WBP_SkillList | 184 | 188 | 920 | 768 | Center | 2 | row: 64: icon, name, level, XP bar with decay marker, 'rusty' tag |  |
| Knowledge | WBP_KnowledgePanel(paper) | 1128 | 188 | 608 | 768 | Center | 3 | bg: paper.surface |  |
