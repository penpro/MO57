"""PIE behavior checks for inspectable crafting/building catalog rows."""


def widgets(widget_class):
    return [obj for obj in unreal.ObjectIterator(widget_class) if obj.get_world() == world]


def active_menus(widget_class):
    return [obj for obj in widgets(widget_class) if obj.is_activated()]


def descendants(widget_class, ancestor):
    found = []
    for obj in widgets(widget_class):
        outer = obj.get_outer()
        while outer:
            if outer == ancestor:
                found.append(obj)
                break
            outer = outer.get_outer()
    return found


def attached_descendants(widget_class, ancestor):
    return [obj for obj in descendants(widget_class, ancestor) if obj.get_parent() is not None]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def validate_crafting():
    menus = active_menus(unreal.MOCraftingMenu)
    require(len(menus) == 1, f"expected one crafting menu, found {len(menus)}")

    entries = attached_descendants(unreal.MORecipeEntryWidget, menus[0])
    require(entries, "crafting menu has no recipe entries")
    unavailable = []
    for entry in entries:
        recipe_id = entry.get_recipe_id()
        require(recipe_id != "None", "crafting entry has NAME_None recipe ID")
        require(entry.get_entry_id() == recipe_id, f"base/domain ID mismatch for {recipe_id}")
        if not entry.can_craft():
            unavailable.append(entry)
            require(entry.is_entry_enabled(), f"unavailable recipe {recipe_id} is not selectable")

    require(unavailable, "expected at least one unavailable crafting recipe in fresh-pawn validation")
    target = unavailable[0]
    lists = descendants(unreal.MORecipeListWidget, menus[0])
    details = descendants(unreal.MORecipeDetailPanel, menus[0])
    require(len(lists) == 1 and len(details) == 1, "crafting list/detail widgets not uniquely available")
    recipe_list = lists[0]
    recipe_id = target.get_recipe_id()
    catalog_ids = list(recipe_list.get_entry_ids())
    recipe_list.select_recipe(recipe_id)
    require(details[0].get_displayed_recipe_id() == target.get_recipe_id(), "selection did not propagate to details")
    require(not details[0].can_craft_current_recipe(), "unavailable crafting action became enabled")
    require(not details[0].get_action_unavailable_reason().is_empty(), "unavailable action has no reason")

    recipe_list.populate_recipes(list(reversed(catalog_ids)))
    require(recipe_list.get_selected_recipe_id() == recipe_id, "crafting selection did not survive repopulation")
    require(details[0].get_displayed_recipe_id() == recipe_id, "crafting details changed during preserved refresh")

    recipe_list.populate_recipes([entry_id for entry_id in catalog_ids if entry_id != recipe_id])
    require(recipe_list.get_selected_recipe_id() == "None", "removed crafting selection was not cleared")
    require(details[0].get_displayed_recipe_id() == "None", "crafting details were not cleared with selection")

    recipe_list.populate_recipes(catalog_ids)
    recipe_list.select_recipe(recipe_id)
    initial_count = len(entries)
    menus[0].set_show_all_known(True)
    all_known_entries = attached_descendants(unreal.MORecipeEntryWidget, menus[0])
    require(len(all_known_entries) > initial_count,
            f"show-all-known did not add cross-station recipes ({initial_count} -> {len(all_known_entries)})")
    out(f"[UIStage1PIE] CRAFT PASS rows={initial_count} all_known={len(all_known_entries)} unavailable={len(unavailable)} selected={target.get_recipe_id()}")


def validate_building():
    menus = active_menus(unreal.MOBuildingMenu)
    require(len(menus) == 1, f"expected one building menu, found {len(menus)}")

    entries = attached_descendants(unreal.MOBuildingRecipeEntryWidget, menus[0])
    require(entries, "building menu has no recipe entries")
    unavailable = []
    for entry in entries:
        recipe_id = entry.get_recipe_id()
        require(recipe_id != "None", "building entry has NAME_None recipe ID")
        require(entry.get_entry_id() == recipe_id, f"base/domain ID mismatch for {recipe_id}")
        if not entry.can_build():
            unavailable.append(entry)
            require(entry.is_entry_enabled(), f"material-incomplete building {recipe_id} is not selectable")

    require(unavailable, "expected at least one material-incomplete building on fresh pawn")
    target = unavailable[0]
    lists = descendants(unreal.MOBuildingRecipeListWidget, menus[0])
    details = descendants(unreal.MOBuildingDetailPanel, menus[0])
    require(len(lists) == 1 and len(details) == 1, "building list/detail widgets not uniquely available")
    recipe_list = lists[0]
    recipe_id = target.get_recipe_id()
    catalog_ids = list(recipe_list.get_entry_ids())
    recipe_list.select_recipe(recipe_id)
    require(details[0].get_displayed_recipe_id() == target.get_recipe_id(), "building selection did not propagate")
    require(details[0].can_build_current_recipe(), "material-incomplete building cannot enter placement mode")

    recipe_list.populate_recipes(list(reversed(catalog_ids)))
    require(recipe_list.get_selected_recipe_id() == recipe_id, "building selection did not survive repopulation")
    require(details[0].get_displayed_recipe_id() == recipe_id, "building details changed during preserved refresh")

    recipe_list.populate_recipes([entry_id for entry_id in catalog_ids if entry_id != recipe_id])
    require(recipe_list.get_selected_recipe_id() == "None", "removed building selection was not cleared")
    require(details[0].get_displayed_recipe_id() == "None", "building details were not cleared with selection")

    recipe_list.populate_recipes(catalog_ids)
    recipe_list.select_recipe(recipe_id)
    out(f"[UIStage1PIE] BUILD PASS rows={len(entries)} incomplete={len(unavailable)} selected={target.get_recipe_id()}")


if active_menus(unreal.MOCraftingMenu):
    validate_crafting()
elif active_menus(unreal.MOBuildingMenu):
    validate_building()
else:
    raise RuntimeError("open crafting or building menu before running UI Stage 1 PIE validation")
