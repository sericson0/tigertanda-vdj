#pragma once

// Use compiler-defined macros here because this header is included before
// vdjPlugin8.h (which defines VDJ_WIN / VDJ_MAC).
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <shlobj.h>
#endif

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

#include "TangoMatcher.h"

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Cross-platform color type
// ─────────────────────────────────────────────────────────────────────────────

#if defined(__APPLE__)
struct TTColor { float r, g, b; };
#define TTRGB(R,G,B) TTColor{(R)/255.f, (G)/255.f, (B)/255.f}
#else
typedef COLORREF TTColor;
#define TTRGB(R,G,B) RGB(R,G,B)
#endif

namespace TCol
{
    inline const TTColor bg          = TTRGB(18,  21,  31);
    inline const TTColor panel       = TTRGB(22,  26,  38);
    inline const TTColor card        = TTRGB(26,  30,  44);
    inline const TTColor cardBorder  = TTRGB(42,  46,  62);
    inline const TTColor textNormal  = TTRGB(176, 180, 192);
    inline const TTColor textBright  = TTRGB(220, 224, 235);
    inline const TTColor textDim     = TTRGB(106, 110, 128);
    inline const TTColor accent      = TTRGB(217, 108, 48);
    inline const TTColor accentBrt   = TTRGB(243, 161, 15);
    inline const TTColor buttonBg    = TTRGB(30,  34,  48);
    inline const TTColor buttonHover = TTRGB(40,  44,  62);
    inline const TTColor good        = TTRGB(76,  175, 80);
    inline const TTColor warn        = TTRGB(255, 152, 0);
    inline const TTColor bad         = TTRGB(244, 67,  54);
    inline const TTColor matchHigh   = TTRGB(30,  60,  34);
    inline const TTColor matchMid    = TTRGB(60,  50,  35);
    inline const TTColor matchLow    = TTRGB(60,  28,  28);
    inline const TTColor selSubtle   = TTRGB(34,  38,  54);

    inline const TTColor buttonDisabled = TTRGB(24,  28,  42);
    inline const TTColor filterActive   = TTRGB(160, 75,  20);
    inline const TTColor waveformBg     = TTRGB(24,  28,  40);
    inline const TTColor waveformPeak   = TTRGB(255, 255, 255);

#if defined(__APPLE__)
    inline TTColor scoreColor (float s)
    {
        if (s >= 90.f) return good;
        if (s >= 70.f) return warn;
        return bad;
    }
    inline TTColor scoreBg (float s)
    {
        if (s >= 90.f) return matchHigh;
        if (s >= 70.f) return matchMid;
        return matchLow;
    }
#else
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
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  UTF helpers (platform-specific implementations)
// ─────────────────────────────────────────────────────────────────────────────

std::wstring toWide (const std::string& utf8);
std::string toUtf8 (const std::wstring& wide);

// ─────────────────────────────────────────────────────────────────────────────
//  String utilities (portable)
// ─────────────────────────────────────────────────────────────────────────────

std::wstring trimWs (const std::wstring& s);
std::wstring joinNonEmptyParts (const std::vector<std::wstring>& parts, const std::wstring& sep);
std::wstring normalizeForSearch (const std::wstring& s);
void rebuildPrelistenWaveBins (std::vector<int>& bins, const std::wstring& keyPath);

// ─────────────────────────────────────────────────────────────────────────────
//  Platform-specific helpers
// ─────────────────────────────────────────────────────────────────────────────

#if defined(_WIN32)
// GDI helpers
HFONT createFont (int height, int weight = FW_NORMAL);
void fillRect (HDC hdc, const RECT& rc, COLORREF color);
void drawText (HDC hdc, const RECT& rc, const std::wstring& text,
               COLORREF color, HFONT font, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE);

// System helpers
void setVdjRootHwnd (HWND pluginHwnd);
bool isVdjHostForeground();
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Portable sleep wrapper
// ─────────────────────────────────────────────────────────────────────────────

void ttSleep (int ms);

// Portable case-insensitive wide string compare
bool wiEqual (const std::wstring& a, const std::wstring& b);
