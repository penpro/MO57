# Main menu

Layer: Menu · Input: menu

Grid: Left rail 480 from x=120; content panel 760 at x=720

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Wordmark | TextBlock | 120 | 120 | 480 | 72 | Left | - | text: type.display, letter-spacing 0.12em |  |
| Nav | WBP_MenuRail | 120 | 300 | 400 | 456 | Left | 1 | button: 56h primary-tertiary, gap 8 |  |
| ContentPanel | WBP_MOPanel(elev1) | 720 | 200 | 760 | 680 | Center | 2 | bg: surface.1 |  |
| SeedInput | WBP_TextInput+IconButton(random) | 752 | 360 | 696 | 56 | Center | 3 |  |  |
| SessionList | WBP_SessionList | 752 | 360 | 696 | 400 | Center | - | row: 56: host, players 2/4, ping, version | Join tab |
| Version | TextBlock | 120 | 1016 | 400 | 24 | BottomLeft | - | text: type.caption text.secondary |  |
| LoadingOverlay | WBP_LoadingOverlay | 0 | 0 | 1920 | 1080 | Fill | - | bg: surface.0; bar: progress 4px at bottom 64; tip: type.body |  |
