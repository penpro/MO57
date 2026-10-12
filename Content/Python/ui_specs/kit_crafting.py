"""The crafting menu family in the Field Ledger kit (shared builders: Content/Python/kit_recipes.py; design: DesignKit screens/crafting.md). Names are the C++ contracts
(`python Tools/ue.py ui contract MOCraftingMenu` ...); the old trees are replaced:

    export MSYS_NO_PATHCONV=1
    for a in "WBP_CraftingMenu SizeBox_41" "WBP_CraftingRecipeList SizeBox_0" "WBP_CraftingRecipeEntry SizeBox_61" "WBP_CraftingRecipeDetail SizeBox_50" "WBP_CraftingQueue SizeBox_31" "WBP_CraftQueueEntry SizeBox_82"; do
        set -- $a; python Tools/ue.py ui remove /MOFramework/UI/CraftingSystem/$1 $2; done
    python Tools/ue.py ui build Content/Python/ui_specs/kit_crafting.py

(Rebuilding an already converted family needs no removal: the builder updates widgets by name.)
"""
import kit_recipes

SPECS = kit_recipes.crafting()
