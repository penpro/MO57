# HUD

Layer: Game · Input: game (no focus)

Grid: 40px safe margin; corners own clusters; centre column reserved for reticle/prompt/progress

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| ColonyBar | WBP_ColonyBar | 40 | 40 | 618 | 108 | TopLeft | - | card: 72x108, gap 6; portrait: portrait.size 64; caption: type.caption + outline | max 8 cards, then +N card; hidden when solo |
| ModeIndicator | WBP_ModeIndicator | 40 | 164 | 220 | 36 | TopLeft | - | bg: color.hud.plate; text: type.label; icon: 24 |  |
| TutorialBanner | WBP_TutorialBanner | 680 | 40 | 560 | 112 | TopCenter | - | bg: color.hud.plate; title: type.heading; body: type.body; rule: color.accent 2px top |  |
| Notifications | WBP_NotificationStack | 1480 | 40 | 400 | 312 | TopRight | - | item: 400x72 gap 8; severity: icon 24 + 4px leading bar; text: type.body | max 4 visible; info 4s, warning 8s, critical sticky until resolved |
| QuestTracker | WBP_QuestTracker | 1520 | 392 | 360 | 152 | TopRight | - | bg: color.hud.plate; title: type.body.strong; objective: type.body (wrap); bar: progress 8px |  |
| Reticle | WBP_Reticle | 956 | 536 | 8 | 8 | Center | - | color: color.text.primary; outline: 1px #000 @80% |  |
| InteractionPrompt | WBP_InteractionPrompt | 760 | 572 | 400 | 44 | Center | - | keycap: keycap 32; text: type.hud; bg: color.hud.plate |  |
| TimedAction | WBP_TimedActionProgress | 760 | 628 | 400 | 76 | Center | - | label: type.hud; time: type.numeric; bar: progress 8px data.fill; hint: type.caption text.secondary |  |
| BreathMeter | WBP_BreathMeter | 760 | 872 | 400 | 40 | BottomCenter | - | segments: 10 x 36w gap 4; fill: color.severity.info -> warning <30% -> critical <10% | only while submerged |
| Moodles | WBP_MoodleStrip | 40 | 760 | 300 | 280 | BottomLeft | - | pill: h40, icon 24, type.hud; gap: 8; warning: severity.warning + triangle; critical: severity.critical + octagon + critical_border | stack upward, critical first; >6 = '+N more' pill opens status |
| Wind | WBP_WindIndicator | 1792 | 912 | 88 | 128 | BottomRight | - | ring: wind_ring 64; arrow: wind_arrow 32 rotated to wind-relative-to-camera; speed: type.numeric.small |  |
| Debug | WBP_FPSCounter | 1736 | 8 | 176 | 24 | TopRight | - | text: type.numeric.small; bg: color.hud.plate | dev builds only, above safe area |
