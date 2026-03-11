//==============================================================================
// TigerTanda VDJ Plugin - Core
// Plugin lifecycle, VDJ interaction, settings persistence
//==============================================================================

#include "TigerTanda.h"

#include <cstdio>
#include <filesystem>

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

TigerTandaPlugin::TigerTandaPlugin()
{
    INITCOMMONCONTROLSEX icc {};
    icc.dwSize = sizeof (icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx (&icc);

    fontNormal = createFont (FONT_SIZE_NORMAL);
    fontBold   = createFont (FONT_SIZE_NORMAL, FW_BOLD);
    fontTitle  = createFont (FONT_SIZE_BRAND,  FW_BOLD);
    fontSmall  = createFont (FONT_SIZE_SMALL);
    fontDetail = createFont (FONT_SIZE_DETAIL);
    panelBrush = CreateSolidBrush (TCol::panel);
    cardBrush  = CreateSolidBrush (TCol::card);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup (&gdiplusToken, &gdiplusInput, nullptr);
}

TigerTandaPlugin::~TigerTandaPlugin()
{
    if (hDlg && IsWindow (hDlg)) DestroyWindow (hDlg);
    if (fontNormal) DeleteObject (fontNormal);
    if (fontBold)   DeleteObject (fontBold);
    if (fontTitle)  DeleteObject (fontTitle);
    if (fontSmall)  DeleteObject (fontSmall);
    if (fontDetail) DeleteObject (fontDetail);
    if (panelBrush) DeleteObject (panelBrush);
    if (cardBrush)  DeleteObject (cardBrush);
    delete reinterpret_cast<Gdiplus::Image*> (logoImage);
    logoImage = nullptr;
    if (gdiplusToken) Gdiplus::GdiplusShutdown (gdiplusToken);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plugin lifecycle
// ─────────────────────────────────────────────────────────────────────────────

HRESULT VDJ_API TigerTandaPlugin::OnLoad()
{
    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW (hInstance, dllPath, MAX_PATH);
    fs::path dllDir = fs::path (dllPath).parent_path();
    settingsPath = dllDir / L"tigertanda_settings.ini";

    loadSettings();
    if (metadataFolder.empty())
        metadataFolder = dllDir.wstring();
    detectMetadataFolder();
    // If metadata.csv still not found (e.g. old settings pointed at a 'metadata/' subdir),
    // fall back to the DLL's own directory.
    {
        std::error_code ec;
        fs::path csvTest = fs::path (metadataFolder) / L"metadata.csv";
        if (!fs::is_regular_file (csvTest, ec))
            metadataFolder = dllDir.wstring();
    }
    loadMetadata();

    DeclareParameterButton (&paramSearch, PID_SEARCH, "Search",       "Search");
    DeclareParameterButton (&paramFind,   PID_FIND,   "Find Similar", "Find");
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
    delete this;
    return S_OK;
}

HRESULT VDJ_API TigerTandaPlugin::OnGetUserInterface (TVdjPluginInterface8* pluginInterface)
{
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

    return E_NOTIMPL;
}

HRESULT VDJ_API TigerTandaPlugin::OnParameter (int id)
{
    switch (id)
    {
        case PID_SEARCH:
        {
            // Use last seen title/artist from polling
            if (!lastSeenTitle.empty())
                runIdentification (lastSeenTitle, lastSeenArtist);
            break;
        }
        case PID_FIND:
            runTandaSearch();
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
    GetInfo (query, &val);
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
}

void TigerTandaPlugin::loadMetadata()
{
    if (metadataFolder.empty()) return;
    std::error_code ec;
    fs::path csvPath = fs::path (metadataFolder) / L"metadata.csv";
    if (fs::is_regular_file (csvPath, ec))
        matcher.loadCsv (csvPath.wstring());
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
                if (activeTab < 0 || activeTab > 3) activeTab = 0;
            }
        }
        catch (...) {}
    }
}

void TigerTandaPlugin::saveSettings()
{
    if (settingsPath.empty()) return;

    std::ofstream out (settingsPath, std::ios::trunc);
    if (!out.is_open()) return;

    out << "metadataFolder=" << toUtf8 (metadataFolder) << "\n";
    out << "sameArtist=" << (filterSameArtist ? 1 : 0) << "\n";
    out << "sameSinger=" << (filterSameSinger ? 1 : 0) << "\n";
    out << "sameGrouping=" << (filterSameGrouping ? 1 : 0) << "\n";
    out << "sameGenre=" << (filterSameGenre ? 1 : 0) << "\n";
    out << "sameOrchestra=" << (filterSameOrchestra ? 1 : 0) << "\n";
    out << "sameLabel=" << (filterSameLabel ? 1 : 0) << "\n";
    out << "useYearRange=" << (filterUseYearRange ? 1 : 0) << "\n";
    out << "yearRange=" << yearRange << "\n";
    out << "activeTab=" << activeTab << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  DLL Entry Point
// ─────────────────────────────────────────────────────────────────────────────

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
