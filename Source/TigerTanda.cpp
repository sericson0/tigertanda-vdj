//==============================================================================
// TigerTanda VDJ Plugin - Core
// Plugin lifecycle, VDJ interaction, settings persistence
//==============================================================================

#include "TigerTanda.h"
#include "CoverArt.h"

#include <cstdio>
#include <filesystem>

#if defined(_WIN32)
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

TigerTandaPlugin::TigerTandaPlugin()
{
#if defined(_WIN32)
    INITCOMMONCONTROLSEX icc {};
    icc.dwSize = sizeof (icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx (&icc);

    fontNormal    = createFont (FONT_SIZE_NORMAL);
    fontBold      = createFont (FONT_SIZE_NORMAL, FW_BOLD);
    fontTitle     = createFont (FONT_SIZE_BRAND,  FW_BOLD);
    fontSmall     = createFont (FONT_SIZE_SMALL);
    fontSmallBold = createFont (FONT_SIZE_SMALL,  FW_BOLD);
    fontDetail    = createFont (FONT_SIZE_DETAIL);

    if (!fontNormal)    fontNormal    = (HFONT) GetStockObject (DEFAULT_GUI_FONT);
    if (!fontBold)      fontBold      = (HFONT) GetStockObject (DEFAULT_GUI_FONT);
    if (!fontTitle)     fontTitle     = (HFONT) GetStockObject (DEFAULT_GUI_FONT);
    if (!fontSmall)     fontSmall     = (HFONT) GetStockObject (DEFAULT_GUI_FONT);
    if (!fontSmallBold) fontSmallBold = (HFONT) GetStockObject (DEFAULT_GUI_FONT);
    if (!fontDetail)    fontDetail    = (HFONT) GetStockObject (DEFAULT_GUI_FONT);

    panelBrush     = CreateSolidBrush (TCol::panel);
    cardBrush      = CreateSolidBrush (TCol::card);
    searchBoxBrush = CreateSolidBrush (RGB (32, 36, 52));

    if (!panelBrush)     panelBrush     = (HBRUSH) GetStockObject (DKGRAY_BRUSH);
    if (!cardBrush)      cardBrush      = (HBRUSH) GetStockObject (DKGRAY_BRUSH);
    if (!searchBoxBrush) searchBoxBrush = (HBRUSH) GetStockObject (DKGRAY_BRUSH);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup (&gdiplusToken, &gdiplusInput, nullptr);
#endif
    // macOS: fonts and resources are created lazily in TigerTandaUI_Mac.mm
}

TigerTandaPlugin::~TigerTandaPlugin()
{
#if defined(_WIN32)
    if (hDlg && IsWindow (hDlg)) DestroyWindow (hDlg);
    if (fontNormal)    DeleteObject (fontNormal);
    if (fontBold)      DeleteObject (fontBold);
    if (fontTitle)     DeleteObject (fontTitle);
    if (fontSmall)     DeleteObject (fontSmall);
    if (fontSmallBold) DeleteObject (fontSmallBold);
    if (fontDetail)    DeleteObject (fontDetail);
    if (panelBrush)     DeleteObject (panelBrush);
    if (cardBrush)      DeleteObject (cardBrush);
    if (searchBoxBrush) DeleteObject (searchBoxBrush);
    delete reinterpret_cast<Gdiplus::Image*> (logoImage);
    logoImage = nullptr;
    CoverArt::shutdown();
    if (gdiplusToken) Gdiplus::GdiplusShutdown (gdiplusToken);
#elif defined(__APPLE__)
    destroyMacUI (this);
    CoverArt::shutdown();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin lifecycle
// ─────────────────────────────────────────────────────────────────────────────

HRESULT VDJ_API TigerTandaPlugin::OnLoad()
{
#if defined(_WIN32)
    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW (hInstance, dllPath, MAX_PATH);
    fs::path dllDir = fs::path (dllPath).parent_path();
    settingsPath = dllDir / L"tigertanda_settings.ini";
#elif defined(__APPLE__)
    // hInstance is a CFBundleRef on Mac
    CFURLRef bundleURL = CFBundleCopyBundleURL ((CFBundleRef) hInstance);
    if (bundleURL)
    {
        char bundlePath[1024] = {};
        CFURLGetFileSystemRepresentation (bundleURL, true, (UInt8*) bundlePath, sizeof (bundlePath));
        CFRelease (bundleURL);
        fs::path bundleDir = fs::path (bundlePath).parent_path();
        settingsPath = bundleDir / "tigertanda_settings.ini";
    }
#endif

    loadSettings();

#if defined(_WIN32)
    if (metadataFolder.empty())
    {
        wchar_t dllPath2[MAX_PATH] = {};
        GetModuleFileNameW (hInstance, dllPath2, MAX_PATH);
        metadataFolder = fs::path (dllPath2).parent_path().wstring();
    }
#elif defined(__APPLE__)
    if (metadataFolder.empty())
    {
        CFURLRef bundleURL2 = CFBundleCopyBundleURL ((CFBundleRef) hInstance);
        if (bundleURL2)
        {
            char bp[1024] = {};
            CFURLGetFileSystemRepresentation (bundleURL2, true, (UInt8*) bp, sizeof (bp));
            CFRelease (bundleURL2);
            metadataFolder = toWide (fs::path (bp).parent_path().string());
        }
    }
#endif

    detectMetadataFolder();

    // If metadata.csv still not found, fall back to the plugin's own directory
    {
        std::error_code ec;
        fs::path csvTest = fs::path (metadataFolder) / L"metadata.csv";
        if (!fs::is_regular_file (csvTest, ec))
        {
#if defined(_WIN32)
            wchar_t dp[MAX_PATH] = {};
            GetModuleFileNameW (hInstance, dp, MAX_PATH);
            metadataFolder = fs::path (dp).parent_path().wstring();
#elif defined(__APPLE__)
            CFURLRef bu = CFBundleCopyBundleURL ((CFBundleRef) hInstance);
            if (bu)
            {
                char p[1024] = {};
                CFURLGetFileSystemRepresentation (bu, true, (UInt8*) p, sizeof (p));
                CFRelease (bu);
                metadataFolder = toWide (fs::path (p).parent_path().string());
            }
#endif
        }
    }

    loadMetadata();

    DeclareParameterButton (&paramSearch, PID_SEARCH, "Search",       "Search");
    DeclareParameterButton (&paramFind,   PID_FIND,   "Re-find in VDJ", "ReFind");
    DeclareParameterButton (&paramReset,  PID_RESET,  "Reset",        "Reset");

    return S_OK;
}

HRESULT VDJ_API TigerTandaPlugin::OnGetPluginInfo (TVdjPluginInfo8* info)
{
    info->PluginName  = "TigerTanda";
    info->Author      = "TigerTanda Project";
    info->Description = "Argentine tango tanda builder - find similar songs";
    info->Version     = "1.0.0";
    info->Flags       = VDJFLAG_NODOCK;
    info->Bitmap      = nullptr;
    return S_OK;
}

ULONG VDJ_API TigerTandaPlugin::Release()
{
    if (metadataThread.joinable())
        metadataThread.join();

    delete this;

#if defined(_WIN32)
    HINSTANCE hInst = GetModuleHandleW (nullptr);
    UnregisterClassW (WND_CLASS, hInst);
#endif

    return S_OK;
}

HRESULT VDJ_API TigerTandaPlugin::OnGetUserInterface (TVdjPluginInterface8* pluginInterface)
{
#if defined(_WIN32)
    if (hDlg && IsWindow (hDlg))
    {
        dialogRequestedOpen = true;
        suppressNextHideSync = false;
        ShowWindow (hDlg, SW_SHOWNOACTIVATE);
        SetWindowPos (hDlg, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        pluginInterface->Type = VDJINTERFACE_DIALOG;
        pluginInterface->hWnd = hDlg;
        return S_OK;
    }

    ensureTandaWindowClass (hInstance);

    HWND parentHwnd = nullptr;
    double hwndVal = 0;
    if (GetInfo ("get hwnd", &hwndVal) == S_OK && hwndVal != 0)
        parentHwnd = (HWND) (intptr_t) hwndVal;

    const int initW = DLG_W;

    int posX, posY;
    if (parentHwnd)
    {
        RECT pr;
        GetWindowRect (parentHwnd, &pr);
        posX = pr.left + ((pr.right - pr.left) - initW) / 2;
        posY = pr.top  + ((pr.bottom - pr.top) - DLG_H) / 2;
    }
    else
    {
        posX = (GetSystemMetrics (SM_CXSCREEN) - initW) / 2;
        posY = (GetSystemMetrics (SM_CYSCREEN) - DLG_H) / 2;
    }

    hDlg = CreateWindowExW (WS_EX_TOOLWINDOW, WND_CLASS, L"TigerTanda",
                            WS_POPUP | WS_CLIPCHILDREN | WS_VISIBLE,
                            posX, posY, initW, DLG_H,
                            parentHwnd, nullptr, hInstance, this);

    if (hDlg)
    {
        pluginInterface->Type = VDJINTERFACE_DIALOG;
        pluginInterface->hWnd = hDlg;
        return S_OK;
    }
#elif defined(__APPLE__)
    // On Mac, VDJ_WINDOW is void*. We get the VDJ window from the API.
    double hwndVal = 0;
    void* vdjWindow = nullptr;
    if (GetInfo ("get hwnd", &hwndVal) == S_OK && hwndVal != 0)
        vdjWindow = (void*) (intptr_t) hwndVal;

    // Always create fresh — previous window was destroyed on close
    if (macUI)
        destroyMacUI (this);

    createMacUI (this, vdjWindow);

    if (macUI)
    {
        pluginInterface->Type = VDJINTERFACE_DIALOG;
        pluginInterface->hWnd = (VDJ_WINDOW) macUI;
        return S_OK;
    }
#endif

    return E_NOTIMPL;
}

HRESULT VDJ_API TigerTandaPlugin::OnParameter (int id)
{
    switch (id)
    {
        case PID_SEARCH:
        {
            if (!lastSeenTitle.empty())
                runIdentification (lastSeenTitle, lastSeenArtist);
            break;
        }
        case PID_FIND:
            if (selectedResultIdx >= 0
                && selectedResultIdx < (int) results.size()
                && !smartSearchPending)
            {
                lastSmartSearchTitle.clear();
                lastSmartSearchArtist.clear();
                triggerBrowserSearch (results[selectedResultIdx]);
            }
            break;
        case PID_RESET:
            resetAll();
            break;
    }
    return S_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
//  VDJ Interaction
// ─────────────────────────────────────────────────────────────────────────────

std::wstring TigerTandaPlugin::vdjGetString (const char* query)
{
    char buf[2048] = {};
    HRESULT hr = GetStringInfo (query, buf, sizeof (buf));
    if (SUCCEEDED (hr) && buf[0] != '\0')
        return toWide (buf);
    return {};
}

double TigerTandaPlugin::vdjGetValue (const char* query)
{
    double val = 0.0;
    HRESULT hr = GetInfo (query, &val);
    if (FAILED (hr))
        return 0.0;
    return val;
}

void TigerTandaPlugin::vdjSend (const std::string& cmd)
{
    SendCommand (cmd.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Metadata loading
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::detectMetadataFolder()
{
    // Check if metadataFolder already contains metadata.csv
    std::error_code ec;
    fs::path candidate = fs::path (metadataFolder) / L"metadata.csv";
    if (fs::is_regular_file (candidate, ec))
    {
        metadataFolder = fs::path (metadataFolder).wstring();
        return;
    }

#if defined(_WIN32)
    // Try %APPDATA%/VirtualDJ/Plugins64/
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW (nullptr, CSIDL_APPDATA, nullptr, 0, appData) == S_OK)
    {
        fs::path vdjDir = fs::path (appData) / L"VirtualDJ" / L"Plugins64";
        if (fs::is_regular_file (vdjDir / L"metadata.csv", ec))
        {
            metadataFolder = vdjDir.wstring();
            return;
        }
    }

    // Try Documents/VirtualDJ/Plugins64/
    wchar_t docs[MAX_PATH] = {};
    if (SHGetFolderPathW (nullptr, CSIDL_PERSONAL, nullptr, 0, docs) == S_OK)
    {
        fs::path vdjDir = fs::path (docs) / L"VirtualDJ" / L"Plugins64";
        if (fs::is_regular_file (vdjDir / L"metadata.csv", ec))
        {
            metadataFolder = vdjDir.wstring();
            return;
        }
    }
#elif defined(__APPLE__)
    // Try ~/Library/Application Support/VirtualDJ/Plugins64/
    const char* home = getenv ("HOME");
    if (!home) home = getpwuid (getuid())->pw_dir;
    if (home)
    {
        fs::path vdjDir = fs::path (home) / "Library" / "Application Support" / "VirtualDJ" / "Plugins64";
        if (fs::is_regular_file (vdjDir / "metadata.csv", ec))
        {
            metadataFolder = toWide (vdjDir.string());
            return;
        }
    }
#endif
}

void TigerTandaPlugin::loadMetadata()
{
    metadataLoadFailed = true;
    if (metadataFolder.empty()) return;
    std::error_code ec;
    fs::path csvPath = fs::path (metadataFolder) / L"metadata.csv";
    if (!fs::is_regular_file (csvPath, ec)) return;

    if (metadataThread.joinable())
        metadataThread.join();

    metadataLoading = true;
    std::wstring path = csvPath.wstring();
    metadataThread = std::thread ([this, path]()
    {
        matcher.loadCsv (path);
        metadataLoadFailed = (matcher.getRecordCount() == 0);
        metadataLoading = false;
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Settings
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::loadSettings()
{
    if (settingsPath.empty()) return;

    std::ifstream in (settingsPath);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline (in, line))
    {
        auto eq = line.find ('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr (0, eq);
        std::string val = line.substr (eq + 1);

        try
        {
            if (key == "metadataFolder")
                metadataFolder = toWide (val);
            else if (key == "sameArtist")
                filterSameArtist = (val != "0");
            else if (key == "sameSinger")
                filterSameSinger = (val != "0");
            else if (key == "sameGrouping")
                filterSameGrouping = (val != "0");
            else if (key == "sameGenre")
                filterSameGenre = (val != "0");
            else if (key == "sameOrchestra")
                filterSameOrchestra = (val != "0");
            else if (key == "sameLabel")
                filterSameLabel = (val != "0");
            else if (key == "sameTrack")
                filterSameTrack = (val != "0");
            else if (key == "useYearRange")
                filterUseYearRange = (val != "0");
            else if (key == "yearRange")
            {
                yearRange = std::stoi (val);
                if (yearRange < 0) yearRange = 0;
                if (yearRange > 20) yearRange = 20;
            }
            else if (key == "activeTab")
            {
                activeTab = std::stoi (val);
                if (activeTab < 0 || activeTab > 1) activeTab = 0;
            }
        }
        catch (...) {}
    }
}

void TigerTandaPlugin::saveSettings()
{
    if (settingsPath.empty()) return;

    fs::path tmpPath = settingsPath;
    tmpPath += ".tmp";

    {
        std::ofstream out (tmpPath, std::ios::trunc);
        if (!out.is_open()) return;

        out << "metadataFolder=" << toUtf8 (metadataFolder) << "\n";
        out << "sameArtist=" << (filterSameArtist ? 1 : 0) << "\n";
        out << "sameSinger=" << (filterSameSinger ? 1 : 0) << "\n";
        out << "sameGrouping=" << (filterSameGrouping ? 1 : 0) << "\n";
        out << "sameGenre=" << (filterSameGenre ? 1 : 0) << "\n";
        out << "sameOrchestra=" << (filterSameOrchestra ? 1 : 0) << "\n";
        out << "sameLabel=" << (filterSameLabel ? 1 : 0) << "\n";
        out << "sameTrack=" << (filterSameTrack ? 1 : 0) << "\n";
        out << "useYearRange=" << (filterUseYearRange ? 1 : 0) << "\n";
        out << "yearRange=" << yearRange << "\n";
        out << "activeTab=" << activeTab << "\n";
        out.flush();
        if (!out.good())
        {
            out.close();
            std::error_code ec;
            fs::remove (tmpPath, ec);
            return;
        }
    }

    std::error_code ec;
    fs::rename (tmpPath, settingsPath, ec);
    if (ec)
        fs::remove (tmpPath, ec);
}

// ─────────────────────────────────────────────────────────────────────────────
//  DLL Entry Point
// ─────────────────────────────────────────────────────────────────────────────

#if defined(_WIN32)
STDAPI DllGetClassObject (REFCLSID rclsid, REFIID riid, LPVOID* ppObject)
{
    if (memcmp (&rclsid, &CLSID_VdjPlugin8, sizeof (GUID)) == 0
        && memcmp (&riid, &IID_IVdjPluginBasic8, sizeof (GUID)) == 0)
    {
        *ppObject = new TigerTandaPlugin();
        return S_OK;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}
#elif defined(__APPLE__)
extern "C" VDJ_EXPORT HRESULT VDJ_API DllGetClassObject (const GUID& rclsid, const GUID& riid, void** ppObject)
{
    if (memcmp (&rclsid, &CLSID_VdjPlugin8, sizeof (GUID)) == 0
        && memcmp (&riid, &IID_IVdjPluginBasic8, sizeof (GUID)) == 0)
    {
        *ppObject = new TigerTandaPlugin();
        return S_OK;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}
#endif
