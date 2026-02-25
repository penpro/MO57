#pragma once

#include "CoreMinimal.h"
#include "MOResourceNodeDefinitionRow.h"

/**
 * Centralized tag schema for MOFramework.
 *
 * This namespace defines the standard tag formats used throughout the framework
 * for PCG spawning, harvesting, job system resource matching, and UI display.
 *
 * TAG ARCHITECTURE:
 * ================
 * Tags are stored as component tags on HISM components spawned by PCG.
 * The tag system enables:
 * - Context menu display (Name tags)
 * - Resource type classification (MOResource_ tags)
 * - Yield/recipe matching (Gives_ tags)
 * - Job system resource finding (ResourceTags)
 * - Special behavior flags (KeepOnHarvest)
 *
 * TAG FORMATS:
 * ============
 * Display Name:     "Name {DisplayName}"     e.g., "Name Black Alder"
 * Resource Type:    "MOResource_{Type}"      e.g., "MOResource_Tree"
 * Yield Item:       "Gives_{ItemId}"         e.g., "Gives_Bark", "Gives_Stick"
 * Keep on Harvest:  "KeepOnHarvest"          (no suffix)
 * Resource Tags:    Direct names             e.g., "Wood", "Stone", "Fiber"
 *
 * DATA SOURCES:
 * =============
 * All tags are auto-generated from DataTable definitions:
 * - DT_ResourceNodes (FMOResourceNodeDefinitionRow): Resource node definitions
 * - DT_SurvivorJobs (FMOSurvivorJobDefinitionRow): Job resource requirements
 * - DT_Items (FMOItemDefinitionRow): Item tags for legacy matching
 *
 * USAGE:
 * ======
 * @code
 * // Generate tags from resource definition
 * const FMOResourceNodeDefinitionRow* Def = ...;
 * TArray<FName> Tags = Def->GetAllTags();
 *
 * // Check if component matches job requirements
 * const FMOSurvivorJobDefinitionRow* JobDef = ...;
 * if (JobDef->DoesMatchResourceTags(ComponentTags)) { ... }
 *
 * // Use schema utilities for custom tag generation
 * FName NameTag = MOTagSchema::MakeDisplayNameTag(TEXT("Oak Tree"));
 * FName YieldTag = MOTagSchema::MakeYieldTag(FName("Acorn"));
 * @endcode
 */
namespace MOTagSchema
{
	// ============================================================================
	// TAG PREFIXES & CONSTANTS
	// ============================================================================

	/** Prefix for display name tags (e.g., "Name Black Alder"). */
	static const FString DisplayNamePrefix = TEXT("Name ");

	/** Prefix for resource type tags (e.g., "MOResource_Tree"). */
	static const FString ResourceTypePrefix = TEXT("MOResource_");

	/** Prefix for yield item tags (e.g., "Gives_Bark"). */
	static const FString YieldPrefix = TEXT("Gives_");

	/** Prefix for harvest action tags (e.g., "Action_ChopDown"). */
	static const FString ActionPrefix = TEXT("Action_");

	/** Prefix for tool requirement tags (e.g., "RequiresTool_Axe"). */
	static const FString ToolRequirementPrefix = TEXT("RequiresTool_");

	/** Tag for resources that persist after harvesting. */
	static const FName KeepOnHarvestTag = FName(TEXT("KeepOnHarvest"));

	/** Tag indicating no tool is required. */
	static const FName NoToolRequiredTag = FName(TEXT("RequiresTool_None"));

	// ============================================================================
	// RESOURCE TYPE TAGS
	// ============================================================================

	/** Get the standard tag for a resource type. */
	inline FName GetResourceTypeTag(EMOResourceType Type)
	{
		switch (Type)
		{
		case EMOResourceType::Tree:    return FName(TEXT("MOResource_Tree"));
		case EMOResourceType::Bush:    return FName(TEXT("MOResource_Bush"));
		case EMOResourceType::Rock:    return FName(TEXT("MOResource_Rock"));
		case EMOResourceType::Ore:     return FName(TEXT("MOResource_Ore"));
		case EMOResourceType::Plant:   return FName(TEXT("MOResource_Plant"));
		case EMOResourceType::Generic:
		default:                       return FName(TEXT("MOResource_Generic"));
		}
	}

	// ============================================================================
	// TAG GENERATION UTILITIES
	// ============================================================================

	/**
	 * Create a display name tag from text.
	 * @param DisplayName - The display name (e.g., "Black Alder")
	 * @return Tag in format "Name {DisplayName}"
	 */
	inline FName MakeDisplayNameTag(const FString& DisplayName)
	{
		return FName(*(DisplayNamePrefix + DisplayName));
	}

	/**
	 * Create a display name tag from FText.
	 * @param DisplayName - The display name text
	 * @return Tag in format "Name {DisplayName}"
	 */
	inline FName MakeDisplayNameTag(const FText& DisplayName)
	{
		return MakeDisplayNameTag(DisplayName.ToString());
	}

	/**
	 * Create a yield/gives tag from an item ID.
	 * @param ItemId - The item ID (e.g., "Bark", "Acorn")
	 * @return Tag in format "Gives_{ItemId}"
	 */
	inline FName MakeYieldTag(FName ItemId)
	{
		return FName(*(YieldPrefix + ItemId.ToString()));
	}

	/**
	 * Create a resource type tag from enum.
	 * @param Type - The resource type enum
	 * @return Tag in format "MOResource_{Type}"
	 */
	inline FName MakeResourceTypeTag(EMOResourceType Type)
	{
		return GetResourceTypeTag(Type);
	}

	/**
	 * Create a harvest action tag.
	 * @param ActionId - The action ID (e.g., "ChopDown", "Mine")
	 * @return Tag in format "Action_{ActionId}"
	 */
	inline FName MakeActionTag(FName ActionId)
	{
		return FName(*(ActionPrefix + ActionId.ToString()));
	}

	/**
	 * Create a tool requirement tag.
	 * @param ToolType - The tool type (e.g., "Axe", "Pickaxe"). NAME_None for no tool.
	 * @return Tag in format "RequiresTool_{ToolType}" or "RequiresTool_None"
	 */
	inline FName MakeToolRequirementTag(FName ToolType)
	{
		if (ToolType.IsNone())
		{
			return NoToolRequiredTag;
		}
		return FName(*(ToolRequirementPrefix + ToolType.ToString()));
	}

	// ============================================================================
	// TAG PARSING UTILITIES
	// ============================================================================

	/**
	 * Parse the display name from a "Name X" tag.
	 * @param Tag - The tag to parse
	 * @param OutDisplayName - Extracted display name if valid
	 * @return true if tag was a valid display name tag
	 */
	inline bool ParseDisplayNameTag(FName Tag, FString& OutDisplayName)
	{
		FString TagStr = Tag.ToString();
		if (TagStr.StartsWith(DisplayNamePrefix))
		{
			OutDisplayName = TagStr.Mid(DisplayNamePrefix.Len());
			return true;
		}
		return false;
	}

	/**
	 * Parse the item ID from a "Gives_X" tag.
	 * @param Tag - The tag to parse
	 * @param OutItemId - Extracted item ID if valid
	 * @return true if tag was a valid yield tag
	 */
	inline bool ParseYieldTag(FName Tag, FName& OutItemId)
	{
		FString TagStr = Tag.ToString();
		if (TagStr.StartsWith(YieldPrefix))
		{
			OutItemId = FName(*TagStr.Mid(YieldPrefix.Len()));
			return true;
		}
		return false;
	}

	/**
	 * Parse the resource type from a "MOResource_X" tag.
	 * @param Tag - The tag to parse
	 * @param OutType - Extracted resource type if valid
	 * @return true if tag was a valid resource type tag
	 */
	inline bool ParseResourceTypeTag(FName Tag, EMOResourceType& OutType)
	{
		FString TagStr = Tag.ToString();
		if (!TagStr.StartsWith(ResourceTypePrefix))
		{
			return false;
		}

		FString TypeStr = TagStr.Mid(ResourceTypePrefix.Len());
		if (TypeStr == TEXT("Tree"))        { OutType = EMOResourceType::Tree; return true; }
		if (TypeStr == TEXT("Bush"))        { OutType = EMOResourceType::Bush; return true; }
		if (TypeStr == TEXT("Rock"))        { OutType = EMOResourceType::Rock; return true; }
		if (TypeStr == TEXT("Ore"))         { OutType = EMOResourceType::Ore; return true; }
		if (TypeStr == TEXT("Plant"))       { OutType = EMOResourceType::Plant; return true; }
		if (TypeStr == TEXT("Generic"))     { OutType = EMOResourceType::Generic; return true; }

		return false;
	}

	/**
	 * Parse the action ID from a "Action_X" tag.
	 * @param Tag - The tag to parse
	 * @param OutActionId - Extracted action ID if valid
	 * @return true if tag was a valid action tag
	 */
	inline bool ParseActionTag(FName Tag, FName& OutActionId)
	{
		FString TagStr = Tag.ToString();
		if (TagStr.StartsWith(ActionPrefix))
		{
			OutActionId = FName(*TagStr.Mid(ActionPrefix.Len()));
			return true;
		}
		return false;
	}

	/**
	 * Parse the tool type from a "RequiresTool_X" tag.
	 * @param Tag - The tag to parse
	 * @param OutToolType - Extracted tool type if valid (NAME_None if "RequiresTool_None")
	 * @return true if tag was a valid tool requirement tag
	 */
	inline bool ParseToolRequirementTag(FName Tag, FName& OutToolType)
	{
		FString TagStr = Tag.ToString();
		if (TagStr.StartsWith(ToolRequirementPrefix))
		{
			FString ToolStr = TagStr.Mid(ToolRequirementPrefix.Len());
			if (ToolStr == TEXT("None"))
			{
				OutToolType = NAME_None;
			}
			else
			{
				OutToolType = FName(*ToolStr);
			}
			return true;
		}
		return false;
	}

	// ============================================================================
	// TAG QUERY UTILITIES
	// ============================================================================

	/**
	 * Check if a tag array contains a specific resource type.
	 * @param Tags - Array of component tags
	 * @param Type - Resource type to check for
	 * @return true if tags contain the resource type tag
	 */
	inline bool HasResourceType(const TArray<FName>& Tags, EMOResourceType Type)
	{
		return Tags.Contains(GetResourceTypeTag(Type));
	}

	/**
	 * Check if a tag array indicates the resource keeps on harvest.
	 * @param Tags - Array of component tags
	 * @return true if KeepOnHarvest tag is present
	 */
	inline bool HasKeepOnHarvest(const TArray<FName>& Tags)
	{
		return Tags.Contains(KeepOnHarvestTag);
	}

	/**
	 * Extract the display name from a tag array.
	 * @param Tags - Array of component tags
	 * @param OutDisplayName - Extracted display name
	 * @return true if a display name tag was found
	 */
	inline bool ExtractDisplayName(const TArray<FName>& Tags, FString& OutDisplayName)
	{
		for (const FName& Tag : Tags)
		{
			if (ParseDisplayNameTag(Tag, OutDisplayName))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Extract all yield item IDs from a tag array.
	 * @param Tags - Array of component tags
	 * @return Array of item IDs that this resource yields
	 */
	inline TArray<FName> ExtractYieldItems(const TArray<FName>& Tags)
	{
		TArray<FName> YieldItems;
		for (const FName& Tag : Tags)
		{
			FName ItemId;
			if (ParseYieldTag(Tag, ItemId))
			{
				YieldItems.Add(ItemId);
			}
		}
		return YieldItems;
	}

	/**
	 * Check if any of the required tags are present in component tags.
	 * @param ComponentTags - Tags on the component
	 * @param RequiredTags - Tags to look for (matches if ANY are present)
	 * @return true if at least one required tag is found
	 */
	inline bool HasAnyTag(const TArray<FName>& ComponentTags, const TArray<FName>& RequiredTags)
	{
		for (const FName& Required : RequiredTags)
		{
			if (ComponentTags.Contains(Required))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Check if all required tags are present in component tags.
	 * @param ComponentTags - Tags on the component
	 * @param RequiredTags - Tags to look for (matches if ALL are present)
	 * @return true if all required tags are found
	 */
	inline bool HasAllTags(const TArray<FName>& ComponentTags, const TArray<FName>& RequiredTags)
	{
		for (const FName& Required : RequiredTags)
		{
			if (!ComponentTags.Contains(Required))
			{
				return false;
			}
		}
		return true;
	}
}
