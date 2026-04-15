# TigerTanda VDJ Plugin — macOS Port Design

**Date:** 2026-04-15
**Status:** Approved
**Constraint:** Windows build must remain completely unchanged.

## Overview

Port the TigerTanda VDJ plugin from Windows-only to cross-platform (Windows + macOS) within the same repository. The macOS UI pixel-matches the Windows version using custom Core Graphics drawing. Shared matching/search logic is reused without duplication.

## File Structure

```
Source/
├── vdjPlugin8.h                # unchanged (already has VDJ_MAC conditionals)
├── TangoMatcher.h              # unchanged
├── TangoMatcher.cpp            # fix: proper UTF-8→wstring in #else path
├── TigerTanda.h                # add: #ifdef VDJ_MAC section for Mac-specific members
├── TigerTanda.cpp              # add: #ifdef VDJ_MAC for lifecycle, settings paths, entry point
├── TigerTandaMatching.cpp      # add: thin UI abstraction wrappers (#ifdef for SendMessageW etc.)
├── TigerTandaHelpers.h         # add: cross-platform TTColor type, #ifdef for GDI declarations
├── TigerTandaHelpers.cpp       # unchanged (Windows-only, compiled only on Win32)
├── TigerTandaHelpers_Mac.mm    # NEW: Core Graphics drawing, NSFont, macOS UTF helpers
├── TigerTandaUI.cpp            # unchanged (compiled only on Win32)
├── TigerTandaUI_Mac.mm         # NEW: Cocoa UI — custom NSView + NSWindow
├── CoverArt.h                  # unchanged (opaque void* API)
├── CoverArt.cpp                # unchanged (compiled only on Win32)
├── CoverArt_Mac.mm             # NEW: NSImage-based decode, same binary parsers
├── TigerTanda.def              # Windows-only
├── TigerTanda.rc               # Windows-only
└── Info.plist                   # NEW: macOS bundle metadata
```

New Mac-specific files: 3 `.mm` files + Info.plist.
Existing Windows files: unmodified except `#ifdef VDJ_MAC` additions in shared headers/cpp.

## Platform Abstraction

### Colors (TigerTandaHelpers.h)

TCol currently uses `COLORREF`. Cross-platform approach:

```cpp
#ifdef VDJ_MAC
struct TTColor { float r, g, b; };
#define TTRGB(R,G,B) TTColor{(R)/255.f, (G)/255.f, (B)/255.f}
#else
typedef COLORREF TTColor;
#define TTRGB(R,G,B) RGB(R,G,B)
#endif

namespace TCol {
    inline const TTColor bg     = TTRGB(18,  21,  31);
    inline const TTColor accent = TTRGB(217, 108, 48);
    // ... all existing colors, same values
}
```

Windows code continues to receive COLORREF values. Mac code receives TTColor structs.
The GDI helper declarations (`createFont`, `fillRect`, `drawText`) are guarded under `#ifdef VDJ_WIN`.

### Plugin Class (TigerTanda.h)

Shared members stay as-is (enums, layout constants, BrowseItem, filters, matching state, VDJ helper methods). Platform-specific UI state is guarded:

```cpp
#ifdef VDJ_WIN
    HWND hDlg = nullptr;
    // ... all 24 HWNDs, 6 HFONTs, 3 HBRUSHes, GDI+ token ...
    LRESULT CALLBACK TandaWndProc(...);
#endif

#ifdef VDJ_MAC
    void* macUI = nullptr;  // opaque TigerTandaMacUI* (avoids ObjC in .h)
#endif
```

### UI Call Abstraction (TigerTandaMatching.cpp)

~10 Win32 UI calls in matching logic become thin wrappers declared in TigerTanda.h:

```cpp
// Platform-implemented in TigerTandaUI.cpp / TigerTandaUI_Mac.mm
void uiInvalidateResults();
void uiInvalidateBrowse();
void uiInvalidateDialog();
void uiResetResultsList();
void uiResetBrowseList();
void uiSetTimer(int timerId, int ms);
void uiKillTimer(int timerId);
void uiSyncBrowseVisibility();
```

Windows: calls `InvalidateRect`, `SendMessageW(LB_RESETCONTENT)`, `SetTimer`.
Mac: calls `[view setNeedsDisplay:YES]`, reloads list data, schedules `NSTimer`.
Matching logic calls wrappers — zero `#ifdef` in the algorithm code itself.

### Plugin Lifecycle (TigerTanda.cpp)

| Function | Windows | macOS |
|---|---|---|
| Constructor | `InitCommonControlsEx`, `CreateFontW`, `CreateSolidBrush`, `GdiplusStartup` | Minimal — fonts created in UI |
| Destructor | `DestroyWindow`, `DeleteObject`, `GdiplusShutdown` | Release NSWindow, clear cover art cache |
| `OnLoad` | `GetModuleFileNameW(hInstance)` | `CFBundleCopyBundleURL(hInstance)` |
| `detectMetadataFolder` | `SHGetFolderPathW(CSIDL_APPDATA)` | `~/Library/Application Support/VirtualDJ/Plugins64/` |
| `saveSettings` | `DeleteFileW` + `MoveFileExW` | `std::filesystem::rename` + `std::filesystem::remove` |
| `OnGetUserInterface` | `CreateWindowExW` → HWND | Create `NSWindow` + `TigerTandaMacUI` view |
| `DllGetClassObject` | `STDAPI` with COM types | Same signature (VDJ SDK provides Mac typedefs) |

### Settings Path (macOS)

Settings INI stored next to the bundle: `~/Library/Application Support/VirtualDJ/Plugins64/tigertanda_settings.ini`. Same format, same keys, same load/save logic — only the path discovery differs.

## macOS UI Architecture (TigerTandaUI_Mac.mm)

### Window

`NSPanel` (floating, non-activating) at `NSFloatingWindowLevel`, parented to VDJ's window. Equivalent to `WS_EX_TOOLWINDOW | WS_POPUP`. Same dimensions: DLG_W x DLG_H (700 x 370).

### Main View: TigerTandaMacUI (NSView subclass)

Single custom `NSView` that owns the entire window contents. Responsible for:
- **Drawing** via `drawRect:` using Core Graphics (CGContext)
- **Event handling** via `mouseDown:`, `keyDown:`, `mouseMoved:`
- **Timer management** via NSTimer for browse polling, smart search, debounce

### Lists: Custom-drawn scroll views

Three lists (candidates, results, browse) are each an `NSScrollView` containing a custom `NSView` subclass that draws rows. No NSTableView — full custom drawing to pixel-match Windows.

Each list view:
- Calculates visible rows from scroll offset and item height
- Draws each visible row using the same layout math as the Win32 `WM_DRAWITEM` code
- Handles click-to-select, keyboard navigation (arrow keys, Enter, Tab, Escape)
- Tracks selection state via the shared plugin members (`confirmedIdx`, `selectedResultIdx`, `selectedBrowseIdx`)

### Text Inputs

3 NSTextFields (title, artist, year) styled dark:
- Background: RGB(32, 36, 52) — matches Win32 `searchBoxBrush`
- Text color: `TCol::textBright`
- Font: System font at FONT_SIZE_NORMAL pt
- Delegate methods replace `EN_CHANGE` notification for search debounce

### Drawing Translation Guide

| Win32 GDI | Core Graphics (macOS) |
|---|---|
| `FillRect(hdc, &rc, brush)` | `CGContextFillRect(ctx, rect)` |
| `CreateSolidBrush(color)` | `CGContextSetRGBFillColor(ctx, r, g, b, 1)` |
| `DrawTextW(hdc, text, -1, &rc, flags)` | `[NSString drawInRect:withAttributes:]` or Core Text |
| `CreateFontW(-h, ..., "Segoe UI")` | `[NSFont systemFontOfSize:h]` (San Francisco) |
| `CreatePen + MoveToEx + LineTo` | `CGContextStrokeLineSegments` |
| `SetTimer(hwnd, id, ms, nullptr)` | `[NSTimer scheduledTimerWithTimeInterval:]` |
| `InvalidateRect(hwnd, nullptr, FALSE)` | `[view setNeedsDisplay:YES]` |
| `SendMessageW(list, LB_SETCURSEL, ...)` | Direct property set + `setNeedsDisplay:YES` |

### Font Mapping

| Windows (Segoe UI) | macOS (System / San Francisco) |
|---|---|
| `fontNormal` — 13pt regular | `[NSFont systemFontOfSize:13]` |
| `fontBold` — 13pt bold | `[NSFont boldSystemFontOfSize:13]` |
| `fontSmall` — 11pt regular | `[NSFont systemFontOfSize:11]` |
| `fontSmallBold` — 11pt bold | `[NSFont boldSystemFontOfSize:11]` |
| `fontDetail` — 15pt regular | `[NSFont systemFontOfSize:15]` |
| `fontTitle` — 17pt bold | `[NSFont boldSystemFontOfSize:17]` |

### Hover Popup

`NSPanel` equivalent of the Win32 custom hover popup window. Shows on mouse dwell over browse rows (750ms). Draws with Core Graphics matching the Win32 popup's dark theme.

### Prelisten Waveform

Same fake-waveform drawing logic (`rebuildPrelistenWaveBins` is portable). Drawn as vertical bars in the waveform rect using `CGContextFillRect`.

## Cover Art (CoverArt_Mac.mm)

### Shared Binary Parsers

The ID3v2, FLAC PICTURE, and MP4 atom-walker code is pure C with `FILE*`, `fread`, `fseek`, `memcmp`. This is 100% portable. The only changes needed:
- Replace `_wfopen` (Windows-only) with `fopen` using UTF-8 `std::string` paths
- Replace `Gdiplus::Bitmap` with `NSImage`

### Image Decode

```objc
static NSImage* decodeBytes(const uint8_t* data, size_t len) {
    NSData* nsData = [NSData dataWithBytes:data length:len];
    return [[NSImage alloc] initWithData:nsData];
}
```

Replaces the `GlobalAlloc → CreateStreamOnHGlobal → Gdiplus::Bitmap` chain. Handles JPEG, PNG, BMP, GIF, TIFF.

### Sidecar Images

Same folder-scan logic. Replace `new Bitmap(path.wstring().c_str())` with `[[NSImage alloc] initWithContentsOfFile:path]`.

### Cache

Same `std::unordered_map` pattern, keyed by `std::wstring` path → `NSImage*`.

## Build System (CMakeLists.txt)

```cmake
cmake_minimum_required(VERSION 3.22)
project(TigerTanda VERSION 1.0.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)

# Shared sources (compiled on both platforms)
set(SHARED_SOURCES
    Source/TangoMatcher.cpp
    Source/TigerTanda.cpp
    Source/TigerTandaMatching.cpp
)

if(WIN32)
    # === Existing Windows build — unchanged ===
    add_library(TigerTanda MODULE
        ${SHARED_SOURCES}
        Source/TigerTandaUI.cpp
        Source/TigerTandaHelpers.cpp
        Source/CoverArt.cpp
        Source/TigerTanda.def
        Source/TigerTanda.rc
    )
    set_target_properties(TigerTanda PROPERTIES OUTPUT_NAME "TigerTanda" SUFFIX ".dll")
    target_compile_definitions(TigerTanda PRIVATE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS UNICODE _UNICODE)
    target_compile_options(TigerTanda PRIVATE /FIshlobj.h)
    target_link_libraries(TigerTanda PRIVATE gdi32 comctl32 gdiplus shell32 ole32 uxtheme)

elseif(APPLE)
    # === macOS build ===
    add_library(TigerTanda MODULE
        ${SHARED_SOURCES}
        Source/TigerTandaUI_Mac.mm
        Source/TigerTandaHelpers_Mac.mm
        Source/CoverArt_Mac.mm
    )
    set_target_properties(TigerTanda PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "bundle"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Source/Info.plist"
    )
    target_link_libraries(TigerTanda PRIVATE
        "-framework Cocoa"
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework CoreFoundation"
    )
endif()

# Common to both
target_include_directories(TigerTanda PRIVATE "${CMAKE_SOURCE_DIR}/Source")
target_compile_features(TigerTanda PRIVATE cxx_std_17)

# Copy metadata.csv next to plugin output
add_custom_command(TARGET TigerTanda POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/metadata.csv"
        "$<TARGET_FILE_DIR:TigerTanda>/metadata.csv"
    COMMENT "Copying metadata.csv to plugin output directory"
)
```

The Windows path produces exactly the same .dll as today. The Mac path produces a .bundle.

## GitHub Actions (CI/CD)

### .github/workflows/build-mac.yml

Triggers on tag push (`v*`). Runs on `macos-latest`:
1. `cmake -B build`
2. `cmake --build build --config Release`
3. Zips the `.bundle` + `metadata.csv`
4. Attaches to GitHub Release

Optional: matrix build for both platforms in a single workflow.

## Implementation Order

1. **Platform abstraction in shared files** — `#ifdef` in TigerTandaHelpers.h (TTColor), TigerTanda.h (Mac members), TigerTanda.cpp (lifecycle). Fix TangoMatcher.cpp UTF-8 #else path.
2. **UI call wrappers in TigerTandaMatching.cpp** — extract `SendMessageW`/`InvalidateRect` calls into wrapper methods, implement Win32 versions inline.
3. **CMakeLists.txt** — add `elseif(APPLE)` block.
4. **TigerTandaHelpers_Mac.mm** — Core Graphics drawing helpers, UTF conversion, font creation.
5. **CoverArt_Mac.mm** — NSImage decode, portable binary parsers copied with `fopen` replacing `_wfopen`.
6. **TigerTandaUI_Mac.mm** — The big one. Build incrementally:
   a. NSWindow + empty dark NSView (verify plugin loads in VDJ)
   b. Top bar (close button, settings toggle, brand text)
   c. Search row (3 NSTextFields)
   d. Candidates list (custom-drawn scroll view)
   e. Results list + detail box
   f. Browse list + prelisten waveform + ADD button
   g. Settings tab
   h. Hover popup
   i. Keyboard navigation (arrow keys, Tab, Enter, Escape)
   j. Timer-based polling (browse poll, smart search, debounce)
7. **Info.plist** — bundle metadata.
8. **GitHub Actions workflow** — Mac build + release.

## Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Breaking Windows build | Windows path in CMakeLists.txt unchanged; CI can verify both |
| Font metrics differ (Segoe UI vs San Francisco) | Layout uses same constants; minor pixel differences acceptable |
| VDJ Mac plugin loading quirks | Test early — step 6a verifies load before building full UI |
| `wchar_t` size difference (2 bytes Win vs 4 bytes Mac) | All fuzzy matching uses `std::wstring` which adapts; file paths use `std::filesystem::path` |
| Core Graphics coordinate system (flipped vs unflipped) | Override `isFlipped` to return `YES` on the main view — then (0,0) is top-left like Win32 |
