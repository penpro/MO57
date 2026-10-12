# In-game menu + Save/Load

Layer: Menu · Input: menu (game keeps running)

Grid: Scrim 72%; left rail 400 at x=120; world-continues readout top; save/load panel 960 at x=600

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Scrim | Border | 0 | 0 | 1920 | 1080 | Fill | - | bg: color.overlay.scrim |  |
| WorldClock | WBP_WorldContinues | 120 | 96 | 400 | 112 | TopLeft | - | time: type.numeric.large; label: type.caption; pulse: clock icon ticks each in-game minute |  |
| Nav | WBP_MenuRail | 120 | 240 | 400 | 400 | Left | 1 | button: 56 |  |
| SavePanel | WBP_SaveLoadPanel(elev1) | 600 | 96 | 1200 | 888 | Center | 2 |  |  |
| SlotCard | WBP_SaveSlotCard | 624 | 168 | 1152 | 120 | Center | - | thumb: 192x108; name: type.heading; meta: type.caption + type.numeric.small |  |
