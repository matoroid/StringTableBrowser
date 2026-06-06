// Copyright (c) 2026 Mato Marion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "StringTableBrowserSettings.generated.h"

/**
 * UStringTableBrowserSettings
 *
 * Developer settings for the String Table Browser plugin.
 * Accessible via Edit → Project Settings → Plugins → String Table Browser.
 *
 * Settings are stored in DefaultStringTableBrowser.ini.
 */
UCLASS(Config=StringTableBrowser, DefaultConfig, meta=(DisplayName="String Table Browser"))
class UStringTableBrowserSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:

    UStringTableBrowserSettings()
    {
        CategoryName = "Plugins";
        SectionName = "String Table Browser";
    }

    /** Returns the singleton settings instance. */
    static const UStringTableBrowserSettings* Get()
    {
        return GetDefault<UStringTableBrowserSettings>();
    }

	/**
	* Controls the delay for debouncing the search after the user finishes typing on the searchbar.
	*/
	UPROPERTY(Config, EditAnywhere, Category="Seach", meta=(DisplayName="Search Debounce Delay"))
	float SearchDebounceDelay = 0.15f;
	
	/**
	* Controls the delay for saving the updated cache to the disk.
	* This delay is triggered when we need to write the saved cache, to avoid writing in rapid succession
	* if the data asset registries are fired in bursts.
	*/
	UPROPERTY(Config, EditAnywhere, Category="Cache", meta=(DisplayName="Save Cache to Disk Delay."))
	float SaveCacheToDiskDelay = 1.5f;

	/**
	 * When enabled, ForceRebuildCache loads all string table assets into memory
	 * via the async streamable manager before building the cache. This guarantees
	 * complete cache coverage at the cost of a background load on cache build.
	 * Recommended for projects where string tables are not kept loaded in memory.
	 * Disable on very large projects if the load time is unacceptable.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Cache", meta=(DisplayName="Force Load String Tables on Cache Build"))
	bool bForceLoadStringTables = true;
};