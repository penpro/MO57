#!/usr/bin/env python3
"""
Update Recipes_JSON.json with tool requirements where appropriate.
Uses EMOToolType enum values.
"""

import json

# Tool requirements to add: RecipeId -> [{"ToolType": "X", "MinQuality": Y, "bIsRequired": bool}]
TOOL_REQUIREMENTS = {
    # Stoneworking recipes that need a hammerstone
    "StrikeFlakes": [
        {"ToolType": "Hammer", "MinQuality": 0.3, "bIsRequired": True, "DurabilityConsumed": 1, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "ShapeChopper": [
        {"ToolType": "Hammer", "MinQuality": 0.3, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "ShapeScraper": [
        {"ToolType": "Hammer", "MinQuality": 0.3, "bIsRequired": True, "DurabilityConsumed": 1, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "KnapFlintFlakes": [
        {"ToolType": "Hammer", "MinQuality": 0.4, "bIsRequired": True, "DurabilityConsumed": 1, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "KnapFlintKnife": [
        {"ToolType": "Hammer", "MinQuality": 0.4, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0},
        {"ToolType": "Chisel", "MinQuality": 0.3, "bIsRequired": False, "DurabilityConsumed": 1, "MissingToolTimeMultiplier": 1.5, "MissingToolQualityMultiplier": 0.8}
    ],
    "KnapBifacialHandaxe": [
        {"ToolType": "Hammer", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 3, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0},
        {"ToolType": "Chisel", "MinQuality": 0.4, "bIsRequired": False, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.3, "MissingToolQualityMultiplier": 0.85}
    ],
    "KnapMicroliths": [
        {"ToolType": "Hammer", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0},
        {"ToolType": "Chisel", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    # Woodworking recipes
    "ShapeDiggingStick": [
        {"ToolType": "Knife", "MinQuality": 0.3, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "StripBark": [
        {"ToolType": "Knife", "MinQuality": 0.2, "bIsRequired": False, "DurabilityConsumed": 1, "MissingToolTimeMultiplier": 2.0, "MissingToolQualityMultiplier": 0.9}
    ],
    "MakeFireDrill": [
        {"ToolType": "Knife", "MinQuality": 0.4, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "MakeBowDrill": [
        {"ToolType": "Knife", "MinQuality": 0.4, "bIsRequired": True, "DurabilityConsumed": 3, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "CraftSimpleBow": [
        {"ToolType": "Knife", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 5, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "CraftAtlatl": [
        {"ToolType": "Knife", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 4, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    # Leatherworking recipes
    "ScrapeHide": [
        {"ToolType": "Knife", "MinQuality": 0.4, "bIsRequired": True, "DurabilityConsumed": 3, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "MakeBoneNeedle": [
        {"ToolType": "Knife", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "SewLeatherTunic": [
        {"ToolType": "Needle", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 5, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "SewLeatherTrousers": [
        {"ToolType": "Needle", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 4, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "SewLeatherBoots": [
        {"ToolType": "Needle", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 3, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    # Weapon hafting
    "HaftSpear": [
        {"ToolType": "Knife", "MinQuality": 0.4, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "HaftFlintSpear": [
        {"ToolType": "Knife", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 3, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    # Stone grinding
    "MakePolishedAxe": [
        {"ToolType": "Hammer", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 5, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "MakeGrindingStone": [
        {"ToolType": "Hammer", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 4, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    # Composite tools
    "MakeSickle": [
        {"ToolType": "Knife", "MinQuality": 0.5, "bIsRequired": True, "DurabilityConsumed": 3, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0},
        {"ToolType": "Hammer", "MinQuality": 0.4, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
    "MakeHoe": [
        {"ToolType": "Knife", "MinQuality": 0.4, "bIsRequired": True, "DurabilityConsumed": 2, "MissingToolTimeMultiplier": 1.0, "MissingToolQualityMultiplier": 1.0}
    ],
}

def main():
    input_file = r'D:\UEProjects\MO57\Plugins\MOFramework\Content\Data\Recipes_JSON.json'
    output_file = r'D:\UEProjects\MO57\Plugins\MOFramework\Content\Data\Recipes_JSON_updated.json'

    print(f"Reading {input_file}...")

    # Read with UTF-8 encoding (UE JSON export)
    with open(input_file, 'r', encoding='utf-8-sig') as f:
        recipes = json.load(f)

    print(f"Loaded {len(recipes)} recipes")

    updated_count = 0

    for recipe in recipes:
        recipe_id = recipe.get('Name', '')

        if recipe_id in TOOL_REQUIREMENTS:
            recipe['RequiredTools'] = TOOL_REQUIREMENTS[recipe_id]
            updated_count += 1
            print(f"  Added tools to: {recipe_id}")

    print(f"\nUpdated {updated_count} recipes with tool requirements")

    # Write with UTF-8 encoding to match UE format
    with open(output_file, 'w', encoding='utf-8-sig') as f:
        json.dump(recipes, f, indent='\t', ensure_ascii=False)

    print(f"Written to {output_file}")
    print("\nTo use in UE:")
    print("1. Open UE Editor")
    print("2. Right-click DT_Recipes -> Reimport")
    print("3. Select Recipes_JSON_updated.json")

if __name__ == '__main__':
    main()
