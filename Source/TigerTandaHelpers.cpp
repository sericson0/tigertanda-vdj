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

bool isVdjHostForeground()
{
    DWORD fgPid = 0;
    HWND  fg    = GetForegroundWindow();
    if (fg) GetWindowThreadProcessId (fg, &fgPid);
    return fgPid == GetCurrentProcessId();
}
