#!/usr/bin/env python3
"""
Transform Items_JSON.json to use new ToolCapabilities array format.
Also adds secondary tool capabilities where items can serve multiple purposes.
"""

import json
import sys
import re

# Tool capability mappings: item name patterns -> additional capabilities
# Format: (pattern, [(ToolType, Effectiveness), ...])
# These are ADDITIONAL capabilities on top of primary (add Chisel to Hammerstone's primary Hammer)
MULTI_TOOL_MAPPINGS = [
    # Claw hammers can be used as pickaxes for light work
    (r'(?i)claw.*hammer', [('Pickaxe', 0.6)]),

    # Hatchets are small axes - handled in conversion, but add Hammer capability
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
]

# Effectiveness rankings by item quality/material (pattern -> multiplier)
# These modify the BASE effectiveness, not reduce it too much
QUALITY_MODIFIERS = [
    (r'(?i)crude|primitive|makeshift', 0.7),
    (r'(?i)stone|flint|chert', 0.8),  # Stone tools work well for stone-age tasks
    (r'(?i)copper|bronze', 0.9),
    (r'(?i)iron|steel', 1.0),
    (r'(?i)refined|quality|master', 1.1),
]

def get_quality_modifier(item_name):
    """Get effectiveness modifier based on item quality/material."""
    for pattern, modifier in QUALITY_MODIFIERS:
        if re.search(pattern, item_name):
            return modifier
    return 0.9  # Default to decent effectiveness

def get_additional_capabilities(item_name, primary_type):
    """Get additional tool capabilities for an item based on its name."""
    additional = []
    for pattern, caps in MULTI_TOOL_MAPPINGS:
        if re.search(pattern, item_name):
            for tool_type, effectiveness in caps:
                # Don't add if it's the primary type
                if tool_type != primary_type:
                    additional.append((tool_type, effectiveness))
    return additional

def transform_item(item):
    """Transform a single item to use new ToolCapabilities format."""
    if not item.get('bIsTool', False):
        # Not a tool - remove old fields, add empty capabilities
        item.pop('ToolType', None)
        item.pop('ToolQuality', None)
        item['ToolCapabilities'] = []
        return item

    item_name = item.get('DisplayName', '')
    # Extract display name from NSLOCTEXT if present
    match = re.search(r'NSLOCTEXT\([^,]+,\s*[^,]+,\s*"([^"]+)"', item_name)
    if match:
        item_name = match.group(1)

    old_type = item.get('ToolType', 'None')
    old_quality = item.get('ToolQuality', 1.0)

    # Handle Hatchet -> Axe conversion
    if old_type == 'Hatchet':
        old_type = 'Axe'
        old_quality = min(old_quality, 0.9)  # Cap at 0.9 since hatchets are smaller

    capabilities = []

    # Add primary capability if not None
    if old_type and old_type != 'None':
        # For primary capability, use the old quality directly (it was already set appropriately)
        # Only apply quality modifier as a slight adjustment, not a full multiplier
        quality_mod = get_quality_modifier(item_name)
        # Blend: 80% original quality, 20% modified
        effectiveness = min(1.0, old_quality * 0.8 + old_quality * quality_mod * 0.2)
        # Ensure minimum effectiveness of 0.3 for any designated tool
        effectiveness = max(0.3, effectiveness)
        capabilities.append({
            'ToolType': old_type,
            'Effectiveness': round(effectiveness, 2)
        })

        # Add additional capabilities (these use the base patterns, not old_quality)
        additional = get_additional_capabilities(item_name, old_type)
        for add_type, add_eff in additional:
            # Apply quality modifier to additional capabilities
            final_eff = min(1.0, add_eff * quality_mod)
            final_eff = max(0.2, final_eff)  # Minimum 0.2 for secondary capabilities
            capabilities.append({
                'ToolType': add_type,
                'Effectiveness': round(final_eff, 2)
            })

    # Remove old fields
    item.pop('ToolType', None)
    item.pop('ToolQuality', None)

    # Add new field
    item['ToolCapabilities'] = capabilities

    return item

def main():
    input_file = r'D:\UEProjects\MO57\Plugins\MOFramework\Content\Data\Items_JSON.json'
    output_file = r'D:\UEProjects\MO57\Plugins\MOFramework\Content\Data\Items_JSON_updated.json'

    print(f"Reading {input_file}...")

    # Read with UTF-16 encoding (UE exports as UTF-16)
    with open(input_file, 'r', encoding='utf-16') as f:
        items = json.load(f)

    print(f"Loaded {len(items)} items")

    tools_found = 0
    multi_cap_items = 0

    for item in items:
        was_tool = item.get('bIsTool', False)
        transform_item(item)
        if was_tool:
            tools_found += 1
            if len(item.get('ToolCapabilities', [])) > 1:
                multi_cap_items += 1

    print(f"Transformed {tools_found} tools ({multi_cap_items} with multiple capabilities)")

    # Write with UTF-16 encoding to match UE format
    with open(output_file, 'w', encoding='utf-16') as f:
        json.dump(items, f, indent='\t', ensure_ascii=False)

    print(f"Written to {output_file}")
    print("\nTo use:")
    print("1. Open UE Editor")
    print("2. Right-click DT_Items -> Reimport")
    print("3. Select Items_JSON_updated.json")

if __name__ == '__main__':
    main()
