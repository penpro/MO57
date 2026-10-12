# Modals, context menus, states

Layer: Modal · Input: modal (captures all)

Grid: Modal widths 480 / 640; 24 padding; footer buttons right-aligned, primary rightmost

| id | widget | x | y | w | h | anchor | focus | tokens | note |
|---|---|---|---|---|---|---|---|---|---|
| Confirm | WBP_ConfirmModal | 720 | 380 | 480 | auto | Center | 1 | title: type.heading; body: type.body; buttons: 2-3, h56 | default focus = the SAFE option |
| TextInput | WBP_TextInputModal | 640 | 360 | 640 | auto | Center | - |  |  |
| QuickPick | WBP_QuickPickModal | 720 | 300 | 480 | auto | Center | - | row: 48 |  |
| Toast | WBP_Toast | 1480 | 40 | 400 | 72 | TopRight | - |  |  |
| BugReport | WBP_BugReportModal | 560 | 140 | 800 | 800 | Center | - | fields: summary input, category dropdown, details multiline, attach-screenshot toggle, log toggle |  |
| ContextMenu | WBP_ContextMenu | 0 | 0 | 280 | auto | Cursor | - | row: 44; keys: right-aligned keycap hints |  |
