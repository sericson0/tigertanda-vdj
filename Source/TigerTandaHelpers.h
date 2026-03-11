#pragma once

#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <shlobj.h>

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

#include "TangoMatcher.h"

namespace fs = std::filesystem;

namespace TCol
{
    inline const COLORREF bg          = RGB(18,  21,  31);
    inline const COLORREF panel       = RGB(22,  26,  38);
    inline const COLORREF card        = RGB(26,  30,  44);
    inline const COLORREF cardBorder  = RGB(42,  46,  62);
    inline const COLORREF textNormal  = RGB(176, 180, 192);
    inline const COLORREF textBright  = RGB(220, 224, 235);  // softened from 240,240,240
    inline const COLORREF textDim     = RGB(106, 110, 128);
    inline const COLORREF accent      = RGB(217, 108, 48);
    inline const COLORREF accentBrt   = RGB(243, 161, 15);
    inline const COLORREF buttonBg    = RGB(30,  34,  48);
    inline const COLORREF buttonHover = RGB(40,  44,  62);
    inline const COLORREF good        = RGB(76,  175, 80);
    inline const COLORREF warn        = RGB(255, 152, 0);
    inline const COLORREF bad         = RGB(244, 67,  54);
    inline const COLORREF matchHigh   = RGB(30,  60,  34);
    inline const COLORREF matchMid    = RGB(60,  50,  35);
    inline const COLORREF matchLow    = RGB(60,  28,  28);
    inline const COLORREF matchSel    = RGB(80,  50,  20);

    inline COLORREF scoreColor (float s)
    {
        if (s >= 90.f) return good;
        if (s >= 70.f) return warn;
        return bad;
    }
    inline COLORREF scoreBg (float s)
    {
        if (s >= 90.f) return matchHigh;
        if (s >= 70.f) return matchMid;
        return matchLow;
    }
}

// UTF helpers
std::wstring toWide (const std::string& utf8);
std::string toUtf8 (const std::wstring& wide);

// GDI helpers
HFONT createFont (int height, int weight = FW_NORMAL);
void fillRect (HDC hdc, const RECT& rc, COLORREF color);
void drawText (HDC hdc, const RECT& rc, const std::wstring& text,
               COLORREF color, HFONT font, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE);

// String utilities
std::wstring trimWs (const std::wstring& s);
std::wstring joinNonEmptyParts (const std::vector<std::wstring>& parts, const std::wstring& sep);

// System helpers
bool isVdjHostForeground();

// Prelisten waveform (fake bins from file-path hash, matches TigerTag)
void rebuildPrelistenWaveBins (std::vector<int>& bins, const std::wstring& keyPath);

// Replace Spanish/Latin diacritics with ASCII equivalents for VDJ search
std::wstring normalizeForSearch (const std::wstring& s);
