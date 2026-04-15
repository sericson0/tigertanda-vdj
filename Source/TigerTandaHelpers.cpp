#include "TigerTandaHelpers.h"

#if defined(_WIN32)
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "uxtheme.lib")
#endif

#include <cwctype>
#include <cstdint>
#include <thread>
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
//  UTF helpers
// ─────────────────────────────────────────────────────────────────────────────

#if defined(_WIN32)
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
#endif

#if defined(__APPLE__)
std::wstring toWide (const std::string& utf8)
{
    if (utf8.empty()) return {};
    std::wstring result;
    result.reserve (utf8.size());
    size_t i = 0;
    while (i < utf8.size())
    {
        uint32_t cp = 0;
        unsigned char c = (unsigned char) utf8[i];
        if (c < 0x80) { cp = c; i += 1; }
        else if (c < 0xC0) { cp = L'?'; i += 1; }
        else if (c < 0xE0) { cp = c & 0x1F; if (i+1 < utf8.size()) cp = (cp << 6) | (utf8[i+1] & 0x3F); i += 2; }
        else if (c < 0xF0) { cp = c & 0x0F; if (i+1 < utf8.size()) cp = (cp << 6) | (utf8[i+1] & 0x3F); if (i+2 < utf8.size()) cp = (cp << 6) | (utf8[i+2] & 0x3F); i += 3; }
        else { cp = c & 0x07; if (i+1 < utf8.size()) cp = (cp << 6) | (utf8[i+1] & 0x3F); if (i+2 < utf8.size()) cp = (cp << 6) | (utf8[i+2] & 0x3F); if (i+3 < utf8.size()) cp = (cp << 6) | (utf8[i+3] & 0x3F); i += 4; }
        result += (wchar_t) cp;
    }
    return result;
}

std::string toUtf8 (const std::wstring& wide)
{
    if (wide.empty()) return {};
    std::string result;
    result.reserve (wide.size() * 2);
    for (wchar_t wc : wide)
    {
        uint32_t cp = (uint32_t) wc;
        if (cp < 0x80)
            result += (char) cp;
        else if (cp < 0x800)
        {
            result += (char) (0xC0 | (cp >> 6));
            result += (char) (0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            result += (char) (0xE0 | (cp >> 12));
            result += (char) (0x80 | ((cp >> 6) & 0x3F));
            result += (char) (0x80 | (cp & 0x3F));
        }
        else
        {
            result += (char) (0xF0 | (cp >> 18));
            result += (char) (0x80 | ((cp >> 12) & 0x3F));
            result += (char) (0x80 | ((cp >> 6) & 0x3F));
            result += (char) (0x80 | (cp & 0x3F));
        }
    }
    return result;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  GDI helpers (Windows only)
// ─────────────────────────────────────────────────────────────────────────────

#if defined(_WIN32)
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
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  String utilities (portable)
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
//  System helpers (Windows only)
// ─────────────────────────────────────────────────────────────────────────────

#if defined(_WIN32)
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
    return true;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Diacritics normalization (portable)
// ─────────────────────────────────────────────────────────────────────────────

std::wstring normalizeForSearch (const std::wstring& s)
{
    std::wstring out;
    out.reserve (s.size());
    for (wchar_t c : s)
    {
        switch (c)
        {
            case L'\u00E0': case L'\u00E1': case L'\u00E2':
            case L'\u00E3': case L'\u00E4': case L'\u00E5': out += L'a'; break;
            case L'\u00C0': case L'\u00C1': case L'\u00C2':
            case L'\u00C3': case L'\u00C4': case L'\u00C5': out += L'A'; break;
            case L'\u00E8': case L'\u00E9': case L'\u00EA': case L'\u00EB': out += L'e'; break;
            case L'\u00C8': case L'\u00C9': case L'\u00CA': case L'\u00CB': out += L'E'; break;
            case L'\u00EC': case L'\u00ED': case L'\u00EE': case L'\u00EF': out += L'i'; break;
            case L'\u00CC': case L'\u00CD': case L'\u00CE': case L'\u00CF': out += L'I'; break;
            case L'\u00F2': case L'\u00F3': case L'\u00F4':
            case L'\u00F5': case L'\u00F6': out += L'o'; break;
            case L'\u00D2': case L'\u00D3': case L'\u00D4':
            case L'\u00D5': case L'\u00D6': out += L'O'; break;
            case L'\u00F9': case L'\u00FA': case L'\u00FB': case L'\u00FC': out += L'u'; break;
            case L'\u00D9': case L'\u00DA': case L'\u00DB': case L'\u00DC': out += L'U'; break;
            case L'\u00F1': out += L'n'; break;
            case L'\u00D1': out += L'N'; break;
            case L'\u00E7': out += L'c'; break;
            case L'\u00C7': out += L'C'; break;
            default: out += c; break;
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Prelisten waveform (portable)
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
//  Portable sleep
// ─────────────────────────────────────────────────────────────────────────────

void ttSleep (int ms)
{
    std::this_thread::sleep_for (std::chrono::milliseconds (ms));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Portable case-insensitive wide string compare
// ─────────────────────────────────────────────────────────────────────────────

bool wiEqual (const std::wstring& a, const std::wstring& b)
{
#if defined(_WIN32)
    return _wcsicmp (a.c_str(), b.c_str()) == 0;
#else
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (towlower (a[i]) != towlower (b[i])) return false;
    return true;
#endif
}
