# macOS Port — Implementation Plan

**Spec:** [2026-04-15-macos-port-design.md](2026-04-15-macos-port-design.md)
**Constraint:** Windows build must produce identical output after every step.

## Steps

### 1. Platform-abstract shared headers
- `TigerTandaHelpers.h`: TTColor type, TTRGB macro, guard GDI declarations + Windows includes
- `TigerTanda.h`: guard HWND/HFONT/HBRUSH members, add `void* macUI`, guard timer types, add UI wrapper declarations
- Verify: Windows build still succeeds with identical output

### 2. Platform-abstract shared cpp files
- `TigerTanda.cpp`: #ifdef VDJ_WIN/VDJ_MAC for constructor, destructor, OnLoad, detectMetadataFolder, saveSettings, OnGetUserInterface, DllGetClassObject
- `TigerTandaMatching.cpp`: replace all SendMessageW/InvalidateRect/SetTimer/ShowWindow/SetWindowTextW with UI wrapper methods; replace `_wcsicmp` with portable; replace `Sleep` with portable
- Implement UI wrapper methods for Windows in TigerTandaUI.cpp
- Verify: Windows build still succeeds

### 3. Fix TangoMatcher.cpp #else UTF-8 path
- Replace broken byte-cast with proper std::mbstowcs or codecvt

### 4. Update CMakeLists.txt
- Restructure: shared sources + if(WIN32)/elseif(APPLE) platform blocks
- Verify: Windows build still succeeds with identical output

### 5. Create TigerTandaHelpers_Mac.mm
- Core Graphics drawing helpers: fillRect, drawText
- NSFont creation helper
- UTF-8 ↔ wstring conversion (macOS wchar_t is 4 bytes)
- isVdjHostForeground (return true), setVdjRootHwnd (no-op)
- normalizeForSearch, rebuildPrelistenWaveBins (copy from Windows, already portable)
- String utilities: trimWs, joinNonEmptyParts (copy, already portable)

### 6. Create CoverArt_Mac.mm
- Copy all binary parsers (ID3v2, FLAC, MP4, AIFF) — replace _wfopen with fopen
- Replace GDI+ Bitmap with NSImage for decode
- Replace GlobalAlloc/CreateStreamOnHGlobal with NSData
- Same cache pattern, NSImage* instead of Bitmap*

### 7. Create TigerTandaUI_Mac.mm
Build incrementally:
- a. TigerTandaMacUI NSView subclass + NSPanel window creation
- b. drawRect: top bar, brand text, background
- c. Three custom list views (TTListView) as NSView in NSScrollView
- d. Candidates list drawing (row layout from WM_DRAWITEM)
- e. Results list drawing + detail box
- f. Browse list drawing + album art
- g. Search row: 3 styled NSTextFields
- h. Settings tab drawing
- i. Timer management (NSTimer for browse poll, smart search, debounce)
- j. Mouse handling (click to select, hover popup)
- k. Keyboard handling (arrow keys, Tab, Enter, Escape)
- l. Prelisten waveform drawing
- m. UI wrapper method implementations for Mac
- n. Hover popup (NSPanel)

### 8. Create Info.plist

### 9. Create .github/workflows/build-mac.yml

### 10. Verify Windows build
- Full rebuild from clean, confirm DLL is identical
