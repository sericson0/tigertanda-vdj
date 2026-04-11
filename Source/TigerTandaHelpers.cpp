#include "TigerTandaHelpers.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "uxtheme.lib")

// ─────────────────────────────────────────────────────────────────────────────
//  UTF helpers
// ─────────────────────────────────────────────────────────────────────────────

std::wstring toWide (const std::string& utf8)
{
    if (utf8.empty()) return {};
    int len = MultiByteToWideChar (CP_UTF8, 0, utf8.c_str(), (int) utf8.size(), nullptr, 0);
    if (len <= 0) return {};
    std::wstring w (len, L'\0');
    MultiByteToWideChar (CP_UTF8, 0, utf8.c_str(), (int) utf8.size(), &w[0], len);
    return w;
}

std::string toUtf8 (const std::wstring& wide)
{
    if (wide.empty()) return {};
    int len = WideCharToMultiByte (CP_UTF8, 0, wide.c_str(), (int) wide.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string u (len, '\0');
    WideCharToMultiByte (CP_UTF8, 0, wide.c_str(), (int) wide.size(), &u[0], len, nullptr, nullptr);
    return u;
}

// ─────────────────────────────────────────────────────────────────────────────
//  GDI helpers
// ─────────────────────────────────────────────────────────────────────────────

HFONT createFont (int height, int weight)
{
    return CreateFontW (-height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void fillRect (HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH br = CreateSolidBrush (color);
    FillRect (hdc, &rc, br);
    DeleteObject (br);
}

void drawText (HDC hdc, const RECT& rc, const std::wstring& text,
               COLORREF color, HFONT font, UINT flags)
{
    HFONT old = (HFONT) SelectObject (hdc, font);
    SetTextColor (hdc, color);
    SetBkMode (hdc, TRANSPARENT);
    RECT r = rc;
    DrawTextW (hdc, text.c_str(), -1, &r, flags | DT_NOPREFIX);
    SelectObject (hdc, old);
}

// ─────────────────────────────────────────────────────────────────────────────
//  String utilities
// ─────────────────────────────────────────────────────────────────────────────

std::wstring trimWs (const std::wstring& s)
{
    size_t start = 0;
    while (start < s.size() && iswspace (s[start])) ++start;
    size_t end = s.size();
    while (end > start && iswspace (s[end - 1])) --end;
    return s.substr (start, end - start);
}

std::wstring joinNonEmptyParts (const std::vector<std::wstring>& parts, const std::wstring& sep)
{
    std::wstring out;
    for (const auto& part : parts)
    {
        std::wstring t = trimWs (part);
        if (t.empty()) continue;
        if (!out.empty()) out += sep;
        out += t;
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  System helpers
// ─────────────────────────────────────────────────────────────────────────────

// Cached VDJ main HWND — set by TigerTandaUI.cpp via setVdjRootHwnd() once the
// plugin dialog has been created. Until it's set we fall back to "assume
// foreground" so startup UI isn't mysteriously hidden.
static HWND g_vdjRootHwnd = nullptr;

void setVdjRootHwnd (HWND pluginHwnd)
{
    if (pluginHwnd && IsWindow (pluginHwnd))
        g_vdjRootHwnd = GetAncestor (pluginHwnd, GA_ROOT);
    else
        g_vdjRootHwnd = nullptr;
}

bool isVdjHostForeground()
{
    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    // Not yet wired — default to visible rather than permanently hidden.
    if (!g_vdjRootHwnd || !IsWindow (g_vdjRootHwnd))
        return true;

    // Match VDJ itself or any top-level child/popup under the same root
    // (settings dialogs, browser windows, etc.) so we don't hide mid-interaction.
    HWND fgRoot = GetAncestor (fg, GA_ROOT);
    return fgRoot == g_vdjRootHwnd;
}

std::wstring normalizeForSearch (const std::wstring& s)
{
    std::wstring out;
    out.reserve (s.size());
    for (wchar_t c : s)
    {
        switch (c)
        {
            // a variants
            case L'\u00E0': case L'\u00E1': case L'\u00E2':
            case L'\u00E3': case L'\u00E4': case L'\u00E5': out += L'a'; break;
            case L'\u00C0': case L'\u00C1': case L'\u00C2':
            case L'\u00C3': case L'\u00C4': case L'\u00C5': out += L'A'; break;
            // e variants
            case L'\u00E8': case L'\u00E9': case L'\u00EA': case L'\u00EB': out += L'e'; break;
            case L'\u00C8': case L'\u00C9': case L'\u00CA': case L'\u00CB': out += L'E'; break;
            // i variants
            case L'\u00EC': case L'\u00ED': case L'\u00EE': case L'\u00EF': out += L'i'; break;
            case L'\u00CC': case L'\u00CD': case L'\u00CE': case L'\u00CF': out += L'I'; break;
            // o variants
            case L'\u00F2': case L'\u00F3': case L'\u00F4':
            case L'\u00F5': case L'\u00F6': out += L'o'; break;
            case L'\u00D2': case L'\u00D3': case L'\u00D4':
            case L'\u00D5': case L'\u00D6': out += L'O'; break;
            // u variants
            case L'\u00F9': case L'\u00FA': case L'\u00FB': case L'\u00FC': out += L'u'; break;
            case L'\u00D9': case L'\u00DA': case L'\u00DB': case L'\u00DC': out += L'U'; break;
            // n tilde
            case L'\u00F1': out += L'n'; break;
            case L'\u00D1': out += L'N'; break;
            // c cedilla
            case L'\u00E7': out += L'c'; break;
            case L'\u00C7': out += L'C'; break;
            default: out += c; break;
        }
    }
    return out;
}

void rebuildPrelistenWaveBins (std::vector<int>& bins, const std::wstring& keyPath)
{
    bins.assign (64, 0);
    if (keyPath.empty()) return;

    uint32_t seed = 2166136261u;
    for (wchar_t c : keyPath)
    {
        seed ^= (uint32_t) towlower (c);
        seed *= 16777619u;
    }

    uint32_t x = seed ? seed : 1u;
    for (size_t i = 0; i < bins.size(); ++i)
    {
        x = x * 1664525u + 1013904223u;
        int v = 2 + (int) ((x >> 24) & 0x0F);
        if (i > 0)
            v = (v + bins[i - 1]) / 2 + ((int) (x & 0x03) - 1);
        if (v < 2) v = 2;
        if (v > 17) v = 17;
        bins[i] = v;
    }
}
