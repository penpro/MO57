#!/usr/bin/env python3
"""
Migrate Items_JSON.json from old schema to new schema.

Old schema:
  - bIsTool: bool
  - ToolType: string enum ("None", "Axe", "Hammer", etc.)
  - ToolQuality: float (0.0-1.0)

New schema:
  - bIsTool: bool (kept)
  - ToolCapabilities: array of {ToolType: string, Effectiveness: float}
  - bIsWeapon: bool (new, default false)
  - WeaponProfileId: string (new, default "")
"""

import json
import sys
from pathlib import Path

def migrate_item(item: dict) -> dict:
    """Migrate a single item to the new schema."""

    # Extract old values (if present - may have been migrated already)
    tool_type = item.pop("ToolType", None)
    tool_quality = item.pop("ToolQuality", None)
    is_tool = item.get("bIsTool", False)

    # Build ToolCapabilities array if old format detected
    if tool_type is not None:
        tool_capabilities = []
        if is_tool and tool_type != "None":
            # Migrate Hatchet -> Axe per KNOWN PITFALLS
            if tool_type == "Hatchet":
                tool_type = "Axe"
            tool_capabilities.append({
                "ToolType": tool_type,
                "Effectiveness": tool_quality if tool_quality is not None else 1.0
            })
        item["ToolCapabilities"] = tool_capabilities

    # Also fix any existing ToolCapabilities with Hatchet
    if "ToolCapabilities" in item:
        for cap in item["ToolCapabilities"]:
            if cap.get("ToolType") == "Hatchet":
                cap["ToolType"] = "Axe"

    # Add new fields if missing
    if "bIsWeapon" not in item:
        item["bIsWeapon"] = False
    if "WeaponProfileId" not in item:
        item["WeaponProfileId"] = ""

    return item

def main():
    # Path to the JSON file
    json_path = Path(__file__).parent.parent / "Plugins/MOFramework/Content/Data/Items_JSON.json"

    if not json_path.exists():
        print(f"Error: {json_path} not found")
        sys.exit(1)

    # Read the JSON file
    print(f"Reading {json_path}...")
    with open(json_path, 'r', encoding='utf-8-sig') as f:
        items = json.load(f)

    print(f"Found {len(items)} items")

    # Migrate each item
    migrated_count = 0
    for item in items:
        migrate_item(item)
        migrated_count += 1

    print(f"Processed {migrated_count} items")

    # Write back
    print(f"Writing {json_path}...")
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(items, f, indent='\t', ensure_ascii=False)

    print("Done!")

if __name__ == "__main__":
    main()
