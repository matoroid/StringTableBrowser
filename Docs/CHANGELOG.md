# Changelog

All notable changes to String Table Browser will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

Features planned for future releases. See the [roadmap](#roadmap) section below
for full descriptions and open questions.

### Planned — Added
- **CSV string table support** — detect and parse pure `.csv` string tables
  alongside `UStringTable` assets, making all localisation sources visible in
  the browser regardless of format.
- **Right-click context menu** — additional row actions exposed via a context
  menu for faster keyboard-driven workflows.
- **Result statistics bar** — entry count and table count shown at the bottom of
  both the main browser and the picker dropdown, updating live as the search
  filter changes.
- **Export results** — button to export the current filtered result set to a
  `.csv` or `.json` file for data gathering and auditing.
- **Localization dashboard shortcut** — button or menu entry to open Unreal's
  built-in Localization Dashboard directly from the browser.

---

## [1.0.0] — 2026-05-23

Initial public release.

### Added

#### Core Browser Panel
- Unified panel listing all `UStringTable` entries across the project in a flat,
  sortable list, grouped by source asset.
- Live search with 150ms debounce — configurable via Project Settings.
- Search scope toggles: **Keys**, **Values** (default on), **String Tables**.
- Search mode toggles: **Match Case**, **Whole Word**, **Regex** (ICU syntax).
- Sortable columns: Key, Value, Source String Table.
- Empty-state overlay: "No entries match your search." shown when the filtered
  list is empty.
- Opened via **Tools → String Table Browser** in the Level Editor, String Table
  Editor, and Widget Blueprint Editor.

#### Row Actions
- **Edit Table** (`Icons.Edit`) — opens the source string table in the asset
  editor.
- **Copy Key** (`Icons.Clipboard`) — copies a `LOCTABLE("path", "key")`
  reference to the clipboard, ready to paste in C++ or Blueprint.
- **View References** (`Icons.Find`) — opens Unreal's native Reference Viewer
  for the source string table asset.

#### Details Panel Integration
- Search icon button injected into every `FText` property row in the Details
  panel via `FPropertyEditorModule::GetGlobalRowExtensionDelegate()`.
- Two placement modes, configurable via Project Settings:
  - **Next to Property Label** *(default)* — always visible, inside name column.
    Covers top-level `FText` properties only. Struct-nested FText properties
    cannot be covered by this path without breaking the Details panel hierarchy.
  - **Extension Bar** — shared right-side bar; compatible with MVVM and other
    plugins that also inject row buttons. Covers ALL `FText` properties
    including those nested inside structs at any depth, since the global row
    extension delegate fires per-row regardless of nesting.
- Applying an entry binds the `FText` property to a proper string table
  reference via `FText::FromStringTable()`, with full undo/redo support
  (`FScopedTransaction` + `UObject::Modify`).

#### Picker Dropdown
- Compact 700×380px dropdown anchored 20px below the cursor.
- Same search bar, scope toggles, and mode toggles as the main browser.
- Empty search shows "Type to search all string table entries." message.
- **Apply** (`Icons.Check`) — binds the FText property and closes the dropdown.
- **Copy** (`Icons.Clipboard`) — copies the LOCTABLE() reference without closing.
- **Open String Table Browser** footer button — invokes the main browser tab.
- Last search term persisted per Details panel instance; seeded from the current
  property value on first open.

#### Cache
- Two-layer cache: `GroupedCache` (per-asset `TMap`) + `FlatCache` (flat
  `TArray` for list view consumption).
- Disk cache at `Saved/StringTableBrowserCache.json` with schema version field —
  stale caches are discarded and rebuilt automatically.
- Incremental updates via Asset Registry events: `OnAssetAdded`,
  `OnAssetRemoved`, `OnAssetUpdated`.
- `UPackage::PackageSavedWithContextEvent` subscription — catches individual key
  edits made inside an open string table editor, which the Asset Registry does
  not report until re-scan.
- **Force load on cache build** — when `bForceLoadStringTables` is enabled,
  `FStreamableManager::RequestAsyncLoad` loads all string table assets in the
  background before the cache enumeration pass runs. The editor remains
  responsive during loading. The `ActiveStreamableHandle` is stored so
  `ShutdownModule` can cancel the load if the module is destroyed mid-stream.
  The toolbar button is labelled **Force Load String Table** to reflect this.
- Debounced disk writes via `ScheduleDiskCacheSave()` — configurable delay
  (default 1.5s) prevents burst writes during version control syncs.
- Thread-safe: all mutations guarded by `FCriticalSection`; broadcasts always
  marshalled to the game thread.

#### Project Settings (`Edit → Project Settings → Plugins → String Table Browser`)
- `ButtonPlacement` — FText row button placement (Next to Label / Extension Bar).
  Next to Label covers top-level properties only; Extension Bar covers all depths.
- `bForceLoadStringTables` — load all string table assets via async streamable
  manager before building the cache (default true). Ensures complete cache
  coverage on projects where assets are not kept loaded between sessions.
- `SearchDebounceDelay` — search filter debounce in seconds (default 0.15).
- `SaveCacheToDiskDelay` — disk write debounce in seconds (default 1.5).

#### Style & Commands
- `FStringTableBrowserStyle` — dedicated Slate style set, loads icons from
  `Resources/`.
- `FStringTableBrowserCommands` — formal `TCommands<>` registration for the
  plugin action, supporting keyboard shortcut rebinding.

#### Code Architecture
- `StringTableBrowserTypes.h` — shared entry struct and column/icon name
  constants in dedicated namespaces.
- `FStringTableBrowserHelpers` — static utility class: `MakeFilterCheckBox` and
  `CopyStringTableEntry`, shared across both widgets.
- `FStringTableSearchFilter` — self-contained search state struct with
  `Compile()` and `PassesFilter()`, shared between the main browser and the
  picker dropdown to guarantee identical behaviour.
- `FStringTableBrowserModule::GetModulePtr()` — null-safe static module accessor,
  replacing verbose `IsModuleLoaded` + `GetModuleChecked` boilerplate at every
  call site.

---

## Roadmap

### CSV string table support

**Goal:** Surface pure `.csv` string table files in the browser alongside
`UStringTable` assets.

**Approach:** Unreal's `UStringTable` can be populated from a `.csv` source file
via the importer, so assets imported from CSV are already covered. True
standalone `.csv` files (not imported, just referenced) would require a separate
scan using `FAssetRegistry` file-system scanning or a custom importer hook.
`FFileHelper::LoadFileToString` + a lightweight CSV parser (splitting on commas
with quote handling) could produce `FStringTableBrowserEntry` records.

**Open question:** Whether standalone `.csv` files are actually used in Unreal
projects in practice, or whether the importer path covers all real-world cases.

---

### Right-click context menu

**Goal:** Expose additional actions via a right-click menu on browser rows for
faster keyboard-driven workflows.

**Approach:** `SListView` supports `OnContextMenuOpening` which returns a
`TSharedPtr<SWidget>` — return an `SMenuBuilder`-constructed menu with entries
for Copy Key, Edit Table, View References, and any future actions. This avoids
crowding the visible column with more icon buttons.

---

### Result statistics bar

**Goal:** Show entry count and distinct table count in the current filtered
result set, updating live as the filter changes.

**Approach:** Add a `STextBlock` below the list view (or in the footer row for
the picker) bound to a `TAttribute<FText>` that formats
`FilteredEntries.Num()` entries across `N` tables. The table count is derived by
collecting unique `TableId` values from `FilteredEntries` into a `TSet`.

---

### Export results

**Goal:** Allow users to export the current filtered result set to a file for
data gathering, auditing, or sharing.

**Approach:** Add an **Export** button to the toolbar that writes
`FilteredEntries` to `FFileHelper::SaveStringToFile` as CSV
(`TableId,Key,Value`) or JSON using the existing `FJsonSerializer` pattern from
the disk cache. A save dialog via `IDesktopPlatform::SaveFileDialog` lets the
user choose the output path.

---

### Localization dashboard shortcut

**Goal:** Open Unreal's built-in Localization Dashboard directly from the
browser, reducing context-switching for developers working on localisation.

**Approach:** `FLocalizationDashboardSettings` and
`ILocalizationDashboardModule` provide access to the dashboard. Call
`ILocalizationDashboardModule::Get().ShowDashboard()` from a toolbar button or
the right-click context menu. The module lives in `LocalizationDashboard` which
would be added as an optional dependency in `Build.cs`.

[Unreleased]: https://github.com/MatoMarion26/StringTableBrowser/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/MatoMarion26/StringTableBrowser/releases/tag/v1.0.0
