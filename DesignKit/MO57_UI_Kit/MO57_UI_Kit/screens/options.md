# Options

Layer: Menu · Input: menu

Grid: Panel 1280x880 centred (320,100); tab bar 56; rows 56; label col 440, control col 720

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Panel | WBP_MOPanel(elev1) | 320 | 100 | 1280 | 880 | Center | - | bg: color.surface.1; edge: color.border.subtle |  |
| Title | TextBlock | 344 | 116 | 600 | 48 | Center | - | text: type.title |  |
| Tabs | WBP_TabBar | 344 | 172 | 1232 | 56 | Center | 1 | tab: h48 + 4px indicator; LB/RB cycle |  |
| Rows | WBP_SettingsList | 344 | 244 | 1232 | 600 | Center | 2 | row: 56: label body 440w | control 720w | badge; section: type.label |  |
| Footer | WBP_FooterBar | 344 | 868 | 1232 | 88 | Center | 3 | buttons: Defaults (tertiary) | Revert (secondary) | Apply (primary 56) |  |
| KeyCapture | WBP_KeyCaptureModal | 720 | 420 | 480 | 240 | Center | - | text: type.heading 'Press a key for Sprint'; esc: Esc / face_east cancels |  |
