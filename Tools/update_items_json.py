#!/usr/bin/env python3
"""
Update Items_JSON.json with:
1. ToolCapabilities array (from ToolType/ToolQuality)
2. bIsWeapon and WeaponProfileId for weapon items
"""

import json
import re

# Weapon items that need weapon profiles linked
WEAPON_PROFILES = {
    "CrudeClub01": "CrudeClub01",
    "ThrowingStone01": "ThrowingStone01",
    "WoodenSpear01": "WoodenSpear01",
    "FireHardenedSpear01": "FireHardenedSpear01",
    "FlintSpear01": "FlintSpear01",
    "SimpleBow01": "SimpleBow01",
    "CompositeBow01": "CompositeBow01",
    "SinewBackedBow01": "SinewBackedBow01",
    "Atlatl01": "Atlatl01",
    "BronzeSword01": "BronzeSword01",
    "IronSword01": "IronSword01",
    "SteelSword01": "SteelSword01",
    "BronzeSpear01": "BronzeSpear01",
    "IronSpear01": "IronSpear01",
}

# Tool capability mappings: item name patterns -> [(ToolType, Effectiveness), ...]
MULTI_TOOL_MAPPINGS = [
    # Claw hammers can be used as pickaxes for light work
    (r'(?i)claw.*hammer', [('Pickaxe', 0.6)]),
    # Hatchets are small axes - add Hammer capability
    (r'(?i)hatchet', [('Hammer', 0.4)]),
    # Hammerstones can also chisel/knap
    (r'(?i)hammer\s*stone|hammerstone', [('Chisel', 0.6)]),
    # Sharp rocks/stones can cut and pound
    (r'(?i)sharp.*rock|sharp.*stone', [('Knife', 0.5), ('Hammer', 0.3)]),
    # Generic rocks/cobbles can hammer
    (r'(?i)^rock$|^stone$|cobble', [('Hammer', 0.4)]),
    # Large knives can work as cleavers
    (r'(?i)large.*knife|hunting.*knife|bowie', [('Cleaver', 0.6)]),
    # Machetes work as axes and knives
    (r'(?i)machete', [('Axe', 0.7), ('Knife', 0.8)]),
    # Pickaxes can work as crude shovels
    (r'(?i)pickaxe|pick', [('Shovel', 0.5)]),
    # Shovels can work as hoes
    (r'(?i)shovel', [('Hoe', 0.6)]),
    # Axes can work as crude hammers (use the back)
    (r'(?i)axe', [('Hammer', 0.5)]),
    # Mallets are primarily hammers but can work as crude axes
    (r'(?i)mallet', [('Axe', 0.3)]),
    # Bow drill / fire starters
    (r'(?i)bow.*drill|fire.*bow', [('Firestarter', 0.9)]),
    # Flint/chert can be used as knives or firestarters
    (r'(?i)flint|chert', [('Knife', 0.5), ('Firestarter', 0.7)]),
    # Hand axes can be used as hammers
    (r'(?i)hand.*axe', [('Hammer', 0.5)]),
]

# Quality modifiers by material
QUALITY_MODIFIERS = [
    (r'(?i)crude|primitive|makeshift', 0.7),
    (r'(?i)stone|flint|chert', 0.8),
    (r'(?i)copper|bronze', 0.9),
    (r'(?i)iron|steel', 1.0),
    (r'(?i)refined|quality|master', 1.1),
]

def get_quality_modifier(item_name):
    for pattern, modifier in QUALITY_MODIFIERS:
        if re.search(pattern, item_name):
            return modifier
    return 0.9

def get_additional_capabilities(item_name, primary_type):
    additional = []
    for pattern, caps in MULTI_TOOL_MAPPINGS:
        if re.search(pattern, item_name):
            for tool_type, effectiveness in caps:
                if tool_type != primary_type:
                    additional.append((tool_type, effectiveness))
    return additional

def transform_item(item):
    """Transform a single item with new fields."""
    item_name = item.get('Name', '')
    display_name = item.get('DisplayName', '')

    # Extract display name from NSLOCTEXT if present
    match = re.search(r'NSLOCTEXT\([^,]+,\s*[^,]+,\s*"([^"]+)"', display_name)
    if match:
        display_name = match.group(1)

    # Handle weapon profile linking
    if item_name in WEAPON_PROFILES:
        item['bIsWeapon'] = True
        item['WeaponProfileId'] = WEAPON_PROFILES[item_name]
    else:
        if 'bIsWeapon' not in item:
            item['bIsWeapon'] = False
        if 'WeaponProfileId' not in item:
            item['WeaponProfileId'] = "None"

    # Handle tool capabilities
    is_tool = item.get('bIsTool', False)

    if is_tool:
        old_type = item.get('ToolType', 'None')
        old_quality = item.get('ToolQuality', 1.0)

        # Convert Hatchet to Axe
        if old_type == 'Hatchet':
            old_type = 'Axe'
            old_quality = min(old_quality, 0.9)

        capabilities = []

        # Add primary capability
        if old_type and old_type != 'None':
            quality_mod = get_quality_modifier(display_name)
            effectiveness = min(1.0, old_quality * 0.8 + old_quality * quality_mod * 0.2)
            effectiveness = max(0.3, effectiveness)
            capabilities.append({
                'ToolType': old_type,
                'Effectiveness': round(effectiveness, 2)
            })

            # Add additional capabilities
            additional = get_additional_capabilities(display_name, old_type)
            for add_type, add_eff in additional:
                final_eff = min(1.0, add_eff * quality_mod)
                final_eff = max(0.2, final_eff)
                capabilities.append({
                    'ToolType': add_type,
                    'Effectiveness': round(final_eff, 2)
                })

        item['ToolCapabilities'] = capabilities
    else:
        if 'ToolCapabilities' not in item:
            item['ToolCapabilities'] = []

    # Remove old fields
    item.pop('ToolType', None)
    item.pop('ToolQuality', None)

    return item

def main():
    input_file = r'D:\UEProjects\MO57\Plugins\MOFramework\Content\Data\Items_JSON.json'
    output_file = r'D:\UEProjects\MO57\Plugins\MOFramework\Content\Data\Items_JSON_updated.json'

    print(f"Reading {input_file}...")

    # Read with UTF-8 encoding (UE JSON export)
    with open(input_file, 'r', encoding='utf-8-sig') as f:
        items = json.load(f)

    print(f"Loaded {len(items)} items")

    tools_found = 0
    weapons_found = 0
    multi_cap_items = 0

    for item in items:
        was_tool = item.get('bIsTool', False)
        item_name = item.get('Name', '')

        transform_item(item)

        if was_tool:
            tools_found += 1
            if len(item.get('ToolCapabilities', [])) > 1:
                multi_cap_items += 1

        if item.get('bIsWeapon', False):
            weapons_found += 1

    print(f"Transformed {tools_found} tools ({multi_cap_items} with multiple capabilities)")
    print(f"Linked {weapons_found} weapons to profiles")

    # Write with UTF-8 encoding to match UE format
    with open(output_file, 'w', encoding='utf-8-sig') as f:
        json.dump(items, f, indent='\t', ensure_ascii=False)

    print(f"Written to {output_file}")
    print("\nTo use in UE:")
    print("1. Open UE Editor")
    print("2. Right-click DT_Items -> Reimport")
    print("3. Select Items_JSON_updated.json")

if __name__ == '__main__':
    main()
