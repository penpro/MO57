"""The building menu family in the Field Ledger kit (shared builders: Content/Python/kit_recipes.py; the same screen as crafting over the BuildSystem Widget Blueprints, plus the three
"take materials from" check boxes of UMOBuildingDetailPanel). The old trees are replaced:

    export MSYS_NO_PATHCONV=1
    for a in "WBP_BuildingMenu SizeBox_41" "WBP_BuildingRecipeList SizeBox_0" "WBP_BuildingRecipeEntry SizeBox_61" "WBP_BuildingRecipeDetail SizeBox_50" "WBP_BuildingQueue SizeBox_31" "WBP_BuildQueueEntry SizeBox_82"; do
        set -- $a; python Tools/ue.py ui remove /MOFramework/UI/BuildSystem/$1 $2; done
    python Tools/ue.py ui build Content/Python/ui_specs/kit_building.py
"""
import kit_recipes

SPECS = kit_recipes.building()
