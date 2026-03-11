# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# Configure (first time only)
cmake -B build -A x64

# Build DLL
cmake --build build --config Release
# Output: build/Release/TigerTanda.dll + build/Release/metadata.csv
```

No tests. No linter. Rebuild after every change and install the DLL into VDJ to test.

## Architecture

Windows-only VDJ native plugin (MODULE DLL, no JUCE). Tango DJ tool for building tandas (sets of 3–4 songs) by searching a curated metadata database.

### Source files

| File | Responsibility |
|------|---------------|
| `TigerTanda.h` | Plugin class, all control IDs (`CtrlId` enum), layout constants, `BrowseItem` struct |
| `TigerTanda.cpp` | Plugin lifecycle (`OnLoad`, `OnGetPluginInfo`, `Release`), VDJ API wrappers (`vdjGetString`, `vdjGetValue`, `vdjSend`), settings INI, metadata loading |
| `TigerTandaUI.cpp` | Entire window procedure (`TandaWndProc`): `WM_CREATE`, `WM_PAINT`, `WM_TIMER`, `WM_COMMAND`, `WM_DRAWITEM`, `WM_MEASUREITEM`. Also `applyLayout` (show/hide/move all controls per active tab) |
| `TigerTandaMatching.cpp` | `runIdentification`, `confirmCandidate`, `runTandaSearch`, `runSmartSearch`, `resetAll` |
| `TigerTandaHelpers.h/.cpp` | `TCol` color namespace, GDI helpers (`fillRect`, `drawText`, `createFont`), `toWide`/`toUtf8`, `isVdjHostForeground`, `rebuildPrelistenWaveBins`, `normalizeForSearch` |
| `TangoMatcher.h/.cpp` | CSV loader, fuzzy matching (`tokenSortRatio`, Levenshtein), `findCandidates`, `findCandidatesForArtist` |
| `vdjPlugin8.h` | VDJ SDK header (do not modify) |

### Three-phase workflow

1. **Identification** — 250ms poll detects browsed song via `get_browsed_song 'title'/'artist'` → `runIdentification()` fuzzy-matches against `metadata.csv` → populates candidates list (Track tab, up to 5)
2. **Tanda search** — user confirms a candidate → `runTandaSearch()` filters all records by active filters + year range → populates results list (Matches tab, up to 20, sorted by year)
3. **Browse** — user selects a result and clicks "Search in VDJ" → VDJ's browser is searched, and `browseItems` is populated immediately from `p->results` → Browse tab shows those songs; clicking one sends a targeted VDJ `search` for that specific song

### Tab system

`activeTab` 0–3 (Track / Matches / Browse / Settings). `applyLayout()` is the single function that positions and shows/hides every control. Always call `applyLayout` + `repaintTabs` together when switching tabs.

### Browser polling

`TIMER_BROWSE_POLL` (250ms) handles: visibility sync, song polling → identification, prelisten waveform, and VDJ browser list population. The browse list poll is skipped while `smartSearchPending == true`. `smartSearchPending` is set by "Search in VDJ" and cleared when the user manually clicks the Browse tab.

### Key gotchas

- `get_browser_file N 'title'` reads VDJ's file-tree browser, **not** VDJ search results — it returns empty after a `search` command. This is why Browse tab population reads from `p->results` directly.
- MSVC: no surrogate-pair UCN literals; avoid `std::min`/`std::max` (Windows.h macros); use ternary instead.
- All owner-draw controls (`BS_OWNERDRAW` buttons, `LBS_OWNERDRAWFIXED` listboxes) are drawn in `WM_DRAWITEM`. `WM_MEASUREITEM` sets item heights.
- Settings persist to `tigertanda_settings.ini` next to the DLL via `loadSettings()`/`saveSettings()`.
- Metadata is a single `metadata.csv` file; path is stored in `metadataFolder` setting and auto-detected next to the DLL on first run.
