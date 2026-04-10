#pragma once

#include <shlobj.h>
#include "TigerTandaHelpers.h"

#define NODLLEXPORT
#include "vdjPlugin8.h"

#include <windowsx.h>
#include <uxtheme.h>
#include <fstream>

#define IDR_LOGO 101

// ─────────────────────────────────────────────────────────────────────────────
//  Control IDs
// ─────────────────────────────────────────────────────────────────────────────

enum CtrlId
{
    // Track tab
    IDC_EDIT_TITLE         = 2101,
    IDC_EDIT_ARTIST        = 2102,
    IDC_EDIT_YEAR          = 2104,
    IDC_CANDIDATES_LIST    = 2201,

    // Settings tab (filter buttons)
    IDC_CHK_SAME_ARTIST    = 2301,
    IDC_CHK_SAME_SINGER    = 2302,
    IDC_CHK_SAME_GROUPING  = 2303,
    IDC_CHK_SAME_GENRE     = 2304,
    IDC_CHK_SAME_ORCHESTRA = 2305,
    IDC_CHK_SAME_LABEL     = 2306,
    IDC_CHK_SAME_TRACK     = 2307,
    IDC_BTN_YEAR_TOGGLE    = 2403,  // toggles whether year range applies
    IDC_BTN_YEAR_RANGE     = 2405,  // cycles through year range values

    // Matches tab
    IDC_RESULTS_LIST       = 2601,

    // Common
    IDC_BTN_CLOSE          = 2701,

    // Tab strip (always in top bar)
    IDC_BTN_TAB_SETTINGS   = 2805,

    // Browse tab
    IDC_BROWSE_LIST        = 2901,
    IDC_BTN_PRELISTEN      = 2902,
    IDC_BTN_ADD_END        = 2903,
    IDC_BTN_FIND_IN_VDJ    = 2904,  // "Find in VDJ" — triggers browser search for selected match

    // Settings: "How it works" sub-tabs
    IDC_BTN_HOW_TAB_0      = 2501,  // Overview
    IDC_BTN_HOW_TAB_1      = 2502,  // Track
    IDC_BTN_HOW_TAB_2      = 2503,  // Matches
    IDC_BTN_HOW_TAB_3      = 2504,  // Browser
    IDC_BTN_HOW_TAB_4      = 2505,  // Filters
};

// ─────────────────────────────────────────────────────────────────────────────
//  Timer IDs
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr UINT_PTR TIMER_BROWSE_POLL     = 1;
inline constexpr UINT_PTR TIMER_SMART_SEARCH   = 2;
inline constexpr UINT_PTR TIMER_WAVE_UPDATE    = 3;
inline constexpr UINT_PTR TIMER_SEARCH_DEBOUNCE = 4;
inline constexpr UINT_PTR TIMER_HOVER_POPUP     = 5;  // 1s dwell before showing hover popup

// ─────────────────────────────────────────────────────────────────────────────
//  Font size constants (pt — passed to createFont)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr int FONT_SIZE_NORMAL = 13;  // fontNormal / fontBold
inline constexpr int FONT_SIZE_SMALL  = 11;  // fontSmall (retained for minor labels)
inline constexpr int FONT_SIZE_DETAIL = 15;  // fontDetail — secondary rows (+4pt from small)
inline constexpr int FONT_SIZE_BRAND  = 17;  // fontTitle — Tiger Tanda brand text

// ─────────────────────────────────────────────────────────────────────────────
//  Layout constants (compact / tab mode only)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr int DLG_H          = 390;
inline constexpr int TOP_GAP        = 4;     // compact gap below top bar
inline constexpr int DLG_W          = 700;
inline constexpr int TOP_H          = 30;    // compact top bar
inline constexpr int PAD            = 8;
inline constexpr int BTN_H          = 24;
inline constexpr int EDIT_H         = 24;
inline constexpr int BRAND_H        = 26;    // Tiger Tanda brand text row at bottom
inline constexpr int CAND_ITEM_H    = 24;    // 13pt row
inline constexpr int TAB_BTN_H      = 20;    // top tab strip height
inline constexpr int RESULT_ITEM_H  = 24;    // 13pt row
inline constexpr int BROWSE_ITEM_H  = 46;    // tall rows: 2 text rows + album art thumbnail
inline constexpr int DETAIL_BOX_H   = 110;   // 5-row: Title + Bandleader·Singer + Date·Genre + Label + Group
inline constexpr int PRE_WAVE_H     = 20;    // prelisten waveform height
inline constexpr int TRACK_SEARCH_GAP = 4;   // reduced gap (was 14)
inline constexpr int LEFT_COL_PCT   = 60;    // left column percentage

// Window class name
inline constexpr const wchar_t* WND_CLASS = L"TigerTandaVdjDialog";

// ─────────────────────────────────────────────────────────────────────────────
//  Browse history item
// ─────────────────────────────────────────────────────────────────────────────

struct BrowseItem
{
    std::wstring title;
    std::wstring artist;
    std::wstring year;
    std::wstring filePath;
    std::wstring album;
    std::wstring comment;
    int   stars        = 0;
    int   playCount    = 0;
    int   browserIndex = -1;   // original index in VDJ browser list
    float score        = 0.0f; // smart search relevance score
};

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin class
// ─────────────────────────────────────────────────────────────────────────────

class TigerTandaPlugin : public IVdjPlugin8
{
public:
    TigerTandaPlugin();
    ~TigerTandaPlugin() override;

    HRESULT VDJ_API OnLoad() override;
    HRESULT VDJ_API OnGetPluginInfo (TVdjPluginInfo8* info) override;
    ULONG   VDJ_API Release() override;
    HRESULT VDJ_API OnGetUserInterface (TVdjPluginInterface8* pluginInterface) override;
    HRESULT VDJ_API OnParameter (int id) override;

    // VDJ helpers
    std::wstring vdjGetString (const char* query);
    double       vdjGetValue  (const char* query);
    void         vdjSend      (const std::string& cmd);

    // Matching (TigerTandaMatching.cpp)
    void runIdentification (const std::wstring& title, const std::wstring& artist);
    void confirmCandidate (int idx);
    void runTandaSearch();
    void resetAll();
    void runSmartSearch();
    // Browser search workflow — saves current folder, fires VDJ search,
    // schedules runSmartSearch timer. runSmartSearch reads results and
    // restores the saved folder.
    void triggerBrowserSearch (const TgRecord& rec);
    // ADD button — re-issues the last search, scrolls to the selected
    // browseItem's stored browserIndex, sends playlist_add, restores folder.
    void addSelectedBrowseToAutomix();
    // Toggle the browse listbox visibility based on browseItems.empty().
    // When empty, the main window paints a placeholder in its place.
    void syncBrowseListVisibility();

    // Settings (TigerTanda.cpp)
    void loadSettings();
    void saveSettings();
    void detectMetadataFolder();
    void loadMetadata();

    // UI helpers (TigerTandaUI.cpp)

    // Parameter IDs for VDJ parameter panel
    enum ParamId { PID_SEARCH = 0, PID_FIND, PID_RESET };
    int paramSearch = 0, paramFind = 0, paramReset = 0;

    // ── Phase 1: Identification ──────────────────────────────────────────────
    TangoMatcher             matcher;
    std::vector<TgMatchResult> candidates;
    int                      confirmedIdx = -1;

    // ── Phase 2: Tanda search ────────────────────────────────────────────────
    std::vector<TgRecord>    results;
    int                      selectedResultIdx = -1;
    int                      selectedBrowseIdx = -1;

    // ── Filters ─────────────────────────────────────────────────────────────
    bool filterSameArtist    = true;
    bool filterSameSinger    = true;
    bool filterSameGrouping  = false;
    bool filterSameGenre     = true;
    bool filterSameOrchestra = false;
    bool filterSameLabel     = false;
    bool filterSameTrack     = false;
    int  yearRange           = 5;
    bool filterUseYearRange  = true;   // whether year range filter applies

    // ── Tab ─────────────────────────────────────────────────────────────────
    int  activeTab    = 0;    // 0=Main, 1=Settings
    int  activeHowTab = 0;    // 0=Overview, 1=Track, 2=Matches, 3=Browser, 4=Filters

    // ── Browser/deck polling ─────────────────────────────────────────────────
    std::wstring lastSeenTitle;
    std::wstring lastSeenArtist;
    std::wstring lastSeenBrowsePath;

    // ── Browse history ───────────────────────────────────────────────────────
    std::vector<BrowseItem> browseItems;   // current VDJ browser list, cap ~200
    int  browseListCount = -1;             // cached file_count for change detection
    std::wstring lastBrowseFolder;         // cached get_browsed_folder_path for change detection

    // ── Smart search (ranked VDJ browser results) ────────────────────────────
    std::wstring searchTargetTitle;
    std::wstring searchTargetArtist;
    std::wstring searchTargetYear;
    bool         smartSearchPending = false;
    // Remember last search target so we don't re-fire on redundant requests
    std::wstring lastSmartSearchTitle;
    std::wstring lastSmartSearchArtist;
    // Monotonically increasing token — runSmartSearch checks that the token
    // it was fired for is still current before applying its results, so a
    // user clicking a different match mid-flight cancels stale results.
    int smartSearchToken = 0;
    int smartSearchActiveToken = 0;
    int smartSearchRetryCount = 0;  // how many times we've waited for VDJ
    bool smartSearchNoResults = false;  // true if the last search timed out / empty
    // Folder path saved at start of a search cycle; restored when done
    std::wstring savedBrowseFolder;
    // Raw VDJ search query string used to populate the current browseItems.
    // ADD needs this to re-issue the same search so saved browserIndex
    // values still line up with VDJ's result order.
    std::wstring lastBrowserSearchQuery;

    // ── Prelisten ────────────────────────────────────────────────────────────
    bool              prelistenActive    = false;
    bool              prelistenSeeking   = false;
    double            prelistenPos       = 0.0;
    RECT              prelistenWaveRect  = {};
    std::vector<int>  prelistenWaveBins;
    std::wstring      prelistenWavePath;

    // ── Settings ─────────────────────────────────────────────────────────────
    std::wstring metadataFolder;
    fs::path     settingsPath;

    // ── Window handles ───────────────────────────────────────────────────────
    HWND hDlg              = nullptr;
    HWND hEditTitle        = nullptr;
    HWND hEditArtist       = nullptr;
    HWND hEditYear         = nullptr;
    HWND hCandList         = nullptr;
    HWND hChkArtist        = nullptr;
    HWND hChkSinger        = nullptr;
    HWND hChkGrouping      = nullptr;
    HWND hChkGenre         = nullptr;
    HWND hChkOrchestra     = nullptr;
    HWND hChkLabel         = nullptr;
    HWND hChkTrack         = nullptr;
    HWND hResultsList      = nullptr;
    HWND hBtnClose         = nullptr;
    HWND hBtnTabSettings   = nullptr;
    HWND hBrowseList       = nullptr;
    HWND hBtnPrelisten     = nullptr;
    HWND hBtnAddEnd        = nullptr;
    HWND hBtnFindInVdj     = nullptr;
    HWND hBtnYearToggle    = nullptr;
    HWND hBtnYearRange     = nullptr;
    HWND hBtnHowTabs[5]    = {};
    HWND hTooltip          = nullptr;
    HWND hHoverPopup       = nullptr;   // themed custom popup window for browse-row metadata
    int  hoverPopupItem    = -1;        // browse row the popup is currently keyed on (-1 = hidden)
    int  hoverPendingItem  = -1;        // row queued by dwell timer but not yet shown
    POINT hoverPendingPt   = {};        // screen position captured when dwell started
    HWND hoveredBtn        = nullptr;   // currently hovered owner-draw button (for hover highlight)

    // ── GDI resources ────────────────────────────────────────────────────────
    HFONT fontNormal    = nullptr;  // FONT_SIZE_NORMAL pt regular
    HFONT fontBold      = nullptr;  // FONT_SIZE_NORMAL pt bold
    HFONT fontSmall     = nullptr;  // FONT_SIZE_SMALL pt regular
    HFONT fontSmallBold = nullptr;  // FONT_SIZE_SMALL pt bold (inline bold in Settings content)
    HFONT fontDetail    = nullptr;  // FONT_SIZE_DETAIL pt regular (secondary rows, +4pt)
    HFONT fontTitle     = nullptr;  // FONT_SIZE_BRAND pt bold (Tiger Tanda brand text)
    HBRUSH panelBrush     = nullptr;
    HBRUSH cardBrush      = nullptr;
    HBRUSH searchBoxBrush = nullptr;
    ULONG_PTR gdiplusToken = 0;
    void*     logoImage    = nullptr;  // Gdiplus::Image* cached logo (opaque to avoid header dep)

    bool dialogRequestedOpen  = true;
    bool suppressNextHideSync = false;
    bool suppressEditChange   = false;  // true while polling updates edit boxes
};

// Window procedure (TigerTandaUI.cpp)
LRESULT CALLBACK TandaWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ensureTandaWindowClass (HINSTANCE hInst);

// DLL entry point
STDAPI DllGetClassObject (REFCLSID rclsid, REFIID riid, LPVOID* ppObject);
