# String Table Browser — Development Challenges & Fixes

A summary of every significant issue encountered during development and how each was resolved.

---

## Module & Startup

**Asset Registry delegate never unbound after OnFilesLoaded**
`OnFilesLoaded` could fire more than once. Fixed by calling `RemoveAll(this)` at the top of `OnAssetRegistryFilesLoaded` so the callback is self-removing.

**AssetRegistry module loaded twice in StartupModule**
Two separate `LoadModuleChecked` calls created a potential stale reference. Fixed by hoisting a single `FAssetRegistryModule&` at the top of `StartupModule` and reusing it throughout.

**OnFilesLoaded not unbound during rapid shutdown**
If the editor closed before the initial asset scan finished, the delegate was never removed. Fixed by also calling `RemoveAll(this)` in `ShutdownModule`.

**Plugin tab and menu not appearing**
The plugin template comment stubs for `RegisterMenus` and `PluginButtonClicked` were never implemented. Fixed by implementing `FGlobalTabmanager::RegisterNomadTabSpawner` and `UToolMenus::ExtendMenu("LevelEditor.MainMenu.Tools")` with a proper menu entry pointing to **Tools → String Table Browser**.

**Plugin loaded but menu entry missing**
`RegisterMenus` was called before the editor toolbar was ready. Fixed by deferring registration via `UToolMenus::RegisterStartupCallback`.

---

## Caching

**ForceRebuildCache showing no entries**
After the thread-safety fix replaced `GetAsset()` with `FindObject()`, the initial cache build found nothing because no assets were loaded in memory yet at startup. Fixed by using `FindObject` first and falling back to `GetAsset()` (synchronous load) only inside `ForceRebuildCache`, keeping `FindObject`-only in the incremental callbacks.

**Stale disk cache loaded silently after updates**
The JSON cache had no version field, so outdated caches were loaded without question after schema changes. Fixed by adding a `GStringTableBrowserCacheVersion` constant and rejecting caches with a mismatched version, triggering a full rebuild.

**No thread safety on cache mutations**
Asset Registry callbacks can fire on background threads in some engine versions. Fixed by protecting all `GroupedCache` and `FlatCache` mutations with `FCriticalSection CacheLock`, and adding `GetCachedEntriesCopy()` for a lock-guarded snapshot safe to consume from Slate.

**BroadcastCacheUpdated called from background thread**
Slate delegates must be fired on the game thread. Fixed by checking `IsInGameThread()` and marshalling to the game thread via `AsyncTask(ENamedThreads::GameThread, ...)` when needed.

**Asset class comparison using short name**
`AssetClassPath.GetAssetName() == TEXT("StringTable")` could false-match other plugins. Fixed by comparing against `UStringTable::StaticClass()->GetClassPathName()` throughout.

---

## Search & Filtering

**Regex constructed per row from raw user input**
An invalid pattern (e.g. a bare `[`) caused undefined behaviour on every row. Fixed by pre-compiling the pattern once in `ApplyFilterAndSort`, wrapping construction in a try/catch, and skipping all rows if the pattern is invalid.

**FRegexMatcher::Sanitize used for whole-word escaping**
This internal API has no public contract. Fixed by replacing it with a custom `EscapeRegex` helper that manually escapes all ICU regex metacharacters.

**Match Case ignored in Whole Word and Regex modes**
Both regex paths used the raw pattern with no case flag. Fixed by prefixing patterns with `(?i)` when `bMatchCase` is false, and omitting the prefix when it is true.

**Search target always included all three fields**
Before scope toggles were added, the search target concatenated Key, Value, and TableId unconditionally. Fixed by building the target dynamically from only the fields the user has enabled, and returning `true` for all entries when no scope is selected.

**Main browser showed nothing on empty search**
After the picker was added with "empty = show nothing" behaviour, the main browser accidentally inherited it. Fixed by branching the two widgets: empty search shows everything in the main browser and nothing in the picker.

---

## Slate & UI

**SortData called directly from OnSortColumnHeader**
Calling `SortData` independently of `ApplyFilterAndSort` could sort a partially rebuilt list. Fixed by routing `OnSortColumnHeader` through `ApplyFilterAndSort` so filter and sort state are always consistent.

**Action column too narrow for two buttons**
Adding the Copy Key button caused it to overflow. Fixed by widening the Action column from `100` to `180` fixed width.

**Action column text buttons replaced with icons**
Text labels ("Edit Table", "Copy Key") were too wide for a compact column. Replaced with `SimpleButton`-style icon buttons using `Icons.Edit`, `Icons.Clipboard`, and `Icons.Find` brushes. A shared `MakeActionIconButton` static helper was introduced so all three buttons share identical sizing, padding, and style without repetition. Column width reduced to `110` to fit three icon buttons.

**Inline MakeIconButton lambda violated code standard**
The initial implementation used a local lambda inside `GenerateWidgetForColumn` to build icon buttons. Lambdas that capture nothing and are reused are better expressed as static file-scope helpers. Promoted to `static TSharedRef<SWidget> MakeActionIconButton(...)` alongside the existing `MakeFilterCheckBox`, matching the established pattern.

**Regex compiled once but pattern moved into TOptional incorrectly**
`MoveTemp` on the test pattern left it in a valid-but-unspecified state before assignment. Fixed by constructing the final `FRegexPattern` directly into `CompiledPattern` after validation.

---

## Search Performance

**Filter ran on every keystroke causing UI lag on large datasets**
With thousands of entries, running `ApplyFilterAndSort` synchronously on every key press caused perceptible lag. Fixed by debouncing via `RegisterActiveTimer` — the filter only runs after the last keystroke elapses. The delay was initially hardcoded to 150ms, then made configurable via `UStringTableBrowserSettings::SearchDebounceDelay` so teams can tune it per project.

---

## Reference Viewer

**IAssetManagerEditorModule availability not checked before use**
Calling `IAssetManagerEditorModule::Get()` when the module is not loaded would assert. Fixed by guarding with `IAssetManagerEditorModule::IsAvailable()` before calling `Get()`, matching Unreal's own convention for optional editor modules.

---

## Menu Registration

**Browser entry needed in String Table and Widget Blueprint editors**
The initial implementation only registered the Tools menu entry in the Level Editor. Fixed by extracting a `RegisterInToolsMenu(FName)` lambda inside `RegisterMenus()` and calling it for `"LevelEditor.MainMenu.Tools"`, `"AssetEditor.StringTableEditor.MainMenu.Tools"`, and `"AssetEditor.WidgetBlueprintEditor.MainMenu.Tools"`. The same `AddMenuEntry` definition is reused for all three, avoiding duplication.

---

## Empty State

**No feedback when search returns no results**
When `FilteredEntries` was empty the list view showed a blank area with no explanation. Fixed by wrapping the `SListView` in an `SOverlay` and adding a `STextBlock` that becomes visible via `Visibility_Lambda` when the list is empty. Two distinct messages are shown: "Type to search..." when the search box is empty (picker only), and "No entries match your search." when a search returned no results. `HitTestInvisible` is used rather than `Visible` so the text does not block interaction with the list.

---

## Details Panel Integration

**IPropertyTypeCustomization for "TextProperty" replaced native FText UI**
Registering a type customization for `"TextProperty"` completely replaced Unreal's own `FTextCustomization`, removing the localization flag and string table picker. Every attempt to render `CreatePropertyValueWidget` alongside the native widget caused conflicts. Fixed by abandoning `IPropertyTypeCustomization` entirely and switching to `IDetailCustomization` on `UObject`, which appends rows without replacing anything.

**Correct property type name was "TextProperty" not "Text"**
The initial registration used `"Text"` which was silently ignored. Found by logging `FTextProperty::StaticClass()->GetFName()` at runtime.

**Customization registered before engine's own customizations**
With `LoadingPhase: Default`, the plugin registered before `FTextCustomization`, which then overwrote it. Fixed by changing the `.uplugin` loading phase to `PostEngineInit`.

**Neither CustomizeHeader nor CustomizeChildren being called**
Even with the correct type name, the native FText customization was winning the registration race. Confirmed by the PostEngineInit fix above resolving it.

**TSharedRef member initialised from null TSharedPtr**
`TSharedRef<IPropertyHandle> PropertyHandleRef = TSharedPtr<IPropertyHandle>().ToSharedRef()` asserted immediately on construction because `ToSharedRef()` on a null pointer is illegal. Fixed by removing the unused member entirely — the handle is passed directly through method arguments.

**FindProperty does not exist on IDetailCategoryBuilder**
`Category.FindProperty(PropHandle)` was called to retrieve an existing row, but this API doesn't exist. Fixed by using `Category.AddProperty(PropHandle)` which both registers the property and returns the `IDetailPropertyRow&` directly.

**CustomizeChildren being implemented caused native children to disappear**
Implementing `CustomizeChildren` at all signals to Unreal that the plugin owns child rendering, suppressing the native ones. Fixed by leaving `CustomizeChildren` as an empty required override and doing all work via `AddProperty`.

**Apply button not setting the FText value**
`EnumerateRawData` writes to raw property memory bypass the transaction system entirely, even when surrounded by `NotifyPreChange`/`NotifyPostChange`. Fixed by replacing the raw write with `FScopedTransaction` + `Obj->Modify()` + `PropertyHandle->SetValue(NewValue)`. Undo now correctly appears in **Edit → Undo** as "Set String Table Reference".

**Wrong TableId passed to FText::FromStringTable**
`FStringTableBrowserEntry::TableId` stores the asset's short name, which doesn't always match the ID registered in `FStringTableRegistry`. Fixed by deriving the table ID from `Item->AssetPath.GetAssetPathString()` in `OnApplyClicked`.

**Dropdown spawn position covered the property row**
Fixed by adding a `FVector2D(0.f, 20.f)` offset to the spawn location.

**FSlateApplication::FindWidgetInAllWindows does not exist**
Fixed by passing an empty `FWidgetPath()` to `PushMenu` and using `GetActiveTopLevelWindow()` as the parent.

**Apply button text unreadable at column width**
Fixed by replacing text with a `Icons.Check` icon and keeping the description in the tooltip.

**Apply column had no header label**
Fixed by giving it the label `"Apply"`.

**OpenPickerDropdown parameter changed from TSharedRef to TSharedPtr**
`OpenPickerDropdown` was initially declared with `TSharedRef<IPropertyHandle>`, requiring a forced conversion at every call site. Changed to `TSharedPtr<IPropertyHandle>` to match how the handle is stored and passed internally, removing unnecessary `.ToSharedRef()` conversions.

---

## Shared Filter Refactor

**Duplicated filter logic between browser and picker**
`PassesFilter`, `EscapeRegex`, and the compiled regex state existed only in `SStringTableBrowser` and would have needed copying into `SStringTableBrowserPickerDropdown`. Fixed by extracting everything into a standalone `FStringTableSearchFilter` struct that both widgets own as a member, guaranteeing identical behaviour.

**MakeFilterCheckBox and CopyStringTableEntry duplicated across widgets**
`MakeFilterCheckBox` was a static file-scope function defined independently in both `SStringTableBrowser.cpp` and `SStringTableBrowserPickerDropdown.cpp`. `CopyStringTableEntry` existed inline in the main browser. Both were extracted into `FStringTableBrowserHelpers` — a dedicated static utility class — so both widgets call the same implementation and future changes propagate everywhere automatically.

---

## Code Organisation

**Data types and constants scattered across module and widget headers**
`FStringTableBrowserEntry` lived in `StringTableBrowserModule.h`, making every widget file depend on the full module header to access the entry struct. Column name constants were inline static variables duplicated in widget files. Icon brush names were bare string literals spread throughout. Fixed by introducing `StringTableBrowserTypes.h` as a lightweight header containing only the entry struct, the `StringTableBrowserColumns` namespace, and the `StringTableBrowserIcons` namespace. All files now include this one header for shared types, and changing an icon or column name requires a single edit.

**Module access pattern was verbose and unsafe across widgets**
Every widget file contained boilerplate `FModuleManager::Get().IsModuleLoaded("StringTableBrowser")` + `GetModuleChecked<>` pairs at every call site. If the module was unloaded between the two calls, `GetModuleChecked` would assert. Fixed by adding a `static FStringTableBrowserModule* GetModulePtr()` helper to the module class that wraps `FModuleManager::GetModulePtr<>`, returning null instead of asserting. All widget files now use `FStringTableBrowserModule::GetModulePtr()` with a null check.

**Style set implemented but never initialised**
`FStringTableBrowserStyle` was described in docs and referenced in `RegisterMenus` but the `.cpp` implementation was missing. The module would crash on startup when trying to look up the style set name. Fixed by implementing `StringTableBrowserStyle.cpp` with `Initialize`, `Shutdown`, `Get`, and `GetStyleSetName`, and calling `FStringTableBrowserStyle::Initialize()` at the top of `StartupModule`.

**No formal command registration**
The plugin action was wired directly to a lambda rather than through Unreal's `TCommands` system, which means it couldn't be rebound via keyboard shortcuts or discovered by the input system. Fixed by introducing `FStringTableBrowserCommands` as a proper `TCommands<>` subclass, registered in `StartupModule` and unregistered in `ShutdownModule`.

---

## Plugin Compatibility

**ExtensionContent() slot overridden by MVVM plugin**
`ExtensionContent()` on a Details panel row accepts only a single widget. When MVVM is enabled, its customization registers after ours and overwrites our button entirely. The correct API in UE 5.7, found by inspecting `PropertyEditorModule.h` directly, is `GetGlobalRowExtensionDelegate()` which returns an `FOnGenerateGlobalRowExtension` multicast delegate. Each plugin binds independently, appending to `OutExtensionButtons` — Unreal collects all registered buttons and renders them in a shared bar. The `IDetailCustomization` path was kept alongside to support the "Next to Label" placement option.

**Extension bar button should appear first, not last**
When multiple plugins add buttons to the extension bar, the order in `OutExtensionButtons` determines display order. Using `Add` placed the button last. Fixed by using `Insert(MoveTemp(Button), 0)` to place the String Table Browser button first in the bar.

---

## Plugin Settings

**Button placement needed to be configurable without code changes**
Different projects have different plugin combinations. A `UStringTableBrowserSettings` subclass of `UDeveloperSettings` was added, surfacing automatically under **Edit → Project Settings → Plugins → String Table Browser**. Both paths are always registered; each checks `ButtonPlacement` at runtime and returns early if inactive. Default changed from `ExtensionBar` to `NextToLabel` — always-visible is the safer default for new users who may not have conflicting plugins.

**Debounce delays were hardcoded**
Both the search debounce (150ms) and the disk write debounce (initially non-existent) were fixed values. Fixed by exposing `SearchDebounceDelay` and `SaveCacheToDiskDelay` as `UPROPERTY` fields in `UStringTableBrowserSettings`, read at runtime so teams can tune them without recompiling.

**Disk writes fired on every incremental cache update**
Every `OnAssetAdded`, `OnAssetUpdated`, and `OnAssetRemoved` call triggered an immediate `SaveCacheToDisk()`. On projects with many string tables, a version control sync could trigger dozens of writes in rapid succession. Fixed by introducing `ScheduleDiskCacheSave()` which sets a dirty flag and starts a `GEditor` timer — if the timer is already running it resets, so only one write happens after the burst settles. The delay is configurable via `SaveCacheToDiskDelay`.

---

## Cache Completeness

**In-editor string table key edits not reflected in cache**
`OnAssetUpdated` only fires when the Asset Registry re-scans an asset — it doesn't fire for individual key edits made while a string table is open in the editor. A user could add 10 new keys, save, and the cache would not update until the next full scan. Fixed by subscribing to `UPackage::PackageSavedWithContextEvent`, which fires immediately on every package save. The handler walks the saved package with `ForEachObjectWithPackage` to find any `UStringTable`, then updates only that table's cache entry — a targeted update rather than a full rebuild.

**Cache empty on first launch — assets not in memory**
`ForceRebuildCache` used `FindObject` to avoid synchronous loads, but `FindObject` only returns assets already resident in memory. On a fresh editor launch, most string table assets are not loaded yet, so the cache would start mostly empty. Fixed by adding a `bForceLoadStringTables` setting (on by default) that uses `FStreamableManager::RequestAsyncLoad` to load all string table assets in the background before building the cache. The editor remains responsive during the load. When all assets are resident, `OnForceLoadComplete` fires and `RebuildCacheFromLoadedAssets` runs — the same function used by the synchronous path — now finding everything via `FindObject`. The `ActiveStreamableHandle` is stored as a member so `ShutdownModule` can cancel the load if the module is destroyed while assets are still streaming. The toolbar button was renamed from **Force Rebuild Cache** to **Force Load String Table** to reflect this behaviour.

---

## Details Panel Integration — Struct Properties

**Search button missing on FText properties nested inside structs**

Three approaches were investigated before landing on the correct solution.

**Attempt 1 — Recursive `IDetailCustomization` walk**
The `CustomizeDetails` flat loop was replaced with a depth-first recursive `TFunction` that visited every child handle. This correctly identified nested `FTextProperty` handles, but calling `Category.AddProperty(Handle)` on a nested handle inserts it at the top level of the `IDetailCategoryBuilder` being held — Unreal always inserts into whichever category builder you reference, regardless of the handle's actual structural depth. The result was every nested FText property being promoted to a top-level row, completely breaking the Details panel hierarchy. The recursive approach was reverted and the path restored to top-level-only.

**Attempt 2 — `IPropertyTypeCustomization` override**
Registering against `"TextProperty"` fires for every FText property at any nesting depth, which is correct. However, taking ownership of `CustomizeHeader` replaces Unreal's own `FTextCustomization` — and the localization flag is rendered by `FTextCustomization`'s header construction, not by `CreatePropertyValueWidget()`. Every layout arrangement tried (button in `NameContent`, in `ValueContent`, in `ExtensionContent`) lost the localization flag because there is no public API to retrieve it independently. This approach was abandoned.

**Final solution — `GetGlobalRowExtensionDelegate()` (Extension Bar mode)**
The global row extension delegate fires for every rendered property row regardless of nesting depth, struct containment, or owning class. It appends to a shared extension bar without replacing any existing widget, so the native FText UI including the localization flag is fully preserved. This is the only public API that gives both complete struct-depth coverage and full native UI preservation. The trade-off — sharing the extension bar slot with other plugins like MVVM — is managed by inserting at index 0 to appear first and by providing the configurable placement setting so teams can choose the appropriate mode for their project.

**Why Next to Label remains top-level only**
The `IDetailCustomization` path cannot follow struct hierarchy without calling `Category.AddProperty` on nested handles, which always promotes them to the top level. This is a fundamental constraint of how `IDetailCategoryBuilder` works — it has no concept of inserting a row at a specific depth within the existing hierarchy. The Next to Label path is therefore intentionally kept to top-level properties, and the documentation reflects this limitation clearly.
