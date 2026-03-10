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
    IDC_BTN_SRC_TOGGLE     = 2001,
    IDC_EDIT_TITLE         = 2101,
    IDC_EDIT_ARTIST        = 2102,
    IDC_BTN_SEARCH         = 2103,
    IDC_CANDIDATES_LIST    = 2201,
    IDC_CHK_SAME_ARTIST    = 2301,
    IDC_CHK_SAME_SINGER    = 2302,
    IDC_CHK_SAME_GROUPING  = 2303,
    IDC_CHK_SAME_GENRE     = 2304,
    IDC_CHK_SAME_ORCHESTRA = 2305,
    IDC_EDIT_YEAR_RANGE    = 2401,
    IDC_SPIN_YEAR_RANGE    = 2402,
    IDC_BTN_FIND_SIMILAR   = 2501,
    IDC_RESULTS_LIST       = 2601,
    IDC_BTN_CLOSE          = 2701,
    IDC_BTN_RESET          = 2702,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Timer IDs
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr UINT_PTR TIMER_BROWSE_POLL = 1;

// ─────────────────────────────────────────────────────────────────────────────
//  Layout constants
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr int DLG_W         = 920;
inline constexpr int DLG_H         = 580;
inline constexpr int TOP_H         = 44;
inline constexpr int LEFT_W        = 310;
inline constexpr int PAD           = 10;
inline constexpr int BTN_H         = 28;
inline constexpr int EDIT_H        = 22;
inline constexpr int CAND_ITEM_H   = 36;
inline constexpr int RESULT_ITEM_H = 28;
inline constexpr int REF_CARD_H    = 72;

// Right panel x-origin
inline constexpr int RIGHT_X = LEFT_W + PAD * 2;
inline constexpr int RIGHT_W = DLG_W - RIGHT_X - PAD;

// Window class name
inline constexpr const wchar_t* WND_CLASS = L"TigerTandaVdjDialog";

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

    // Settings (TigerTanda.cpp)
    void loadSettings();
    void saveSettings();
    void detectMetadataFolder();
    void loadMetadata();

    // UI helpers (TigerTandaUI.cpp)
    void repaintRefCard();
    void repaintTopBar();

    // Parameter IDs for VDJ parameter panel
    enum ParamId { PID_SEARCH = 0, PID_FIND, PID_RESET };
    int paramSearch = 0, paramFind = 0, paramReset = 0;

    // ── Phase 1: Identification ──────────────────────────────────────────────
    TangoMatcher             matcher;
    std::vector<TgMatchResult> candidates;   // up to 10
    int                      confirmedIdx = -1;

    // ── Phase 2: Tanda search ────────────────────────────────────────────────
    std::vector<TgRecord>    results;        // up to 20

    // ── Filters ─────────────────────────────────────────────────────────────
    bool filterSameArtist    = true;
    bool filterSameSinger    = false;
    bool filterSameGrouping  = false;
    bool filterSameGenre     = true;
    bool filterSameOrchestra = false;
    int  yearRange           = 5;   // 0 = disabled

    // ── Source mode ─────────────────────────────────────────────────────────
    int  sourceMode = 0;           // 0=Browser, 1=Deck(Active), 2=Deck(Other)

    // ── Browser/deck polling ─────────────────────────────────────────────────
    std::wstring lastSeenTitle;
    std::wstring lastSeenArtist;

    // ── Settings ─────────────────────────────────────────────────────────────
    std::wstring metadataFolder;  // empty = auto-detect
    fs::path     settingsPath;

    // ── Window handles ───────────────────────────────────────────────────────
    HWND hDlg           = nullptr;
    HWND hEditTitle     = nullptr;
    HWND hEditArtist    = nullptr;
    HWND hBtnSearch     = nullptr;
    HWND hCandList      = nullptr;
    HWND hChkArtist     = nullptr;
    HWND hChkSinger     = nullptr;
    HWND hChkGrouping   = nullptr;
    HWND hChkGenre      = nullptr;
    HWND hChkOrchestra  = nullptr;
    HWND hEditYearRange = nullptr;
    HWND hSpinYear      = nullptr;
    HWND hResultsList   = nullptr;
    HWND hBtnSrcToggle  = nullptr;
    HWND hBtnClose      = nullptr;
    HWND hBtnReset      = nullptr;

    // ── GDI resources ────────────────────────────────────────────────────────
    HFONT fontNormal   = nullptr;
    HFONT fontBold     = nullptr;
    HFONT fontSmall    = nullptr;
    HFONT fontTitle    = nullptr;
    HBRUSH panelBrush  = nullptr;
    HBRUSH cardBrush   = nullptr;


    bool dialogRequestedOpen = true;
    bool suppressNextHideSync = false;
};

// Window procedure (TigerTandaUI.cpp)
LRESULT CALLBACK TandaWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ensureTandaWindowClass (HINSTANCE hInst);

// DLL entry point
STDAPI DllGetClassObject (REFCLSID rclsid, REFIID riid, LPVOID* ppObject);
