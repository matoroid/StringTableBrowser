// Copyright (c) 2026 Mato Marion. All Rights Reserved.

#include "StringTableBrowserModule.h"

#include "Editor.h"
#include "FTextStringTableBrowserDetailCustomization.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "SStringTableBrowser.h"
#include "StringTableBrowserSettings.h"
#include "StringTableBrowserStyle.h"
#include "StringTableBrowserTypes.h"
#include "ToolMenus.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ObjectSaveContext.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "StringTableBrowserModule"

DEFINE_LOG_CATEGORY(LogStringTableBrowser);

// -------------------------------------------------------------------------
// Tab spawner
// -------------------------------------------------------------------------

TSharedRef<SDockTab> FStringTableBrowserModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SStringTableBrowser)
		];
}

void FStringTableBrowserModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("StringTableBrowser"));
}

void FStringTableBrowserModule::RegisterMenus()
{
	UToolMenus* ToolMenus = UToolMenus::Get();

	// Helper to avoid repeating the entry definition for each menu target
	auto RegisterInToolsMenu = [&](const FName& MenuName)
	{
		UToolMenu* ToolsMenu = ToolMenus->ExtendMenu(MenuName);
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection("Tools");

		Section.AddMenuEntry(
			"OpenStringTableBrowser",
			LOCTEXT("OpenStringTableBrowser", "String Table Browser"),
			LOCTEXT("OpenStringTableBrowserTooltip", "Open the String Table Browser panel."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), StringTableBrowserIcons::OpenBrowserSearch),
		FUIAction(FExecuteAction::CreateRaw(this, &FStringTableBrowserModule::PluginButtonClicked)
			)
		);
	};

	// Level Editor Tools menu — the primary entry point
	RegisterInToolsMenu("LevelEditor.MainMenu.Tools");

	// String Table asset editor Tools menu
	RegisterInToolsMenu("AssetEditor.StringTableEditor.MainMenu.Tools");

	// Widget Blueprint editor Tools menu
	RegisterInToolsMenu("AssetEditor.WidgetBlueprintEditor.MainMenu.Tools");
}

// -------------------------------------------------------------------------
// Module lifecycle
// -------------------------------------------------------------------------

void FStringTableBrowserModule::StartupModule()
{
	// Use a single AssetRegistry reference throughout startup to avoid loading
	// the module more than once and to ensure all bindings share the same instance.
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// Attempt to warm up from the disk cache first.
	// On failure (first run, schema change, or corruption) wait for the registry
	// to finish its initial scan before doing a full rebuild.
	if (!LoadCacheFromDisk())
	{
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			AssetRegistryModule.Get().OnFilesLoaded().AddRaw(
				this, &FStringTableBrowserModule::OnAssetRegistryFilesLoaded);
		}
		else
		{
			ForceRebuildCache();
		}
	}

	// Subscribe to incremental Asset Registry events for live updates
	AssetRegistryModule.Get().OnAssetAdded().AddRaw(  this, &FStringTableBrowserModule::OnAssetAdded);
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw( this, &FStringTableBrowserModule::OnAssetRemoved);
	AssetRegistryModule.Get().OnAssetUpdated().AddRaw( this, &FStringTableBrowserModule::OnAssetUpdated);
	
	// Subscribe to package save updates to ensure references are updated after individual key edits.
	UPackage::PackageSavedWithContextEvent.AddRaw(this, &FStringTableBrowserModule::OnPackageSaved);

	FPropertyEditorModule& PropertyModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Extension bar — always bound, but internally checks the setting
	// before adding any buttons so only one path is active at a time.
	GlobalRowExtensionHandle =
		PropertyModule.GetGlobalRowExtensionDelegate().AddStatic(
			&FTextStringTableBrowserDetailCustomization::OnGeneratePropertyRowExtension
		);

	PropertyModule.NotifyCustomizationModuleChanged();
	
	// Register the tab spawner
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		FName("StringTableBrowser"),
		FOnSpawnTab::CreateRaw(this, &FStringTableBrowserModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("StringTableBrowserTab", "String Table Browser"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), StringTableBrowserIcons::OpenBrowserSearch))
		.SetMenuType(ETabSpawnerMenuType::Hidden // Hidden because we manage the menu entry ourselves
	); 

	// Register menus (deferred so the editor toolbar is ready)
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FStringTableBrowserModule::RegisterMenus)
	);
}

void FStringTableBrowserModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);

	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		IAssetRegistry& AssetRegistry =
			FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

		AssetRegistry.OnFilesLoaded().RemoveAll(this);
		AssetRegistry.OnAssetAdded().RemoveAll(this);
		AssetRegistry.OnAssetRemoved().RemoveAll(this);
		AssetRegistry.OnAssetUpdated().RemoveAll(this);
	}
	
	UPackage::PackageSavedWithContextEvent.RemoveAll(this);

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		
		PropertyModule.GetGlobalRowExtensionDelegate().Remove(GlobalRowExtensionHandle);
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	if (ActiveStreamableHandle.IsValid())
	{
		ActiveStreamableHandle->CancelHandle();
		ActiveStreamableHandle.Reset();
	}
	
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::Get()->UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FName("StringTableBrowser"));
	FStringTableBrowserStyle::Shutdown(); 
}

// -------------------------------------------------------------------------
// Cache — disk persistence
// -------------------------------------------------------------------------

FString FStringTableBrowserModule::GetCacheFilePath() const
{
	return FPaths::ProjectSavedDir() / TEXT("StringTableBrowserCache.json");
}

bool FStringTableBrowserModule::LoadCacheFromDisk()
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *GetCacheFilePath()))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	// Reject caches written by an older or newer schema version
	int32 CachedVersion = 0;
	RootObject->TryGetNumberField(TEXT("Version"), CachedVersion);
	if (CachedVersion != GStringTableBrowserCacheVersion)
	{
		UE_LOG(LogStringTableBrowser, Log,
			TEXT("StringTableBrowser: Cache version mismatch (stored=%d, expected=%d). Rebuilding."),
			CachedVersion, GStringTableBrowserCacheVersion
		);
		return false;
	}

	FScopeLock Lock(&CacheLock);
	GroupedCache.Empty();

	const TArray<TSharedPtr<FJsonValue>>* JsonTables = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("Tables"), JsonTables))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& TableValue : *JsonTables)
	{
		const TSharedPtr<FJsonObject> TableObject = TableValue->AsObject();
		if (!TableObject.IsValid())
		{
			continue;
		}

		const FName  PackageName(TableObject->GetStringField(TEXT("PackageName")));
		const FName  TableId(TableObject->GetStringField(TEXT("TableId")));
		const FSoftObjectPath AssetPath(TableObject->GetStringField(TEXT("AssetPath")));

		TArray<TSharedPtr<FStringTableBrowserEntry>> Entries;

		const TArray<TSharedPtr<FJsonValue>>* JsonEntries = nullptr;
		if (TableObject->TryGetArrayField(TEXT("Entries"), JsonEntries))
		{
			for (const TSharedPtr<FJsonValue>& EntryValue : *JsonEntries)
			{
				const TSharedPtr<FJsonObject> EntryObject = EntryValue->AsObject();
				if (!EntryObject.IsValid())
				{
					continue;
				}

				TSharedPtr<FStringTableBrowserEntry> Entry = MakeShared<FStringTableBrowserEntry>();
				Entry->TableId   = TableId;
				Entry->AssetPath = AssetPath;
				Entry->Key       = EntryObject->GetStringField(TEXT("Key"));
				Entry->Value     = EntryObject->GetStringField(TEXT("Value"));
				Entries.Add(Entry);
			}
		}

		GroupedCache.Add(PackageName, MoveTemp(Entries));
	}

	RebuildFlatCache();
	return true;
}

void FStringTableBrowserModule::SaveCacheToDisk()
{
	// Snapshot under lock, then do all I/O outside the lock to minimise contention
	TMap<FName, TArray<TSharedPtr<FStringTableBrowserEntry>>> CacheSnapshot;
	{
		{
			FScopeLock Lock(&CacheLock);
			CacheSnapshot = GroupedCache;
		}
	}

	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("Version"), GStringTableBrowserCacheVersion);

	TArray<TSharedPtr<FJsonValue>> JsonTables;
	for (const auto& Pair : CacheSnapshot)
	{
		TSharedPtr<FJsonObject> TableObject = MakeShared<FJsonObject>();
		TableObject->SetStringField(TEXT("PackageName"), Pair.Key.ToString());

		if (Pair.Value.Num() > 0)
		{
			TableObject->SetStringField(TEXT("TableId"),   Pair.Value[0]->TableId.ToString());
			TableObject->SetStringField(TEXT("AssetPath"), Pair.Value[0]->AssetPath.ToString());
		}

		TArray<TSharedPtr<FJsonValue>> JsonEntries;
		for (const TSharedPtr<FStringTableBrowserEntry>& Entry : Pair.Value)
		{
			TSharedPtr<FJsonObject> EntryObject = MakeShared<FJsonObject>();
			EntryObject->SetStringField(TEXT("Key"),   Entry->Key);
			EntryObject->SetStringField(TEXT("Value"), Entry->Value);
			JsonEntries.Add(MakeShared<FJsonValueObject>(EntryObject));
		}

		TableObject->SetArrayField(TEXT("Entries"), JsonEntries);
		JsonTables.Add(MakeShared<FJsonValueObject>(TableObject));
	}

	RootObject->SetArrayField(TEXT("Tables"), JsonTables);

	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		FFileHelper::SaveStringToFile(OutputString, *GetCacheFilePath());
	}
	
	BroadcastCacheUpdated();
}

void FStringTableBrowserModule::ScheduleDiskCacheSave()
{
	bDiskCacheDirty = true;

	// Reset the timer on every call — only fires after the burst settles
	if (GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(DiskSaveTimerHandle);
		GEditor->GetTimerManager()->SetTimer(
			DiskSaveTimerHandle,
			[this]()
			{
				if (bDiskCacheDirty)
				{
					SaveCacheToDisk();
					bDiskCacheDirty = false;
				}
			},
			UStringTableBrowserSettings::Get()->SaveCacheToDiskDelay, /*InRate*/
			false /*InbLoop*/
		);
	}
}

// -------------------------------------------------------------------------
// Cache — rebuild
// -------------------------------------------------------------------------

void FStringTableBrowserModule::ForceRebuildCache()
{
    const UStringTableBrowserSettings* Settings = UStringTableBrowserSettings::Get();
    const bool bForceLoad = Settings && Settings->bForceLoadStringTables;

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    TArray<FAssetData> AssetDataList;
    AssetRegistryModule.Get().GetAssetsByClass(
        UStringTable::StaticClass()->GetClassPathName(), AssetDataList);

    if (bForceLoad)
    {
        // Collect paths for assets not already in memory
        TArray<FSoftObjectPath> PathsToLoad;
        for (const FAssetData& Data : AssetDataList)
        {
            if (!FindObject<UStringTable>(nullptr, *Data.GetObjectPathString()))
            {
                PathsToLoad.Add(Data.ToSoftObjectPath());
            }
        }

        if (PathsToLoad.Num() > 0)
        {
            // Store the full asset list for use in the callback
            PendingRebuildAssetList = AssetDataList;

            FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
            ActiveStreamableHandle = Streamable.RequestAsyncLoad(
                PathsToLoad,
                FStreamableDelegate::CreateRaw(
                    this, &FStringTableBrowserModule::OnForceLoadComplete));

            // Return early — cache rebuild runs in OnForceLoadComplete
            return;
        }
    }

    // No force-load needed or all assets already resident — rebuild synchronously
    RebuildCacheFromLoadedAssets(AssetDataList);
}

void FStringTableBrowserModule::OnForceLoadComplete()
{
    // All requested assets are now resident — run the cache build
    RebuildCacheFromLoadedAssets(PendingRebuildAssetList);
    PendingRebuildAssetList.Empty();
    ActiveStreamableHandle.Reset();
}

void FStringTableBrowserModule::RebuildCacheFromLoadedAssets(
    const TArray<FAssetData>& AssetDataList
)
{
    {
        FScopeLock Lock(&CacheLock);
        GroupedCache.Empty();

        for (const FAssetData& Data : AssetDataList)
        {
            UStringTable* Table = Cast<UStringTable>(
                FindObject<UStringTable>(nullptr, *Data.GetObjectPathString()));

            if (Table)
            {
                CacheSingleStringTable(Data, Table);
            }
        }

        RebuildFlatCache();
    }

    ScheduleDiskCacheSave();
}

// -------------------------------------------------------------------------
// Cache — Asset Registry event handlers
// -------------------------------------------------------------------------

void FStringTableBrowserModule::OnAssetRegistryFilesLoaded()
{
	// Self-remove immediately — this delegate must fire at most once
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().OnFilesLoaded().RemoveAll(this);
	}

	ForceRebuildCache();
}

void FStringTableBrowserModule::OnAssetAdded(const FAssetData& AssetData)
{
	// Do not process events that arrive during the initial editor scan
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		return;
	}

	if (AssetData.AssetClassPath != UStringTable::StaticClass()->GetClassPathName())
	{
		return;
	}

	UStringTable* Table = Cast<UStringTable>(FindObject<UStringTable>(nullptr, *AssetData.GetObjectPathString()));

	if (!Table)
	{
		const UStringTableBrowserSettings* Settings = UStringTableBrowserSettings::Get();
		if (Settings && Settings->bForceLoadStringTables)
		{
			// Single asset — synchronous load is acceptable here
			Table = Cast<UStringTable>(AssetData.GetAsset());
		}
	}

	if (Table)
	{
		{
			FScopeLock Lock(&CacheLock);
			CacheSingleStringTable(AssetData, Table);
		}
		ScheduleDiskCacheSave();
	}
}

void FStringTableBrowserModule::OnAssetRemoved(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath != UStringTable::StaticClass()->GetClassPathName())
	{
		return;
	}

	{
		FScopeLock Lock(&CacheLock);
		RemoveStringTableFromCache(AssetData.PackageName);
		RebuildFlatCache();
	}

	ScheduleDiskCacheSave();
}

void FStringTableBrowserModule::OnAssetUpdated(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath != UStringTable::StaticClass()->GetClassPathName())
	{
		return;
	}

	UStringTable* Table = Cast<UStringTable>(
		FindObject<UStringTable>(nullptr, *AssetData.GetObjectPathString()));

	if (Table)
	{
		{
			FScopeLock Lock(&CacheLock);
			RemoveStringTableFromCache(AssetData.PackageName);
			CacheSingleStringTable(AssetData, Table);
			RebuildFlatCache();
		}

		ScheduleDiskCacheSave();
	}
}

void FStringTableBrowserModule::OnPackageSaved(
	const FString& PackageFilename, 
	UPackage* Package,
	FObjectPostSaveContext ObjectSaveContext
)
{
	// Walk the package to find a UStringTable object.
	// Most string table packages contain exactly one.
	UStringTable* Table = nullptr;
	ForEachObjectWithPackage(Package, 
		[&Table](UObject* Object) -> bool
		{
			Table = Cast<UStringTable>(Object);
			return Table == nullptr; // returning false stops iteration
		},
		false /*bIncludeNestedObjects=*/
	);

	if (!Table)
	{
		return;
	}
	
	{
		const FAssetData AssetData(Table);
		FScopeLock Lock(&CacheLock);
		RemoveStringTableFromCache(AssetData.PackageName);
		CacheSingleStringTable(AssetData, Table);
		RebuildFlatCache();
	}

	ScheduleDiskCacheSave();
}

// -------------------------------------------------------------------------
// Cache — internal helpers (CacheLock must be held by the caller)
// -------------------------------------------------------------------------

void FStringTableBrowserModule::CacheSingleStringTable(
	const FAssetData& AssetData,
	const UStringTable* Table
)
{
	TArray<TSharedPtr<FStringTableBrowserEntry>> TableEntries;

	Table->GetStringTable()->EnumerateSourceStrings(
		[&](const FString& InKey, const FString& InSourceString)
		{
			TSharedPtr<FStringTableBrowserEntry> Entry = MakeShared<FStringTableBrowserEntry>();
			Entry->TableId = AssetData.AssetName;
			Entry->AssetPath = AssetData.ToSoftObjectPath();
			Entry->Key = InKey;
			Entry->Value = InSourceString;
			TableEntries.Add(Entry);
			return true; // continue enumeration
		}
	);

	GroupedCache.Add(AssetData.PackageName, MoveTemp(TableEntries));
}

void FStringTableBrowserModule::RemoveStringTableFromCache(const FName& PackageName)
{
	GroupedCache.Remove(PackageName);
}

void FStringTableBrowserModule::RebuildFlatCache()
{
	FlatCache.Empty();
	for (const auto& Pair : GroupedCache)
	{
		FlatCache.Append(Pair.Value);
	}
}

void FStringTableBrowserModule::BroadcastCacheUpdated()
{
	if (IsInGameThread())
	{
		OnCacheUpdated.Broadcast();
	}
	else
	{
		// Asset Registry callbacks can arrive on background threads in some engine versions.
		// Slate is not thread-safe, so always marshal broadcasts to the game thread.
		AsyncTask(ENamedThreads::GameThread, 
			[]()
			{
				if (auto* const Module = FStringTableBrowserModule::GetModulePtr())
				{
					Module->OnCacheUpdated.Broadcast();
				}
			}
		);
	}
}

IMPLEMENT_MODULE(FStringTableBrowserModule, StringTableBrowser)

#undef LOCTEXT_NAMESPACE
