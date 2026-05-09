#pragma once

#ifdef VDJ_WIN
#include <shlobj.h>
#endif

#include "TigerTandaHelpers.h"

#define NODLLEXPORT
#include "vdjPlugin8.h"
#include "vdjDsp8.h"

// vdjPlugin8.h defines S_OK/E_FAIL on Mac but not these standard COM macros
#ifdef VDJ_MAC
#ifndef SUCCEEDED
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#endif
#ifndef FAILED
#define FAILED(hr) ((HRESULT)(hr) < 0)
#endif
#endif
#ifdef VDJ_WIN
#include <windowsx.h>
#include <uxtheme.h>
#endif

#include <fstream>
#include <thread>
#include <atomic>

#ifdef VDJ_WIN
#define IDR_LOGO 101
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Control IDs (shared — used as logical IDs on both platforms)
// ─────────────────────────────────────────────────────────────────────────────

enum CtrlId
{
    // Track tab
    IDC_EDIT_TITLE         = 2101,
    IDC_EDIT_ARTIST        = 2102,
    IDC_EDIT_YEAR          = 2104,
    IDC_BTN_LOCK           = 2105,
    IDC_CANDIDATES_LIST    = 2201,

    // Settings tab (filter buttons)
    IDC_CHK_SAME_ARTIST    = 2301,
    IDC_CHK_SAME_SINGER    = 2302,
    IDC_CHK_SAME_GROUPING  = 2303,
    IDC_CHK_SAME_GENRE     = 2304,
    IDC_CHK_SAME_ORCHESTRA = 2305,
    IDC_CHK_SAME_LABEL     = 2306,
    IDC_CHK_SAME_TRACK     = 2307,
    IDC_BTN_YEAR_TOGGLE    = 2403,
    IDC_BTN_YEAR_RANGE     = 2405,
    IDC_BTN_YEAR_MINUS     = 2406,
    IDC_BTN_YEAR_PLUS      = 2407,

    // Matches tab
    IDC_RESULTS_LIST       = 2601,

    // Common
    IDC_BTN_CLOSE          = 2701,

    // Tab strip
    IDC_BTN_TAB_SETTINGS   = 2805,

    // Browse tab
    IDC_BROWSE_LIST        = 2901,
    IDC_BTN_PRELISTEN      = 2902,
    IDC_BTN_ADD_END        = 2903,

    // Settings: "How it works" sub-tabs
    IDC_BTN_HOW_TAB_0      = 2501,
    IDC_BTN_HOW_TAB_1      = 2502,
    IDC_BTN_HOW_TAB_2      = 2503,
    IDC_BTN_HOW_TAB_3      = 2504,
    IDC_BTN_HOW_TAB_4      = 2505,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Timer IDs
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr int TT_TIMER_BROWSE_POLL     = 1;
inline constexpr int TT_TIMER_SMART_SEARCH    = 2;
inline constexpr int TT_TIMER_WAVE_UPDATE     = 3;
inline constexpr int TT_TIMER_SEARCH_DEBOUNCE = 4;
inline constexpr int TT_TIMER_HOVER_POPUP     = 5;
inline constexpr int TT_TIMER_MATCH_SELECT    = 6;

#ifdef VDJ_WIN
// Windows timer IDs (backward compat — existing UI code uses these)
inline constexpr UINT_PTR TIMER_BROWSE_POLL     = TT_TIMER_BROWSE_POLL;
inline constexpr UINT_PTR TIMER_SMART_SEARCH    = TT_TIMER_SMART_SEARCH;
inline constexpr UINT_PTR TIMER_WAVE_UPDATE     = TT_TIMER_WAVE_UPDATE;
inline constexpr UINT_PTR TIMER_SEARCH_DEBOUNCE = TT_TIMER_SEARCH_DEBOUNCE;
inline constexpr UINT_PTR TIMER_HOVER_POPUP     = TT_TIMER_HOVER_POPUP;
inline constexpr UINT_PTR TIMER_MATCH_SELECT    = TT_TIMER_MATCH_SELECT;
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Font size constants (pt)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr int FONT_SIZE_NORMAL = 13;
inline constexpr int FONT_SIZE_SMALL  = 11;
inline constexpr int FONT_SIZE_DETAIL = 15;
inline constexpr int FONT_SIZE_BRAND  = 17;

// ─────────────────────────────────────────────────────────────────────────────
//  Layout constants
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr int DLG_H          = 370;
inline constexpr int TOP_GAP        = 4;
inline constexpr int DLG_W          = 700;
inline constexpr int TOP_H          = 30;
inline constexpr int PAD            = 8;
inline constexpr int BTN_H          = 24;
inline constexpr int EDIT_H         = 24;
inline constexpr int BRAND_H        = 26;
inline constexpr int CAND_ITEM_H    = 24;
inline constexpr int TAB_BTN_H      = 20;
inline constexpr int RESULT_ITEM_H  = 24;
inline constexpr int BROWSE_ITEM_H  = 46;
inline constexpr int DETAIL_BOX_H   = 100;
inline constexpr int PRE_WAVE_H     = 20;
inline constexpr int TRACK_SEARCH_GAP = 4;
inline constexpr int LEFT_COL_PCT   = 60;
inline constexpr int YEAR_COL_W     = 40;
inline constexpr int LOCK_BTN_W    = 16;
inline constexpr int DETAIL_PAD_X   = 6;
inline constexpr int DETAIL_PAD_Y   = 3;
inline constexpr int DETAIL_ROW_GAP = 1;
inline constexpr int BROWSE_HEADER_H = 18;
inline constexpr int PRELISTEN_TOP_GAP = 8;
inline constexpr int META_BANNER_H   = 22;

#ifdef VDJ_WIN
inline constexpr const wchar_t* WND_CLASS = L"TigerTandaVdjDialog";
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Browse history item (shared)
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
    int   browserIndex = -1;
    float score        = 0.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Rect type for layout (cross-platform)
// ─────────────────────────────────────────────────────────────────────────────

#ifdef VDJ_MAC
struct TTRect { int left, top, right, bottom; };
#else
typedef RECT TTRect;
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin class
// ─────────────────────────────────────────────────────────────────────────────

class TigerTandaPlugin : public IVdjPluginDsp8
{
public:
    TigerTandaPlugin();
    ~TigerTandaPlugin() override;

    HRESULT VDJ_API OnLoad() override;
    HRESULT VDJ_API OnGetPluginInfo (TVdjPluginInfo8* info) override;
    ULONG   VDJ_API Release() override;
    HRESULT VDJ_API OnGetUserInterface (TVdjPluginInterface8* pluginInterface) override;
    HRESULT VDJ_API OnParameter (int id) override;
    HRESULT VDJ_API OnProcessSamples (float* buffer, int nb) override;

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
    void triggerBrowserSearch (const TgRecord& rec);
    void addSelectedBrowseToAutomix (const char* vdjCommand = "playlist_add");
    void syncBrowseListVisibility();

    // UI wrapper methods (platform-specific implementations in UI files)
    void uiResetCandidatesList();
    void uiAddCandidateRow();
    void uiInvalidateCandidates();
    void uiResetResultsList();
    void uiAddResultRow();
    void uiResetBrowseList();
    void uiAddBrowseRow();
    void uiInvalidateDialog();
    void uiSetTimer (int timerId, int ms);
    void uiKillTimer (int timerId);
    void uiSetEditText (int ctrlId, const std::wstring& text);

    // Settings (TigerTanda.cpp)
    void loadSettings();
    void saveSettings();
    void detectMetadataFolder();
    void loadMetadata();

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
    bool filterUseYearRange  = true;

    // ── Tab ─────────────────────────────────────────────────────────────────
    int  activeTab    = 0;
    int  activeHowTab = 0;

    // ── Cached layout Ys ────────────────────────────────────────────────────
    int  columnHeaderY       = 0;
    int  matchHeaderY        = 0;
    int  browseResultsHeaderY= 0;
    TTRect metaBannerRect    = {};

    int  settingsMainHeaderY    = 0;
    int  settingsArtistsHeaderY = 0;
    int  settingsYearHeaderY    = 0;
    int  settingsOtherHeaderY   = 0;
    int  settingsLogoY          = 0;
    int  settingsLogoH          = 0;

    // ── Browser/deck polling ─────────────────────────────────────────────────
    std::wstring lastSeenTitle;
    std::wstring lastSeenArtist;
    std::wstring lastSeenBrowsePath;

    // ── Browse history ───────────────────────────────────────────────────────
    std::vector<BrowseItem> browseItems;
    int  browseListCount = -1;
    std::wstring lastBrowseFolder;

    // ── Smart search ─────────────────────────────────────────────────────────
    std::wstring searchTargetTitle;
    std::wstring searchTargetArtist;
    std::wstring searchTargetYear;
    bool         smartSearchPending = false;
    std::wstring lastSmartSearchTitle;
    std::wstring lastSmartSearchArtist;
    int smartSearchToken = 0;
    int smartSearchActiveToken = 0;
    int smartSearchRetryCount = 0;
    bool smartSearchNoResults = false;
    std::wstring savedBrowseFolder;
    std::wstring lastBrowserSearchQuery;

    // ── Prelisten ────────────────────────────────────────────────────────────
    bool              prelistenActive    = false;
    bool              prelistenSeeking   = false;
    double            prelistenPos       = 0.0;
    TTRect            prelistenWaveRect  = {};
    std::vector<int>  prelistenWaveBins;
    std::wstring      prelistenWavePath;

    // ── Settings ─────────────────────────────────────────────────────────────
    std::wstring metadataFolder;
    fs::path     settingsPath;
    bool         metadataLoadFailed = false;
    std::atomic<bool> metadataLoading { false };
    std::thread       metadataThread;

    bool dialogRequestedOpen  = true;
    bool suppressNextHideSync = false;
    bool suppressEditChange   = false;
    bool resultsLastInputWasMouse = false;
    bool searchLocked         = false;  // lock icon: freeze search fields on browser nav

    // ── Platform-specific UI state ───────────────────────────────────────────

#ifdef VDJ_WIN
    // Window handles
    HWND hDlg              = nullptr;
    HWND hEditTitle        = nullptr;
    HWND hEditArtist       = nullptr;
    HWND hEditYear         = nullptr;
    HWND hBtnLock          = nullptr;
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
    HWND hBtnYearToggle    = nullptr;
    HWND hBtnYearRange     = nullptr;
    HWND hBtnYearMinus     = nullptr;
    HWND hBtnYearPlus      = nullptr;
    HWND hBtnHowTabs[5]    = {};
    HWND hTooltip          = nullptr;
    HWND hHoverPopup       = nullptr;
    int  hoverPopupItem    = -1;
    int  hoverPendingItem  = -1;
    POINT hoverPendingPt   = {};
    HWND hoveredBtn        = nullptr;

    // GDI resources
    HFONT fontNormal    = nullptr;
    HFONT fontBold      = nullptr;
    HFONT fontSmall     = nullptr;
    HFONT fontSmallBold = nullptr;
    HFONT fontDetail    = nullptr;
    HFONT fontTitle     = nullptr;
    HFONT fontLockIcon  = nullptr;
    HBRUSH panelBrush     = nullptr;
    HBRUSH cardBrush      = nullptr;
    HBRUSH searchBoxBrush = nullptr;
    ULONG_PTR gdiplusToken = 0;
    void*     logoImage    = nullptr;
#endif

#ifdef VDJ_MAC
    void* macUI = nullptr;  // opaque TigerTandaMacUI* (avoids ObjC in header)
#endif
};

// ─────────────────────────────────────────────────────────────────────────────
//  Platform-specific declarations
// ─────────────────────────────────────────────────────────────────────────────

#ifdef VDJ_WIN
LRESULT CALLBACK TandaWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ensureTandaWindowClass (HINSTANCE hInst);
#endif

#ifdef VDJ_MAC
void createMacUI (TigerTandaPlugin* p, void* vdjWindow);
void destroyMacUI (TigerTandaPlugin* p);
#endif

// DLL entry point (cross-platform via VDJ SDK macros)
#ifdef VDJ_WIN
STDAPI DllGetClassObject (REFCLSID rclsid, REFIID riid, LPVOID* ppObject);
#endif
