# Tech Debt Report - MO57 / MOFramework
**Date:** 2026-02-01

## Executive Summary

The codebase has solid fundamentals with interface-based decoupling and DataTable-driven design. However, several areas need attention before scaling to multiplayer or expanding features.

**Overall Health: 6.5/10** - Good for early development, needs cleanup before feature expansion.

---

## 1. Critical Issues

| Issue | Location | Impact | Effort |
|-------|----------|--------|--------|
| **MOUIManagerComponent god object** | 2,263 lines | Maintenance nightmare, violates SRP | High |
| **Debug logging in production** | 10 occurrences across 4 files | Performance, log spam | Low |
| **Missing server validation** | `MOCraftingQueueComponent` | Security hole in multiplayer | Medium |

---

## 2. Outstanding TODOs (13 items)

### Medical System (4)
| File | Line | Description |
|------|------|-------------|
| `MOAnatomyComponent.cpp` | 257 | Get treatment definition from DataTable |
| `MOAnatomyComponent.cpp` | 823 | Get condition definition from DataTable |
| `MOAnatomyComponent.cpp` | 968-975 | DataTable lookup via MOMedicalSubsystem (2x) |

### Crafting System (2)
| File | Line | Description |
|------|------|-------------|
| `MOCraftingQueueComponent.cpp` | 396 | Store active station GUID (blocked on Phase 3) |
| `MOCraftingQueueComponent.cpp` | 699 | Apply tool quality modifiers |

### UI System (5)
| File | Line | Description |
|------|------|-------------|
| `MOStatusPanel.cpp` | 190 | Expose threshold setters on StatusField |
| `MOStatusPanel.cpp` | 439 | Add visual selected state to buttons |
| `MOStatusPanel.cpp` | 971 | Show input dialog to change value |
| `MOStatusPanel.cpp` | 980 | Show name change dialog |
| `MOUIManagerComponent.cpp` | 1428 | Implement stack splitting UI |

### Save/Load System (2)
| File | Line | Description |
|------|------|-------------|
| `MOSaveSlotEntry.cpp` | 72 | Load screenshot thumbnail |
| `MOSavePanel.cpp` | 104 | Load actual metadata from save file |

---

## 3. Crafting System Plan Progress

| Phase | Status | Details |
|-------|--------|---------|
| **Phase 1: Core Types & Discovery** | Done | `MOCraftingTypes.h`, `MORecipeDiscoveryComponent`, item/recipe fields |
| **Phase 2: Crafting Queue** | Done | `MOCraftingQueueComponent`, save/load, basic tool validation |
| **Phase 3: Crafting Stations** | Not Started | `AMOCraftingStationActor`, fuel system, interaction |
| **Phase 4: Crafting UI** | Done | All widgets created and wired |
| **Phase 5: Recipe Discovery Methods** | Partial | Basic discovery exists, missing: inspection unlock, skill-level unlock, experimentation |
| **Phase 6: Polish** | Not Started | Sound hooks, notifications, performance |

---

## 4. Architecture Concerns

### God Objects
```
MOUIManagerComponent.cpp    2,263 lines  (should be <500)
MOPersistenceSubsystem.cpp  1,396 lines  (borderline)
MOInventoryComponent.cpp    1,076 lines  (acceptable)
```

**Recommendation**: Split `MOUIManagerComponent` into:
- `MOInventoryUIController`
- `MOCraftingUIController`
- `MOStatusUIController`
- `MOMenuUIController`

### Coupling Issues
1. **Persistence <-> Inventory circular dependency**
   - `MOInventoryComponent.DropItemByGuid()` -> `MOPersistenceSubsystem.IsGuidDestroyed()`
   - Fix: `IMOPersistenceProvider` interface

2. **Monolithic module**
   - All 60+ classes in single `MOFramework` module
   - Future: Split into Core, Interaction, Inventory, Medical, UI submodules

---

## 5. Data Integrity

| Asset | Count | Status |
|-------|-------|--------|
| Items.csv | 200 | Validated |
| Recipes.csv | 92 | Validated, progression verified |
| Skills.csv | 22 | Complete |
| TechProgression.txt | 578 lines | Documentation complete |

**No broken references** between items and recipes.

---

## Next Steps (Priority Order)

### Immediate (This Sprint)
1. **Remove debug logging** - 10 `AddOnScreenDebugMessage` calls
2. **Implement MOMedicalSubsystem DataTable lookups** - Unblock medical system (4 TODOs)
3. **Add stack splitting UI** - Common player request

### Short-Term (Next 2 Sprints)
4. **Phase 3: Crafting Stations** - `AMOCraftingStationActor` with fuel system
5. **Recipe discovery via inspection** - Hook `OnKnowledgeLearned` delegate
6. **Save slot thumbnails** - Screenshot capture on save

### Medium-Term (Backlog)
7. **Split MOUIManagerComponent** - Break into 4 specialized controllers
8. **Server-side craft validation** - Prevent multiplayer exploits
9. **Tool durability degradation** - Consume tools during crafting
10. **Experimentation system** - Blind crafting attempts

### Deferred
- Module splitting (wait until feature-complete)
- Performance optimization (wait until profiling data)

---

## File Locations

- Status Reports: `Plugins/MOFramework/Content/Data/StatusReport_*.md`
- Tech Progression: `Plugins/MOFramework/Content/Data/TechProgression.txt`
- Data CSVs: `Plugins/MOFramework/Content/Data/*.csv`
