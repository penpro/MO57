# Documentation Audit

Status: Inventory in progress.

| Document path | Audience | Purpose | Accuracy | Problems | Recommended action | Action taken |
|---|---|---|---|---|---|---|
| `Docs/PROJECT_STATUS.md` | Maintainers | Status, metrics, audit tracker, recent history | Partially verified | Large and authoritative-looking; individual claims require source/runtime verification | Preserve as active index; cross-check claims during audit | Read; verification pending |
| `README.md` | New contributors/players | Project overview, setup, architecture, authoring | Current as of 2026-07-12 | Previous monolithic-module, CSV-first, stale audit, and automatic-commit guidance removed | Keep concise; point volatile status to `PROJECT_STATUS.md` | Replaced and link-validated |
| `Docs/AUTONOMOUS_TOOLING.md` | Maintainers/automation agents | Tool routing and proven autonomous loop | Mostly current; partially verified | Very long; mixes durable reference with chronological experiments; security boundary needs stronger prominence | Preserve operational core, split historical hit/miss record later, add explicit trust model | Read; core MCP/bridge claims statically verified |
| `Docs/Agent_PIE_Testing.md` | Automation agents | PIE testing playbook | Partially current | Historical sections remain large; UE 5.7 launch and username-specific bridge path were stale | Prefer `ue.py`; later separate historical record from durable recipes | Corrected launch, bridge override, and boot paths |
| `AGENTS.md` | Agents/maintainers | Mandatory engineering and workflow policy | Current controlling guidance | Large but load-bearing; compile and git rules need careful interpretation with explicit user consent | Preserve | Applied during audit |
