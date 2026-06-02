# String Table Browser

An Unreal Engine editor plugin that lets you browse, search, and bind every string table
entry in your project from a single panel.

![String Table Browser Panel](Docs/Screenshots/browser_panel.png)

## Features

**Unified view** — all string tables across your project displayed in one flat list, grouped
by source asset.

**Live search** — filter entries as you type, scoped to whichever fields you choose, with
debounced filtering for responsiveness on large datasets. The debounce delay is configurable
from Project Settings.

**Search scope** — toggle which fields are searched: Keys, Values, and/or String Table names.
Defaults to Values only.

**Search modes** — plain text, match case, whole word, and full regex support.

**Sortable columns** — click any column header to sort ascending or descending.

**Row actions** — three icon buttons per row: open the asset editor, copy a `LOCTABLE()`
reference, or open the native Reference Viewer.

**Details panel integration** — a search icon button appears on every `FText` property row in
the Details panel, opening a compact search dropdown to bind the property to a string table
entry directly.

**Configurable button placement** — choose whether the Details panel button appears next to
the property label (default, always visible, compatible with all plugins) or in the shared
extension bar on the right side of the row, via Project Settings.

**Incremental cache** — the plugin listens to the Asset Registry and updates automatically
when string tables are added, removed, or modified. Value-only edits (where keys are unchanged
and Asset Registry metadata does not change) are also detected via
`UPackage::PackageSavedWithContextEvent`, ensuring the cache stays current after every save.

**Debounced disk writes** — cache saves are batched so that bursts of Asset Registry events
(e.g. a large import) produce a single disk write rather than one per event. The delay is
configurable from Project Settings.

**Disk cache** — the entry list is persisted to disk so the panel is populated instantly on
editor startup without re-scanning all assets.

## Requirements

- Unreal Engine 5.x
- C++ project (the plugin must be compiled from source)

## Why This Plugin Exists

Unreal Engine ships with basic string table tooling that works well for small, well-organised
projects. At production scale, where localisation spans dozens of tables and thousands of
entries, the built-in tools show their limits. The plugin does not replace native
functionality — it extends it by addressing three specific gaps.

### 1. No Unified View

String tables are individual assets in the Content Browser. Each one has its own editor with
its own entry list, and Unreal provides no way to see entries from multiple tables at the same
time. Finding a string when you are not certain which asset it lives in means opening tables
one by one and scanning each in isolation.

The native String Table editor does include a search bar, but it is scoped to the currently
open asset and matches only against key names. There is no way to search by value — the actual
human-readable string — and no way to search across more than one table at a time.

**What the plugin provides:** a single docked panel that aggregates every entry from every
loaded string table in the project into one flat, sortable list. Search runs across all tables
simultaneously and can target keys, values, or table names in any combination.

### 2. No Cross-Table Value Search

The most common reason to browse a string table is to find a string you want to reuse or
reference — and in practice you are far more likely to remember what a string *says* than what
its key *is*. The native tooling has no path to this workflow. You can search for a key name
if you already know it, but there is no way to search for a value across the project.

This creates two real friction points:

**Avoiding duplication.** Before adding a new string table entry, a developer should check
whether an equivalent string already exists. Without value search this check is impractical,
so duplicate entries accumulate across tables.

**Finding the right reference.** When binding a string table entry to an FText property, you
need the exact table path and key. If you know the string's content but not its key, retrieving
that information requires opening each candidate table, scanning entries by eye, and
copy-pasting the key — a multi-step interruption for a one-second lookup.

**What the plugin provides:** live search against entry values, keys, and table names
simultaneously, with plain text, whole-word, match-case, and full regex modes.

### 3. Tedious FText Binding Workflow

Unreal does provide a way to bind an `FText` property to a string table entry from the Details
panel — the **Localization Flag** button (🏳️) that appears on every `FText` row opens a picker
window. In practice this picker is limited in ways that make it slow to use unless you already
know exactly where your string is:

- You must first select the string table asset manually from a dropdown, then select the key
  within it. There is no way to search across all tables in one step.
- The picker searches only key names within the selected table. If you do not know which table
  contains the string you want, you must select each one in turn and search inside it.
- There is no value preview. You see only keys, so there is no way to confirm you have the
  right entry without opening the asset separately to check.

The result is that the native binding flow is efficient only when you already know the exact
table and key. Any uncertainty — the wrong table, a forgotten key name, a string that might
exist somewhere but you are not sure where — makes the process significantly more involved.

**What the plugin provides:** a 🔍 button injected directly into every `FText` property row
in the Details panel. Clicking it opens a compact search dropdown — without leaving the panel
— where you can search all tables simultaneously by key, value, or table name, preview both
the key and value side by side, and click Apply to bind the property as a proper,
undo-able, localization-pipeline-compatible reference. The dropdown pre-populates with the
property's current value so refining an existing binding requires no extra navigation.

## Installation

Copy the `StringTableBrowser` folder into your project's `Plugins/` directory:

```
YourProject/
└── Plugins/
    └── StringTableBrowser/
        ├── StringTableBrowser.uplugin
        ├── README.md
        ├── Docs/
        │   ├── IMPLEMENTATION.md
        │   └── CHALLENGES.md
        ├── Resources/
        └── Source/
            └── StringTableBrowser/
                ├── StringTableBrowser.Build.cs
                ├── Public/
                │   ├── StringTableBrowserModule.h
                │   ├── StringTableBrowserSettings.h
                │   ├── StringTableBrowserTypes.h
                │   ├── StringTableSearchFilter.h
                │   ├── StringTableBrowserHelpers.h
                │   ├── SStringTableBrowser.h
                │   ├── SStringTableBrowserPickerDropdown.h
                │   └── FTextStringTableBrowserDetailCustomization.h
                └── Private/
                    ├── StringTableBrowserModule.cpp
                    ├── StringTableBrowserHelpers.cpp
                    ├── SStringTableBrowser.cpp
                    ├── SStringTableBrowserPickerDropdown.cpp
                    └── FTextStringTableBrowserDetailCustomization.cpp
```

Right-click your `.uproject` file and select **Generate Visual Studio project files**
(Windows) or **Generate Xcode project** (Mac).

Build the **Editor** target from your IDE. The plugin compiles automatically as part of the
project.

Open the Unreal Editor. If the plugin does not appear automatically, go to
**Edit → Plugins**, search for **String Table Browser**, enable it, and restart the editor.

## Opening the Panel

The **String Table Browser** entry appears in the **Tools** menu of three editors:

**Level Editor** — the primary entry point, always available.

**String Table Editor** — when editing a string table asset directly.

**Widget Blueprint Editor** — when editing a Widget Blueprint.

All three open the same docked browser tab. If the tab is already open, focus is moved to it
rather than spawning a duplicate.

## Usage

### Searching

Type in the search box to filter all entries in real time. The filter runs after the last
keystroke (debounced) to keep the UI responsive on large datasets. The debounce delay defaults
to 150ms and can be adjusted in **Edit → Project Settings → Plugins → String Table Browser**.
Toggle changes (Match Case, Whole Word, Regex, scope) apply immediately since they fire at
most once per click.

![Live Search](Docs/Screenshots/search_demo.gif)

#### Search Scope

The toggles beneath the search box control which fields are matched against your search term.
By default only **Values** is enabled.

| Toggle | Field searched | Example |
|---|---|---|
| **Keys** | The unique string identifier used to reference the entry in code | `MAIN_MENU_TITLE` |
| **Values** | The human-readable string displayed in game | `Start Game` |
| **String Tables** | The name of the source string table asset | `UI_Strings` |

If no entries match the current search, a **No entries match your search** message is shown in
the list area. If the search box is empty, all entries are displayed.

#### Search Modes

| Option | Behaviour |
|---|---|
| **Match Case** | Search is case-sensitive. Works with plain text, Whole Word, and Regex. |
| **Whole Word** | Only matches complete words, not substrings. For example, searching `table` will not match `StringTable`. |
| **Regex** | Interprets the search input as a regular expression (ICU syntax). See [Regex examples](#regex-examples) below. |

Match Case can be combined with any mode. Whole Word and Regex are mutually exclusive — if
both are enabled, Regex takes precedence.

#### Regex Examples

| Pattern | What it matches |
|---|---|
| `^Hello` | Entries whose searched field starts with `Hello` |
| `String\|Table` | Entries containing either `String` or `Table` |
| `This is a \w+` | Entries matching the phrase followed by any single word |
| `\d{3,}` | Entries containing a number with 3 or more digits |

**Note:** The `^` anchor applies to the beginning of the entire search target string. If
multiple scope fields are enabled, they are concatenated with a space before matching.

### Row Actions

Each result row has three icon buttons in the Action column:

| Action | Description |
|---|---|
| ✏️ Edit | Opens the source string table asset in the Unreal string table editor |
| 📋 Copy | Copies a `LOCTABLE()` reference for this entry to the clipboard |
| 🔍 References | Opens Unreal's native Reference Viewer for the source string table asset |

![Row Actions](Docs/Screenshots/row_actions.gif)

The reference copied by the Copy action is paste-ready in C++ source files, Blueprint string
table reference pins, and any Unreal property that accepts a localised string reference.

### Forcing a Cache Rebuild

Click **Force Rebuild Cache** in the toolbar to discard the current cache and re-scan all
string table assets from scratch. Use this if you have synced changes from version control and
the panel is not reflecting them.

**Note:** The cache only populates entries for string table assets that are already loaded in
memory at startup. Assets are loaded on demand as you work. If an asset you expect to see is
missing, open it in the Content Browser and click Force Rebuild Cache.

## Details Panel Picker

Every `FText` property in the Details panel gets a small search icon button injected into its
property row. This lets you bind an `FText` property to a string table entry without leaving
the Details panel.

![Details Panel Picker](Docs/Screenshots/picker_demo.gif)

### Opening the Picker

Click the 🔍 **search icon** on any `FText` property row. A compact dropdown opens anchored
below your cursor, containing the same search bar and toggles as the main viewer.

### Searching

The picker starts empty — type to begin searching. The same search scope and mode toggles
from the main viewer are available: Match Case, Whole Word, Regex, and scope toggles for Keys,
Values, and String Tables.

**The search box is pre-populated** with the property's current value on first open, so you
can immediately refine an existing binding. After that, the last search term you typed is
remembered for the lifetime of the Details panel instance, regardless of what the property is
set to.

### Applying an Entry

Click the **✓ (Apply)** button on any result row to bind the property to that entry. This sets
the `FText` property to a proper string table reference equivalent to:

```cpp
LOCTABLE("TablePath", "Key")
```

The binding is fully compatible with Unreal's localization pipeline, supports undo/redo, and
correctly dirties the asset for saving.

### Opening the Full Browser

Click **Open String Table Browser** in the dropdown footer to open the main panel for more
advanced browsing. The dropdown closes automatically and the browser tab is focused or spawned.

### Picker Search Behaviour vs Main Viewer

| Behaviour | Main Browser | Details Panel Picker |
|---|---|---|
| Empty search | Shows all entries | Shows nothing (type to search) |
| No results message | "No entries match your search." | Same |
| Default scope | Values | Values |
| Remembers last search | Per session | Per Details panel instance |
| Row actions | Edit, Copy, References | Apply (bind FText), Copy |


## Plugin Settings

![Plugin Settings](Docs/Screenshots/plugin_settings.png)

Go to **Edit → Project Settings → Plugins → String Table Browser** to configure the plugin.

### FText Button Placement

Controls where the search button appears on `FText` property rows in the Details panel.

| Option | Description | When to use |
|---|---|---|
| **Next to Property Label** *(default)* | Button appears inline to the right of the property name label | Recommended for all projects — always visible, never conflicts with MVVM or other plugins |
| **Extension Bar** | Button appears in the shared right-side extension bar, alongside buttons from other plugins | Use only if you specifically want the right-side placement and are not using MVVM or other plugins that also add buttons to FText rows |

> **Known limitation (Extension Bar):** A UE layout bug prevents the extension bar column
> from resizing when exactly two extension buttons are present (e.g. this plugin's button +
> the Reset-to-Default button). In that specific case the second button is clipped. This
> resolves automatically when three or more extension buttons are present, as UE's overflow
> dropdown activates. **Next to Property Label** does not have this limitation and is
> recommended for most projects.

![Details_Issue](Docs/Screenshots/details_issue.gif)

The setting is stored in `Config/DefaultStringTableBrowser.ini` and is project-scoped, so it
commits to source control and applies to all team members.

### Search Debounce Delay

Controls how long the search filter waits after the last keystroke before running, in seconds.
Default is `0.15` (150ms). Increase this on very large projects to reduce CPU usage while
typing; decrease it for more immediate feedback on smaller datasets.

### Save Cache to Disk Delay

Controls how long the plugin waits after the last Asset Registry event before writing the
updated cache to disk, in seconds. Default is `1.5` (1500ms). Events that arrive in bursts
(e.g. importing multiple string tables at once) are batched into a single write. Force Rebuild
Cache always writes immediately, bypassing this delay.

## How the Cache Works

The plugin maintains two levels of cache:

**In-memory cache** — a `TMap` keyed by package name, holding all entries per table. This is
rebuilt incrementally as the Asset Registry reports changes.

**Disk cache** — written to `Saved/StringTableBrowserCache.json` after every rebuild. On
startup, this file is loaded first to avoid scanning all assets. If the file is missing,
outdated, or from an older plugin version, a full rebuild runs automatically.

The cache is kept current through two complementary mechanisms:

- **Asset Registry events** (`OnAssetAdded`, `OnAssetRemoved`, `OnAssetUpdated`) — handle
  structural changes such as creating, deleting, or adding/removing keys from a string table.
  `OnAssetUpdated` fires when the asset's registry metadata (tags) changes.

- **`UPackage::PackageSavedWithContextEvent`** — fires unconditionally whenever a package is
  saved to disk, catching **value-only edits** that `OnAssetUpdated` misses. When you edit a
  string value without adding or removing keys, the registry tags are unchanged and
  `OnAssetUpdated` does not fire. `OnPackageSaved` ensures the cache is updated regardless.

Disk writes are **debounced** — multiple events arriving in rapid succession (e.g. a large
import) are batched into a single write after a short delay. The delay is configurable via
`SaveCacheToDiskDelay` in Project Settings.

The cache is versioned. Bumping `GStringTableBrowserCacheVersion` in
`StringTableBrowserModule.h` will force all users to rebuild on their next editor launch,
which is useful after schema changes.

## Troubleshooting

**The panel is empty on first launch.**
Open a string table asset in the Content Browser to load it into memory, then click **Force
Rebuild Cache**. Alternatively, wait for the editor to finish its initial asset scan and
reopen the panel.

**Entries are missing after a version control sync.**
The disk cache may be stale. Delete `Saved/StringTableBrowserCache.json` and restart the
editor, or click **Force Rebuild Cache**.

**A string table was edited and saved but the panel did not update.**
The cache updates after the `SaveCacheToDiskDelay` interval (default 1.5s) following the save.
If the panel still does not reflect the changes after a moment, click **Force Rebuild Cache**.
If the issue persists, confirm the asset is loaded in memory by opening it in the Content
Browser.

**The plugin doesn't appear in the Tools menu.**
Confirm the plugin is enabled under **Edit → Plugins**. If it was just enabled, a full editor
restart is required.

**The search icon button doesn't appear on FText properties.**
The Details panel customization loads during `PostEngineInit`. If the button is missing,
confirm the plugin is enabled and the editor has fully finished loading before inspecting a
property. Hot-reloading the plugin mid-session may also require a full editor restart.

**The search icon button appears in the wrong place or is hidden.**
Check the **FText Button Placement** setting under **Edit → Project Settings → Plugins →
String Table Browser**. If set to **Extension Bar** and the button appears hidden, switch to
**Next to Property Label** (the default). See the [known limitation](#ftext-button-placement)
note above.

**Clicking Apply shows a missing string table entry error.**
The string table asset may not be loaded in memory. Open the asset in the Content Browser and
try again. If the entry still doesn't resolve, click **Force Rebuild Cache** to ensure the
cache reflects the current state of your assets.

**The plugin fails to compile.**
Check that `"LevelEditor"`, `"PropertyEditor"`, `"DetailCustomizations"`,
`"AssetManagerEditor"`, `"DeveloperSettings"`, and all other module dependencies listed in
`StringTableBrowser.Build.cs` are present. Some modules shift between engine minor versions —
check the Output Log for the specific missing symbol.

**The button placement setting has no effect.**
Close and reopen any Details panel that was already open when you changed the setting. The
customization is rebuilt the next time a Details panel refreshes its layout.

## License

This plugin is released under the **MIT License**.

In short, you are free to:

- Use this plugin in free or commercial Unreal Engine projects.
- Modify the source code to suit your needs.
- Distribute it or include it in your own tools.

The only requirement is that the original copyright notice and license text are included in
any substantial distributions of the source code.

For the full legal text, please see the [LICENSE](LICENSE) file.

Copyright (c) 2026 Mato Marion.
 
