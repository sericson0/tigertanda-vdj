//==============================================================================
// TigerTanda VDJ Plugin - Main Window UI
// TandaWndProc: WM_CREATE, WM_PAINT, WM_TIMER, WM_COMMAND, WM_DRAWITEM
//==============================================================================

#include "TigerTanda.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <cmath>

using namespace Gdiplus;

// ─────────────────────────────────────────────────────────────────────────────
//  Window class registration
// ─────────────────────────────────────────────────────────────────────────────

static bool wndClassRegistered = false;

void ensureTandaWindowClass (HINSTANCE hInst)
{
    if (wndClassRegistered) return;
    WNDCLASSEXW wc {};
    wc.cbSize        = sizeof (wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = TandaWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor (nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush (TCol::bg);
    wc.lpszClassName = WND_CLASS;
    RegisterClassExW (&wc);
    wndClassRegistered = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

static TigerTandaPlugin* getPlugin (HWND hwnd)
{
    return reinterpret_cast<TigerTandaPlugin*> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Owner-draw buttons (flat dark style)
// ─────────────────────────────────────────────────────────────────────────────

static void drawOwnerButton (const DRAWITEMSTRUCT* di, const std::wstring& label,
                             COLORREF bgColor, COLORREF fgColor, HFONT font,
                             bool extraHover = false)
{
    HDC hdc = di->hDC;
    RECT r = di->rcItem;
    bool pressed  = (di->itemState & ODS_SELECTED) != 0;
    bool disabled = (di->itemState & ODS_DISABLED) != 0;
    bool hovered  = (di->itemState & ODS_HOTLIGHT)  != 0 || extraHover;

    COLORREF bg = pressed  ? TCol::buttonHover
                : disabled ? RGB (24, 28, 42)
                           : bgColor;
    if (hovered && !pressed && !disabled)
    {
        BYTE hr = (BYTE)(GetRValue (bg) + 12 > 255 ? 255 : GetRValue (bg) + 12);
        BYTE hg = (BYTE)(GetGValue (bg) + 12 > 255 ? 255 : GetGValue (bg) + 12);
        BYTE hb = (BYTE)(GetBValue (bg) + 12 > 255 ? 255 : GetBValue (bg) + 12);
        bg = RGB (hr, hg, hb);
    }

    HRGN rgn = CreateRoundRectRgn (r.left, r.top, r.right, r.bottom, 6, 6);
    SelectClipRgn (hdc, rgn);
    fillRect (hdc, r, bg);
    SelectClipRgn (hdc, nullptr);
    DeleteObject (rgn);

    HPEN pen    = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN oldPen = (HPEN) SelectObject (hdc, pen);
    HBRUSH oldBr = (HBRUSH) SelectObject (hdc, GetStockObject (NULL_BRUSH));
    RoundRect (hdc, r.left, r.top, r.right, r.bottom, 6, 6);
    SelectObject (hdc, oldPen);
    SelectObject (hdc, oldBr);
    DeleteObject (pen);

    COLORREF fg = disabled ? TCol::textDim : fgColor;
    drawText (hdc, r, label, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// Flat text toggle — orange+bold when active, gray+normal otherwise
static void drawTextToggle (const DRAWITEMSTRUCT* di, const std::wstring& label,
                            bool isActive, HFONT fontNormal, HFONT fontBold,
                            COLORREF bgColor = TCol::panel, bool extraHover = false)
{
    HDC hdc = di->hDC;
    RECT r = di->rcItem;
    bool pressed = (di->itemState & ODS_SELECTED) != 0;
    bool hovered = (di->itemState & ODS_HOTLIGHT)  != 0 || extraHover;

    COLORREF fillBg = bgColor;
    if (hovered && !isActive && !pressed)
    {
        BYTE hr = (BYTE)(GetRValue (bgColor) + 18 > 255 ? 255 : GetRValue (bgColor) + 18);
        BYTE hg = (BYTE)(GetGValue (bgColor) + 18 > 255 ? 255 : GetGValue (bgColor) + 18);
        BYTE hb = (BYTE)(GetBValue (bgColor) + 18 > 255 ? 255 : GetBValue (bgColor) + 18);
        fillBg = RGB (hr, hg, hb);
    }
    fillRect (hdc, r, fillBg);

    COLORREF fg = (isActive || pressed) ? TCol::accentBrt : hovered ? TCol::textNormal : TCol::textDim;
    HFONT font  = (isActive || pressed) ? fontBold : fontNormal;
    drawText (hdc, r, label, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    HPEN pen = CreatePen (PS_SOLID, isActive ? 2 : 1,
                          isActive ? TCol::accent : TCol::cardBorder);
    HPEN old = (HPEN) SelectObject (hdc, pen);
    MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
    LineTo   (hdc, r.right, r.bottom - 1);
    SelectObject (hdc, old);
    DeleteObject (pen);
}

// Small match-score dot
static void drawScoreDot (HDC hdc, int cx, int cy, float score)
{
    COLORREF col = TCol::scoreColor (score);
    HBRUSH br = CreateSolidBrush (col);
    HBRUSH oldBr = (HBRUSH) SelectObject (hdc, br);
    HPEN pen = CreatePen (PS_SOLID, 1, col);
    HPEN oldPen = (HPEN) SelectObject (hdc, pen);
    Ellipse (hdc, cx - 4, cy - 4, cx + 4, cy + 4);
    SelectObject (hdc, oldPen);
    SelectObject (hdc, oldBr);
    DeleteObject (pen);
    DeleteObject (br);
}

// Format raw date (M/D/YYYY or YYYY-M-D) to YYYY-mm-dd
static std::wstring formatDateYMD (const std::wstring& raw)
{
    if (raw.empty()) return raw;
    std::wstring parts[3];
    int idx = 0;
    for (wchar_t c : raw)
    {
        if ((c == L'/' || c == L'-') && idx < 2)
            ++idx;
        else
            parts[idx] += c;
    }
    auto pad2 = [] (const std::wstring& s) -> std::wstring {
        return s.size() < 2 ? L"0" + s : s;
    };
    if (parts[2].size() == 4)  // M/D/YYYY
        return parts[2] + L"-" + pad2 (parts[0]) + L"-" + pad2 (parts[1]);
    if (parts[0].size() == 4)  // YYYY-M-D
        return parts[0] + L"-" + pad2 (parts[1]) + L"-" + pad2 (parts[2]);
    return raw;
}

// Metadata detail box for selected result
// Row 1: Bandleader · Singer
// Row 2: Date (YYYY-mm-dd) · Genre · Label
// Row 3: Orchestra
// Row 4: Group: <grouping>
// fontRow1 = fontSmallBold (11pt bold), fontRows234 = fontNormal (13pt)
// Both reduced 2pt from previous fontBold(13) / fontDetail(15) to fix descender clipping
static void drawResultDetailBox (HDC hdc, RECT r, const TgRecord& rec,
                                 HFONT fontRow1, HFONT fontRows234, HFONT fontSmall)
{
    fillRect (hdc, r, TCol::card);

    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN old = (HPEN) SelectObject (hdc, pen);
    MoveToEx (hdc, r.left,  r.top, nullptr);
    LineTo   (hdc, r.right, r.top);
    SelectObject (hdc, old);
    DeleteObject (pen);

    int px = r.left + 6;
    int py = r.top + 4;
    const int lineH = 17;  // increased from 16 — gives descenders room at 13pt

    // Row 1: Bandleader · Singer
    std::wstring line1 = rec.bandleader;
    if (!rec.singer.empty()) { if (!line1.empty()) line1 += L"  \u00B7  "; line1 += rec.singer; }
    RECT r1 { px, py, r.right - 6, py + lineH };
    drawText (hdc, r1, line1, TCol::textBright, fontRow1,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    py += lineH + 1;

    // Row 2: Date (YYYY-mm-dd) · Genre · Label
    std::wstring dateStr = formatDateYMD (rec.date);
    std::wstring line2;
    if (!dateStr.empty())    line2 += dateStr;
    if (!rec.genre.empty())  { if (!line2.empty()) line2 += L"  \u00B7  "; line2 += rec.genre; }
    if (!rec.label.empty())  { if (!line2.empty()) line2 += L"  \u00B7  "; line2 += rec.label; }
    RECT r2 { px, py, r.right - 6, py + lineH };
    drawText (hdc, r2, line2, TCol::textDim, fontRows234,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    py += lineH + 1;

    // Row 3: Orchestra
    if (!rec.orchestra.empty())
    {
        RECT r3 { px, py, r.right - 6, py + lineH };
        drawText (hdc, r3, rec.orchestra, TCol::textDim, fontRows234,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    py += lineH + 1;

    // Row 4: Group: <grouping>
    if (!rec.grouping.empty())
    {
        std::wstring line4 = L"Group: " + rec.grouping;
        RECT r4 { px, py, r.right - 6, py + lineH };
        drawText (hdc, r4, line4, TCol::textDim, fontRows234,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

// Draw prelisten waveform into a RECT (double-buffered)
static void drawPrelistenWave (HDC hdc, RECT wr, const std::vector<int>& bins, double posPercent)
{
    int ww = wr.right - wr.left;
    int wh = wr.bottom - wr.top;
    HDC memDC = CreateCompatibleDC (hdc);
    HBITMAP memBmp = CreateCompatibleBitmap (hdc, ww, wh);
    HBITMAP oldBmp = (HBITMAP) SelectObject (memDC, memBmp);

    RECT localWr = { 0, 0, ww, wh };
    fillRect (memDC, localWr, RGB (24, 28, 40));

    {
        HPEN border = CreatePen (PS_SOLID, 1, TCol::cardBorder);
        HPEN oldPen = (HPEN) SelectObject (memDC, border);
        HBRUSH oldBrush = (HBRUSH) SelectObject (memDC, GetStockObject (NULL_BRUSH));
        Rectangle (memDC, 0, 0, ww, wh);
        SelectObject (memDC, oldPen);
        SelectObject (memDC, oldBrush);
        DeleteObject (border);
    }

    if (!bins.empty())
    {
        int innerL = 2, innerR = ww - 2, innerT = 2, innerB = wh - 2;
        int innerW = (innerR - innerL);
        int centerY = (innerT + innerB) / 2;
        int n = (int) bins.size();
        int headX = innerL + (int) (posPercent / 100.0 * innerW);

        if (n > 0 && innerW > n)
        {
            for (int i = 0; i < n; ++i)
            {
                int x0 = innerL + (i * innerW) / n;
                int x1 = innerL + ((i + 1) * innerW) / n;
                int barW = (x1 - x0);
                if (barW < 1) barW = 1;
                int amp = bins[(size_t) i];
                int h = (amp * (innerB - innerT)) / 20;
                if (h < 1) h = 1;
                RECT b = { x0, centerY - h / 2, x0 + barW, centerY + (h + 1) / 2 };
                fillRect (memDC, b, (x0 < headX) ? TCol::accent : RGB (96, 108, 132));
            }
            RECT ph = { headX, innerT, headX + 1, innerB };
            fillRect (memDC, ph, RGB (255, 255, 255));
        }
    }

    BitBlt (hdc, wr.left, wr.top, ww, wh, memDC, 0, 0, SRCCOPY);
    SelectObject (memDC, oldBmp);
    DeleteObject (memBmp);
    DeleteDC (memDC);
}

// ─────────────────────────────────────────────────────────────────────────────
//  applyLayout – reposition + show/hide all controls
// ─────────────────────────────────────────────────────────────────────────────

static void applyLayout (TigerTandaPlugin* p, HWND hwnd)
{
    if (!p || !hwnd) return;

    SetWindowPos (hwnd, nullptr, 0, 0, DLG_W, DLG_H,
                  SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    const int lx = PAD;
    const int lw = DLG_W - PAD * 2;
    const int topY = (TOP_H - TAB_BTN_H) / 2;

    // Close button — right-anchored in top bar
    MoveWindow (p->hBtnClose, DLG_W - 26, topY, 22, TAB_BTN_H, FALSE);
    ShowWindow (p->hBtnClose, SW_SHOW);

    // 4 tab buttons occupy remaining top bar left of close
    {
        const int rightBtnsW = 22 + PAD;     // close + right pad
        const int tabAreaW   = DLG_W - rightBtnsW;
        const int tabW       = tabAreaW / 4;
        MoveWindow (p->hBtnTabTrack,    0,         topY, tabW,              TAB_BTN_H, FALSE);
        MoveWindow (p->hBtnTabMatches,  tabW,      topY, tabW,              TAB_BTN_H, FALSE);
        MoveWindow (p->hBtnTabBrowse,   tabW * 2,  topY, tabW,              TAB_BTN_H, FALSE);
        MoveWindow (p->hBtnTabSettings, tabW * 3,  topY, tabAreaW - tabW*3, TAB_BTN_H, FALSE);
        ShowWindow (p->hBtnTabTrack,    SW_SHOW);
        ShowWindow (p->hBtnTabMatches,  SW_SHOW);
        ShowWindow (p->hBtnTabBrowse,   SW_SHOW);
        ShowWindow (p->hBtnTabSettings, SW_SHOW);
    }

    const int bodyY  = TOP_H;
    const bool showT = (p->activeTab == 0);  // Track
    const bool showM = (p->activeTab == 1);  // Matches
    const bool showB = (p->activeTab == 2);  // Browse
    const bool showS = (p->activeTab == 3);  // Settings

    auto showCtrl = [](HWND h, bool vis)
    {
        if (h) ShowWindow (h, vis ? SW_SHOW : SW_HIDE);
    };

    // ── Track tab ─────────────────────────────────────────────────────────────
    if (showT)
    {
        int ly = bodyY + PAD;

        // Search row: [Title][Artist][Q]
        const int sbW = BTN_H;
        const int gap = 4;
        const int etW = lw - sbW - gap;
        const int tW  = etW * 55 / 100;
        const int aW  = etW - tW - gap;
        MoveWindow (p->hEditTitle,  lx,            ly, tW,  EDIT_H, FALSE);
        MoveWindow (p->hEditArtist, lx + tW + gap, ly, aW,  EDIT_H, FALSE);
        MoveWindow (p->hBtnSearch,  lx + lw - sbW, ly, sbW, BTN_H,  FALSE);
        ly += EDIT_H + TRACK_SEARCH_GAP;

        // Leave BRAND_H at bottom for Tiger Tanda brand
        MoveWindow (p->hCandList, lx, ly, lw, DLG_H - ly - PAD - BRAND_H - 2, FALSE);
    }
    showCtrl (p->hEditTitle,     showT);
    showCtrl (p->hEditArtist,    showT);
    showCtrl (p->hBtnSearch,     showT);
    showCtrl (p->hCandList,      showT);

    // ── Matches tab ───────────────────────────────────────────────────────────
    if (showM)
    {
        int ry = bodyY + PAD;
        // FIND TRACK button shares the brand row (right-aligned)
        const int brandY  = DLG_H - PAD - BRAND_H;
        const int vdjBtnW = 100;
        MoveWindow (p->hBtnSearchVdj, lx + lw - vdjBtnW, brandY, vdjBtnW, BRAND_H, FALSE);

        const int detailTop = brandY - DETAIL_BOX_H - 4;
        MoveWindow (p->hResultsList, lx, ry, lw, detailTop - ry - 2, FALSE);
    }
    showCtrl (p->hResultsList,  showM);
    showCtrl (p->hBtnSearchVdj, showM);

    // ── Browse tab ────────────────────────────────────────────────────────────
    if (showB)
    {
        // Combined prelisten+ADD row sits above the brand row
        const int preRowY  = DLG_H - PAD - BRAND_H - 6 - BTN_H;
        const int listBot  = preRowY - 6;
        const int listH    = listBot - (bodyY + PAD);

        MoveWindow (p->hBrowseList, lx, bodyY + PAD, lw, listH, FALSE);

        // Prelisten row: [▶ btn][waveform ... ][ADD btn]
        const int preBtnW = 28;
        const int addBtnW = 72;  // +20% wider (was 60)
        MoveWindow (p->hBtnPrelisten, lx, preRowY, preBtnW, BTN_H, FALSE);
        MoveWindow (p->hBtnAddEnd,    lx + lw - addBtnW, preRowY, addBtnW, BTN_H, FALSE);

        // Store waveform rect for painting (between prelisten btn and ADD btn)
        p->prelistenWaveRect = { lx + preBtnW + 4, preRowY,
                                 lx + lw - addBtnW - 4, preRowY + BTN_H };
    }
    showCtrl (p->hBrowseList,    showB);
    showCtrl (p->hBtnPrelisten,  showB);
    showCtrl (p->hBtnAddEnd,     showB);

    // ── Settings tab ──────────────────────────────────────────────────────────
    if (showS)
    {
        const int btnH = BTN_H - 4;  // 20 — slightly smaller filter buttons
        const int gap  = 4;
        const int colW = (lw - gap * 2) / 3;

        int sy = bodyY + PAD - 5;  // how-tabs 5pt closer to top bar

        // "How it works" sub-tabs row at top
        const int howTabW = lw / 5;
        for (int i = 0; i < 5; ++i)
            MoveWindow (p->hBtnHowTabs[i], lx + i * howTabW, sy, howTabW, 18, FALSE);
        sy += 18 + 4;  // 18px tabs + 4px gap

        // Content area (drawn in WM_PAINT): 4 rows × 15px = 60px, then separator
        sy += 60 + 13;  // skip painted content + 8 gap + 5 extra to push filters down

        // Filter Row 1: ARTIST, SINGER, GENRE
        MoveWindow (p->hChkArtist,    lx,                    sy, colW, btnH, FALSE);
        MoveWindow (p->hChkSinger,    lx + colW + gap,       sy, colW, btnH, FALSE);
        MoveWindow (p->hChkGenre,     lx + colW * 2 + gap*2, sy, lw - colW*2 - gap*2, btnH, FALSE);
        sy += btnH + 4;

        // Filter Row 2: GROUPING, LABEL, ORCHESTRA
        MoveWindow (p->hChkGrouping,  lx,                    sy, colW, btnH, FALSE);
        MoveWindow (p->hChkLabel,     lx + colW + gap,       sy, colW, btnH, FALSE);
        MoveWindow (p->hChkOrchestra, lx + colW * 2 + gap*2, sy, lw - colW*2 - gap*2, btnH, FALSE);
        sy += btnH + 4;

        // Filter Row 3: [YEAR][±N] (half-col each, left) ... [TRACK] (right)
        const int halfCol = (colW - gap) / 2;
        MoveWindow (p->hBtnYearToggle,  lx,                    sy, halfCol, btnH, FALSE);
        MoveWindow (p->hBtnYearRange,   lx + halfCol + gap,    sy, halfCol, btnH, FALSE);
        MoveWindow (p->hChkTrack,       lx + colW * 2 + gap*2, sy, lw - colW*2 - gap*2, btnH, FALSE);

    }
    showCtrl (p->hChkArtist,      showS);
    showCtrl (p->hChkSinger,      showS);
    showCtrl (p->hChkGrouping,    showS);
    showCtrl (p->hChkGenre,       showS);
    showCtrl (p->hChkOrchestra,   showS);
    showCtrl (p->hChkLabel,       showS);
    showCtrl (p->hChkTrack,       showS);
    showCtrl (p->hBtnYearToggle,  showS);
    showCtrl (p->hBtnYearRange,   showS);
    for (int i = 0; i < 5; ++i)
        showCtrl (p->hBtnHowTabs[i], showS);

    InvalidateRect (hwnd, nullptr, TRUE);
}

// Invalidate all 4 tab buttons
static void repaintTabs (TigerTandaPlugin* p)
{
    if (p->hBtnTabTrack)    InvalidateRect (p->hBtnTabTrack,    nullptr, FALSE);
    if (p->hBtnTabMatches)  InvalidateRect (p->hBtnTabMatches,  nullptr, FALSE);
    if (p->hBtnTabBrowse)   InvalidateRect (p->hBtnTabBrowse,   nullptr, FALSE);
    if (p->hBtnTabSettings) InvalidateRect (p->hBtnTabSettings, nullptr, FALSE);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Window procedure
// ─────────────────────────────────────────────────────────────────────────────

LRESULT CALLBACK TandaWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TigerTandaPlugin* p = nullptr;

    if (msg == WM_CREATE)
    {
        auto cs = reinterpret_cast<CREATESTRUCT*> (lParam);
        p = reinterpret_cast<TigerTandaPlugin*> (cs->lpCreateParams);
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, (LONG_PTR) p);
    }
    else
    {
        p = getPlugin (hwnd);
    }

    switch (msg)
    {
    // ── Close ────────────────────────────────────────────────────────────────
    case WM_CLOSE:
        if (p)
        {
            p->dialogRequestedOpen  = false;
            p->suppressNextHideSync = true;
        }
        ShowWindow (hwnd, SW_HIDE);
        return 0;

    // ── Show/hide state sync ─────────────────────────────────────────────────
    case WM_SHOWWINDOW:
        if (p)
        {
            if (wParam)
                p->dialogRequestedOpen = true;
            else if (p->suppressNextHideSync)
                p->suppressNextHideSync = false;
            else
                p->dialogRequestedOpen = false;
        }
        return 0;

    // ── Drag by top bar ──────────────────────────────────────────────────────
    case WM_NCHITTEST:
    {
        LRESULT hit = DefWindowProcW (hwnd, msg, wParam, lParam);
        if (hit == HTCLIENT)
        {
            POINT pt = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
            ScreenToClient (hwnd, &pt);
            if (pt.y < TOP_H)
                return HTCAPTION;
        }
        return hit;
    }

    // ── Hover tracking via WM_SETCURSOR (wParam = HWND under cursor) ─────────
    case WM_SETCURSOR:
    {
        if (p)
        {
            HWND tracked[] = {
                p->hBtnClose, p->hBtnSearch, p->hBtnSearchVdj, p->hBtnPrelisten,
                p->hBtnAddEnd, p->hBtnYearToggle, p->hBtnYearRange,
                p->hChkArtist, p->hChkSinger,
                p->hChkGrouping, p->hChkGenre, p->hChkOrchestra, p->hChkLabel,
                p->hChkTrack,
                p->hBtnTabTrack, p->hBtnTabMatches, p->hBtnTabBrowse, p->hBtnTabSettings,
                p->hBtnHowTabs[0], p->hBtnHowTabs[1], p->hBtnHowTabs[2],
                p->hBtnHowTabs[3], p->hBtnHowTabs[4]
            };
            HWND newHover = nullptr;
            for (HWND h : tracked)
                if (h && (HWND) wParam == h) { newHover = h; break; }
            if (p->hoveredBtn != newHover)
            {
                HWND old = p->hoveredBtn;
                p->hoveredBtn = newHover;
                if (old)      InvalidateRect (old,      nullptr, FALSE);
                if (newHover) InvalidateRect (newHover, nullptr, FALSE);
            }
        }
        return FALSE;
    }

    // ── Create controls ──────────────────────────────────────────────────────
    case WM_CREATE:
    {
        if (!p) break;

        auto mkBtn = [&](int id, const wchar_t* text,
                         DWORD style = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW) -> HWND
        {
            return CreateWindowW (L"BUTTON", text, style,
                                  0, 0, 10, 10, hwnd, (HMENU)(intptr_t)id, nullptr, nullptr);
        };

        // Top bar
        p->hBtnClose        = mkBtn (IDC_BTN_CLOSE,        L"X");
        p->hBtnTabTrack     = mkBtn (IDC_BTN_TAB_TRACK,    L"Track");
        p->hBtnTabMatches   = mkBtn (IDC_BTN_TAB_MATCHES,  L"Matches");
        p->hBtnTabBrowse    = mkBtn (IDC_BTN_TAB_BROWSE,   L"Browse");
        p->hBtnTabSettings  = mkBtn (IDC_BTN_TAB_SETTINGS, L"\u26ED");  // ⛭

        // Search row
        p->hEditTitle = CreateWindowExW (WS_EX_CLIENTEDGE, L"EDIT", L"",
                                         WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                         0, 0, 10, 10, hwnd,
                                         (HMENU) IDC_EDIT_TITLE, nullptr, nullptr);
        SendMessageW (p->hEditTitle, EM_SETCUEBANNER, TRUE, (LPARAM) L"Title...");

        p->hEditArtist = CreateWindowExW (WS_EX_CLIENTEDGE, L"EDIT", L"",
                                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                          0, 0, 10, 10, hwnd,
                                          (HMENU) IDC_EDIT_ARTIST, nullptr, nullptr);
        SendMessageW (p->hEditArtist, EM_SETCUEBANNER, TRUE, (LPARAM) L"Artist...");

        p->hBtnSearch = mkBtn (IDC_BTN_SEARCH, L"");

        // Filter buttons (ALL CAPS)
        p->hChkArtist    = mkBtn (IDC_CHK_SAME_ARTIST,    L"ARTIST");
        p->hChkSinger    = mkBtn (IDC_CHK_SAME_SINGER,    L"SINGER");
        p->hChkGrouping  = mkBtn (IDC_CHK_SAME_GROUPING,  L"GROUPING");
        p->hChkGenre     = mkBtn (IDC_CHK_SAME_GENRE,     L"GENRE");
        p->hChkOrchestra = mkBtn (IDC_CHK_SAME_ORCHESTRA, L"ORCHESTRA");
        p->hChkLabel     = mkBtn (IDC_CHK_SAME_LABEL,     L"LABEL");
        p->hChkTrack     = mkBtn (IDC_CHK_SAME_TRACK,     L"TRACK");

        // Year range controls
        p->hBtnYearToggle = mkBtn (IDC_BTN_YEAR_TOGGLE, L"YEAR");
        // Year range button (cycles through values on click)
        {
            static const wchar_t* kYrLabels[] = { L"\u00B10", L"\u00B11", L"\u00B12", L"\u00B13", L"\u00B15", L"\u00B110" };
            static const int      kYrVals[]   = { 0, 1, 2, 3, 5, 10 };
            int initIdx = 0;
            for (int i = 0; i < 6; ++i)
                if (kYrVals[i] == p->yearRange) { initIdx = i; break; }
            p->hBtnYearRange = mkBtn (IDC_BTN_YEAR_RANGE, kYrLabels[initIdx]);
        }

        // "How it works" sub-tab buttons (Settings tab)
        static const wchar_t* kHowNames[] = { L"Overview", L"Track", L"Matches", L"Browser", L"Filters" };
        for (int i = 0; i < 5; ++i)
            p->hBtnHowTabs[i] = mkBtn (IDC_BTN_HOW_TAB_0 + i, kHowNames[i]);

        // Candidates list
        p->hCandList = CreateWindowW (L"LISTBOX", nullptr,
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL
                                      | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS
                                      | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                      0, 0, 10, 10, hwnd,
                                      (HMENU) IDC_CANDIDATES_LIST, nullptr, nullptr);

        // Results list
        p->hResultsList = CreateWindowW (L"LISTBOX", nullptr,
                                         WS_CHILD | WS_VISIBLE | WS_VSCROLL
                                         | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS
                                         | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                         0, 0, 10, 10, hwnd,
                                         (HMENU) IDC_RESULTS_LIST, nullptr, nullptr);

        // Browse list
        p->hBrowseList = CreateWindowW (L"LISTBOX", nullptr,
                                        WS_CHILD | WS_VISIBLE | WS_VSCROLL
                                        | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS
                                        | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                        0, 0, 10, 10, hwnd,
                                        (HMENU) IDC_BROWSE_LIST, nullptr, nullptr);

        // Prelisten + action buttons
        p->hBtnPrelisten = mkBtn (IDC_BTN_PRELISTEN, L"\u25B6");  // ▶
        p->hBtnAddEnd    = mkBtn (IDC_BTN_ADD_END,   L"ADD");

        // Find Track button (Matches tab, brand row) — created before tooltips so tip registers
        p->hBtnSearchVdj = mkBtn (IDC_BTN_SEARCH_VDJ, L"FIND TRACK");

        // Tooltips for all buttons
        p->hTooltip = CreateWindowExW (0, TOOLTIPS_CLASS, nullptr,
                                       WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       hwnd, nullptr, nullptr, nullptr);
        if (p->hTooltip)
        {
            SendMessageW (p->hTooltip, TTM_SETMAXTIPWIDTH, 0, 220);
            SendMessageW (p->hTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 2000);

            auto addTip = [&](HWND h, const wchar_t* tip)
            {
                if (!h) return;
                TOOLINFOW ti {};
                ti.cbSize   = sizeof (ti);
                ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
                ti.hwnd     = hwnd;
                ti.uId      = (UINT_PTR) h;
                ti.lpszText = (LPWSTR) tip;
                SendMessageW (p->hTooltip, TTM_ADDTOOLW, 0, (LPARAM) &ti);
            };

            addTip (p->hBtnClose,        L"Close Tiger Tanda");
            addTip (p->hBtnSearch,       L"Search database for entered title/artist");
            addTip (p->hBtnSearchVdj,    L"Find track in VDJ browser");
            addTip (p->hBtnPrelisten,    L"Preview selected song");
            addTip (p->hBtnAddEnd,       L"Add selected song to end of playlist");
            addTip (p->hChkArtist,       L"Filter: same bandleader / artist");
            addTip (p->hChkSinger,       L"Filter: same singer");
            addTip (p->hChkGrouping,     L"Filter: same recording period / group");
            addTip (p->hChkGenre,        L"Filter: same genre");
            addTip (p->hChkOrchestra,    L"Filter: same orchestra");
            addTip (p->hChkLabel,        L"Filter: same record label");
            addTip (p->hChkTrack,        L"Filter: same track title");
            addTip (p->hBtnYearToggle,   L"Toggle year-range filter on/off");
            addTip (p->hBtnYearRange,    L"Click to cycle year range");
        }

        // Sync year toggle text (just "YEAR" — active state shown by color)
        SetWindowTextW (p->hBtnYearToggle, L"YEAR");

        // Apply font to all children
        if (p->fontNormal)
        {
            EnumChildWindows (hwnd, [] (HWND child, LPARAM lp) -> BOOL {
                SendMessageW (child, WM_SETFONT, (WPARAM) lp, TRUE);
                return TRUE;
            }, (LPARAM) p->fontNormal);
        }

        applyLayout (p, hwnd);
        SetTimer (hwnd, TIMER_BROWSE_POLL, 250, nullptr);
        return 0;
    }

    // ── Paint ────────────────────────────────────────────────────────────────
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint (hwnd, &ps);

        RECT clientR;
        GetClientRect (hwnd, &clientR);

        fillRect (hdc, clientR, TCol::bg);

        // Top bar
        RECT topR { 0, 0, DLG_W, TOP_H };
        fillRect (hdc, topR, TCol::panel);

        if (!p) { EndPaint (hwnd, &ps); return 0; }

        RECT bodyR { 0, TOP_H, DLG_W, DLG_H };
        fillRect (hdc, bodyR, TCol::bg);

        // ── Brand text — Track / Matches / Browse tabs only (not Settings) ───
        if (p->activeTab != 3)
        {
            const int lx2 = PAD;
            const int lw2 = DLG_W - PAD * 2;
            int brandW = lw2;
            RECT brandR { lx2, DLG_H - PAD - BRAND_H, lx2 + brandW, DLG_H - PAD };
            drawText (hdc, brandR, L"Tiger Tanda", TCol::accent, p->fontTitle,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // ── Matches tab paint ─────────────────────────────────────────────────
        if (p->activeTab == 1)
        {
            const int lx = PAD;
            const int lw = DLG_W - PAD * 2;
            const int brandY    = DLG_H - PAD - BRAND_H;
            const int detailTop = brandY - DETAIL_BOX_H - 4;
            RECT detR { lx, detailTop, lx + lw, detailTop + DETAIL_BOX_H };

            if (p->selectedResultIdx >= 0 && p->selectedResultIdx < (int) p->results.size())
                drawResultDetailBox (hdc, detR, p->results[p->selectedResultIdx],
                                     p->fontBold, p->fontNormal, p->fontSmall);
            else
            {
                fillRect (hdc, detR, TCol::card);
                HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN old = (HPEN) SelectObject (hdc, pen);
                MoveToEx (hdc, detR.left, detR.top, nullptr);
                LineTo   (hdc, detR.right, detR.top);
                SelectObject (hdc, old);
                DeleteObject (pen);
                RECT txtR { detR.left + 6, detR.top + 4, detR.right - 6, detR.bottom - 4 };
                drawText (hdc, txtR, L"Select a song to see details", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // ── Browse tab paint ──────────────────────────────────────────────────
        if (p->activeTab == 2)
        {
            // Prelisten waveform
            RECT wr = p->prelistenWaveRect;
            if (wr.right > wr.left)
                drawPrelistenWave (hdc, wr, p->prelistenWaveBins, p->prelistenPos);
        }

        // ── Settings tab paint ────────────────────────────────────────────────
        if (p->activeTab == 3)
        {
            const int lx  = PAD;
            const int lw  = DLG_W - PAD * 2;
            const int bY  = TOP_H + PAD - 5;  // matches applyLayout how-tabs -5 offset

            // Content area: below how-tab buttons (tabs at bY, h=18; content at bY+22)
            const int contentY = bY + 18 + 4;

            if (p->activeHowTab < 4)
            {
                static const wchar_t* kContent[4][5] = {
                    // 0 Overview
                    { L"\u2022  Tiger Tanda helps you build tandas",
                      L"\u2022  Matches song to other tracks that can work in a tanda",
                      L"\u2022  Options to customize matching criteria",
                      nullptr },
                    // 1 Track
                    { L"\u2022 Select track in VDJ",
                      L"\u2022 Use search bar if correct match isn't shown",
                      L"\u2022 Choose matching candidate",
                      nullptr, nullptr },
                    // 2 Matches
                    { L"\u2022  Show similar tracks based on set filters",
                      L"\u2022  Select track to see details below",
                      L"\u2022  Click FIND to search for selected track",
                      nullptr, nullptr },
                    // 3 Browser
                    { L"\u2022  Display ranked search results",
                      L"\u2022  Click candidate to focus VDJ on that file",
                      L"\u2022  ADD selected file to end of playlist",
                      nullptr },
                };
                for (int i = 0; i < 5; ++i)
                {
                    if (!kContent[p->activeHowTab][i]) break;
                    RECT br { lx, contentY + i * 15, lx + lw, contentY + i * 15 + 15 };
                    drawText (hdc, br, kContent[p->activeHowTab][i], TCol::textNormal, p->fontSmall,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
            }
            else  // Filters tab — inline bold for keyword terms
            {
                // Helper: draw a line with one leading bold word followed by normal text
                auto drawBoldLine = [&] (int lineIdx, const wchar_t* boldWord,
                                         const wchar_t* rest, bool hasBold = true)
                {
                    int ly = contentY + lineIdx * 15;
                    int x  = lx;
                    if (hasBold)
                    {
                        SIZE sz {};
                        SelectObject (hdc, p->fontSmallBold);
                        GetTextExtentPoint32W (hdc, boldWord, (int) wcslen (boldWord), &sz);
                        SetBkMode (hdc, TRANSPARENT);
                        SetTextColor (hdc, TCol::textBright);
                        ExtTextOutW (hdc, x, ly + (15 - sz.cy) / 2, ETO_CLIPPED,
                                     nullptr, boldWord, (UINT) wcslen (boldWord), nullptr);
                        x += sz.cx;
                    }
                    if (rest && rest[0])
                    {
                        RECT rr { x, ly, lx + lw, ly + 15 };
                        drawText (hdc, rr, rest, TCol::textNormal, p->fontSmall,
                                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                };

                drawBoldLine (0, L"\u2022  Group:",    L" Similar period");
                drawBoldLine (1, L"\u2022  Label:",    L" Record label");
                drawBoldLine (2, L"\u2022  Year:",     L" Max difference in recording years");
            }

            // Separator: midpoint between content bottom (bY+22+3*15=bY+67) and filters top (bY+95)
            const int sepY = bY + 81;
            HPEN sep = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, sep);
            MoveToEx (hdc, lx, sepY, nullptr);
            LineTo   (hdc, lx + lw, sepY);
            SelectObject (hdc, old);
            DeleteObject (sep);

            // ── Logo (fills bottom of settings — no brand text on this tab) ──
            const int logoH    = 125;
            const int logoMaxW = lw;
            const int logoTop  = DLG_H - PAD - logoH;
            RECT logoRect { lx, logoTop, lx + logoMaxW, logoTop + logoH };

            if (!p->logoImage)
            {
                HRSRC hRes = FindResource (p->hInstance, MAKEINTRESOURCE (IDR_LOGO), RT_RCDATA);
                if (hRes)
                {
                    HGLOBAL hData = LoadResource (p->hInstance, hRes);
                    DWORD   sz    = SizeofResource (p->hInstance, hRes);
                    if (hData && sz > 0)
                    {
                        void* pData = LockResource (hData);
                        HGLOBAL hMem = GlobalAlloc (GMEM_MOVEABLE, sz);
                        if (hMem)
                        {
                            memcpy (GlobalLock (hMem), pData, sz);
                            GlobalUnlock (hMem);
                            IStream* stream = nullptr;
                            if (SUCCEEDED (CreateStreamOnHGlobal (hMem, TRUE, &stream)))
                            {
                                auto* img = new Gdiplus::Image (stream);
                                if (img->GetLastStatus() == Gdiplus::Ok)
                                    p->logoImage = img;
                                else
                                    delete img;
                                stream->Release();
                            }
                            else
                                GlobalFree (hMem);
                        }
                    }
                }
            }

            if (p->logoImage)
            {
                auto* img = reinterpret_cast<Gdiplus::Image*> (p->logoImage);
                UINT imgW = img->GetWidth();
                UINT imgH = img->GetHeight();
                if (imgW > 0 && imgH > 0)
                {
                    int dstH = logoH;
                    int dstW = (int) ((float) imgW / imgH * dstH);
                    if (dstW > logoMaxW) { dstW = logoMaxW; dstH = (int) ((float) imgH / imgW * dstW); }
                    int dstX = lx + (logoMaxW - dstW) / 2;
                    int dstY = logoTop + (logoH - dstH) / 2;

                    Gdiplus::Graphics g (hdc);
                    g.SetInterpolationMode (Gdiplus::InterpolationModeHighQualityBicubic);
                    g.DrawImage (img, dstX, dstY, dstW, dstH);
                }
            }
        }

        EndPaint (hwnd, &ps);
        return 0;
    }

    // ── Timer: browser/deck polling + visibility sync ────────────────────────
    case WM_TIMER:
    {
        if (!p) break;

        // ── Smart search timer (one-shot) ────────────────────────────────────
        if (wParam == TIMER_SMART_SEARCH)
        {
            KillTimer (hwnd, TIMER_SMART_SEARCH);
            p->runSmartSearch();
            return 0;
        }

        // ── Waveform position update (50ms) ──────────────────────────────────
        if (wParam == TIMER_WAVE_UPDATE)
        {
            if (p->prelistenActive && IsWindowVisible (hwnd))
            {
                double pos = p->vdjGetValue ("prelisten_pos");
                if (pos <= 0.0) pos = p->vdjGetValue ("get_prelisten_pos");
                if (pos >= 0.0)
                {
                    double newPos = (pos <= 1.001) ? pos * 100.0 : pos;
                    if (newPos < 0.0)   newPos = 0.0;
                    if (newPos > 100.0) newPos = 100.0;
                    if (!p->prelistenSeeking)
                        p->prelistenPos = newPos;
                    InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
                }
            }
            return 0;
        }

        if (wParam != TIMER_BROWSE_POLL) break;

        // ── Visibility: show only when VDJ process is foreground ──────────────
        if (p->dialogRequestedOpen)
        {
            HWND ownerHwnd    = GetWindow (hwnd, GW_OWNER);
            bool vdjMinimised = ownerHwnd && IsIconic (ownerHwnd);
            bool vdjFg        = isVdjHostForeground();

            if (!vdjMinimised && vdjFg)
            {
                if (!IsWindowVisible (hwnd))
                    ShowWindow (hwnd, SW_SHOWNOACTIVATE);
                SetWindowPos (hwnd, HWND_TOP, 0, 0, 0, 0,
                              SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            else if (IsWindowVisible (hwnd))
            {
                p->suppressNextHideSync = true;
                ShowWindow (hwnd, SW_HIDE);
            }
        }
        else if (IsWindowVisible (hwnd))
        {
            p->suppressNextHideSync = true;
            ShowWindow (hwnd, SW_HIDE);
        }

        // ── Poll song from browser ────────────────────────────────────────────
        std::wstring newTitle  = p->vdjGetString ("get_browsed_song 'title'");
        std::wstring newArtist = p->vdjGetString ("get_browsed_song 'artist'");

        if (!newTitle.empty()
            && (newTitle != p->lastSeenTitle || newArtist != p->lastSeenArtist))
        {
            p->lastSeenTitle  = newTitle;
            p->lastSeenArtist = newArtist;
            if (p->hEditTitle)  SetWindowTextW (p->hEditTitle,  newTitle.c_str());
            if (p->hEditArtist) SetWindowTextW (p->hEditArtist, newArtist.c_str());
            p->runIdentification (newTitle, newArtist);
        }

        // ── Poll prelisten filepath for waveform ──────────────────────────────
        std::wstring browsePath = p->vdjGetString ("get_browsed_filepath");
        if (!browsePath.empty() && browsePath != p->lastSeenBrowsePath)
        {
            p->lastSeenBrowsePath = browsePath;
            rebuildPrelistenWaveBins (p->prelistenWaveBins, browsePath);
            if (p->activeTab == 2)
                InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
        }

        // ── Poll VDJ browser list contents (skip while smart search pending) ──
        // Uses file_count + get_browsed_folder_path + filesystem enumeration,
        // matching TigerTag's proven pattern. get_browser_file does not exist in VDJ API.
        if (!p->smartSearchPending)
        {
            int newCount = (int) p->vdjGetValue ("file_count");
            std::wstring curFolder = p->vdjGetString ("get_browsed_folder_path");

            if (newCount != p->browseListCount || curFolder != p->lastBrowseFolder)
            {
                p->browseListCount  = newCount;
                p->lastBrowseFolder = curFolder;
                p->browseItems.clear();

                // Only enumerate when VDJ is showing a real folder (not search results).
                // When curFolder is empty (search results), leave browseItems alone so
                // smart search results are not overwritten.
                if (!curFolder.empty() && newCount > 0)
                {
                    static const wchar_t* kAudioExts[] = {
                        L".mp3", L".flac", L".wav", L".aiff", L".aif",
                        L".ogg", L".m4a", L".wma", L".aac", nullptr
                    };
                    auto isAudioExt = [] (const fs::path& fp) -> bool {
                        std::wstring ext = fp.extension().wstring();
                        for (wchar_t& c : ext) c = towlower (c);
                        for (int k = 0; kAudioExts[k]; ++k)
                            if (ext == kAudioExts[k]) return true;
                        return false;
                    };

                    try
                    {
                        for (const auto& e : fs::directory_iterator (fs::path (curFolder)))
                        {
                            bool isDir = false;
                            try { isDir = e.is_directory(); } catch (...) {}
                            if (isDir) continue;
                            if (!isAudioExt (e.path())) continue;

                            BrowseItem bi;
                            bi.filePath     = e.path().wstring();
                            bi.title        = e.path().stem().wstring();
                            bi.browserIndex = -1;
                            p->browseItems.push_back (bi);
                        }
                        std::sort (p->browseItems.begin(), p->browseItems.end(),
                                   [] (const BrowseItem& a, const BrowseItem& b) {
                                       return _wcsicmp (a.title.c_str(), b.title.c_str()) < 0;
                                   });
                    }
                    catch (...) {}

                    if (p->hBrowseList)
                    {
                        SendMessageW (p->hBrowseList, LB_RESETCONTENT, 0, 0);
                        for (int i = 0; i < (int) p->browseItems.size(); ++i)
                            SendMessageW (p->hBrowseList, LB_ADDSTRING, 0, (LPARAM) L"");
                    }
                }
            }
        }

        return 0;
    }

    // ── Commands ─────────────────────────────────────────────────────────────
    case WM_COMMAND:
    {
        if (!p) break;
        int ctrlId    = LOWORD (wParam);
        int notifCode = HIWORD (wParam);

        switch (ctrlId)
        {
        // Tab buttons
        case IDC_BTN_TAB_TRACK:
            p->activeTab = 0; p->saveSettings(); applyLayout (p, hwnd); repaintTabs (p); break;
        case IDC_BTN_TAB_MATCHES:
            p->activeTab = 1; p->saveSettings(); applyLayout (p, hwnd); repaintTabs (p); break;
        case IDC_BTN_TAB_BROWSE:
            p->activeTab = 2; p->saveSettings(); applyLayout (p, hwnd); repaintTabs (p); break;
        case IDC_BTN_TAB_SETTINGS:
            p->activeTab = 3; p->saveSettings(); applyLayout (p, hwnd); repaintTabs (p); break;

        // Manual search
        case IDC_BTN_SEARCH:
        {
            wchar_t title[512] = {}, artist[512] = {};
            GetWindowTextW (p->hEditTitle,  title,  512);
            GetWindowTextW (p->hEditArtist, artist, 512);
            p->runIdentification (title, artist);
            break;
        }

        // Candidate selection
        case IDC_CANDIDATES_LIST:
            if (notifCode == LBN_SELCHANGE || notifCode == LBN_DBLCLK)
            {
                int sel = (int) SendMessageW (p->hCandList, LB_GETCURSEL, 0, 0);
                if (sel >= 0)
                {
                    p->confirmCandidate (sel);
                    // Auto-switch to Matches tab
                    if (p->activeTab != 1)
                    {
                        p->activeTab = 1;
                        applyLayout (p, hwnd);
                        repaintTabs (p);
                    }
                }
            }
            break;

        // Result list selection
        case IDC_RESULTS_LIST:
            if (notifCode == LBN_SELCHANGE)
            {
                int sel = (int) SendMessageW (p->hResultsList, LB_GETCURSEL, 0, 0);
                p->selectedResultIdx = sel;
                InvalidateRect (hwnd, nullptr, FALSE);
            }
            break;

        // Filter toggles
        case IDC_CHK_SAME_ARTIST:
            p->filterSameArtist = !p->filterSameArtist;
            if (p->hChkArtist) InvalidateRect (p->hChkArtist, nullptr, FALSE);
            p->saveSettings(); if (p->confirmedIdx >= 0) p->runTandaSearch(); break;
        case IDC_CHK_SAME_SINGER:
            p->filterSameSinger = !p->filterSameSinger;
            if (p->hChkSinger) InvalidateRect (p->hChkSinger, nullptr, FALSE);
            p->saveSettings(); if (p->confirmedIdx >= 0) p->runTandaSearch(); break;
        case IDC_CHK_SAME_GROUPING:
            p->filterSameGrouping = !p->filterSameGrouping;
            if (p->hChkGrouping) InvalidateRect (p->hChkGrouping, nullptr, FALSE);
            p->saveSettings(); if (p->confirmedIdx >= 0) p->runTandaSearch(); break;
        case IDC_CHK_SAME_GENRE:
            p->filterSameGenre = !p->filterSameGenre;
            if (p->hChkGenre) InvalidateRect (p->hChkGenre, nullptr, FALSE);
            p->saveSettings(); if (p->confirmedIdx >= 0) p->runTandaSearch(); break;
        case IDC_CHK_SAME_ORCHESTRA:
            p->filterSameOrchestra = !p->filterSameOrchestra;
            if (p->hChkOrchestra) InvalidateRect (p->hChkOrchestra, nullptr, FALSE);
            p->saveSettings(); if (p->confirmedIdx >= 0) p->runTandaSearch(); break;
        case IDC_CHK_SAME_LABEL:
            p->filterSameLabel = !p->filterSameLabel;
            if (p->hChkLabel) InvalidateRect (p->hChkLabel, nullptr, FALSE);
            p->saveSettings(); if (p->confirmedIdx >= 0) p->runTandaSearch(); break;
        case IDC_CHK_SAME_TRACK:
            p->filterSameTrack = !p->filterSameTrack;
            if (p->hChkTrack) InvalidateRect (p->hChkTrack, nullptr, FALSE);
            p->saveSettings(); if (p->confirmedIdx >= 0) p->runTandaSearch(); break;

        // Search VDJ browser (with diacritic normalization) then switch to Browse
        case IDC_BTN_SEARCH_VDJ:
        {
            int sel = (int) SendMessageW (p->hResultsList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int) p->results.size())
            {
                const TgRecord& rec = p->results[sel];
                std::wstring query = normalizeForSearch (rec.title);
                if (!rec.bandleader.empty()) query += L" " + normalizeForSearch (rec.bandleader);
                std::string cmd = "search \"" + toUtf8 (query) + "\"";

                // Switch to songs window first so search targets the right list,
                // then issue the search command
                p->vdjSend ("browser_window 'songs'");
                p->vdjSend (cmd);

                // Store target for smart search scoring
                p->searchTargetTitle  = rec.title;
                p->searchTargetArtist = rec.bandleader;
                p->searchTargetYear   = rec.year;
                p->smartSearchPending = true;

                // Fire smart search after 500ms to let VDJ populate results
                SetTimer (hwnd, TIMER_SMART_SEARCH, 500, nullptr);

                // Switch to Browse tab so user sees results
                p->activeTab = 2;
                p->saveSettings();
                applyLayout (p, hwnd);
                repaintTabs (p);
            }
            break;
        }

        // Prelisten toggle
        case IDC_BTN_PRELISTEN:
            p->prelistenActive = !p->prelistenActive;
            p->vdjSend (p->prelistenActive ? "prelisten" : "prelisten_stop");
            if (p->prelistenActive)
                SetTimer (hwnd, TIMER_WAVE_UPDATE, 50, nullptr);
            else
            {
                KillTimer (hwnd, TIMER_WAVE_UPDATE);
                p->prelistenPos = 0.0;
                p->prelistenSeeking = false;
            }
            SetWindowTextW (p->hBtnPrelisten, p->prelistenActive ? L"\u23F8" : L"\u25B6");
            InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
            break;

        // Browse list: clicking a row moves VDJ browser focus to that item
        case IDC_BROWSE_LIST:
            if (notifCode == LBN_SELCHANGE || notifCode == LBN_DBLCLK)
            {
                int sel = (int) SendMessageW (p->hBrowseList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int) p->browseItems.size())
                {
                    const BrowseItem& bi = p->browseItems[sel];

                    // Use browserIndex for smart search results, otherwise listbox index
                    int vdjIdx = (bi.browserIndex >= 0) ? bi.browserIndex : sel;
                    p->vdjSend ("browser_scroll 'top'");
                    Sleep (20);
                    if (vdjIdx > 0)
                        p->vdjSend ("browser_scroll +" + std::to_string (vdjIdx));

                    // Update local prelisten waveform if path available
                    if (!bi.filePath.empty())
                    {
                        rebuildPrelistenWaveBins (p->prelistenWaveBins, bi.filePath);
                        p->prelistenWavePath = bi.filePath;
                        p->prelistenPos = 0.0;
                        InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
                    }
                }
            }
            break;

        // Browse: Add to end of playlist
        case IDC_BTN_ADD_END:
            p->vdjSend ("playlist_add");
            break;

        // Settings: toggle year range on/off (color-only feedback — no text change)
        case IDC_BTN_YEAR_TOGGLE:
            p->filterUseYearRange = !p->filterUseYearRange;
            if (p->hBtnYearToggle)  InvalidateRect (p->hBtnYearToggle,  nullptr, FALSE);
            if (p->hBtnYearRange) InvalidateRect (p->hBtnYearRange, nullptr, FALSE);
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;

        // Settings: year range button (cycles through values)
        case IDC_BTN_YEAR_RANGE:
        {
            static const int      kYrVals[]   = { 0, 1, 2, 3, 5, 10 };
            static const wchar_t* kYrLabels[] = { L"\u00B10", L"\u00B11", L"\u00B12", L"\u00B13", L"\u00B15", L"\u00B110" };
            int curIdx = 0;
            for (int i = 0; i < 6; ++i)
                if (kYrVals[i] == p->yearRange) { curIdx = i; break; }
            curIdx = (curIdx + 1) % 6;
            p->yearRange = kYrVals[curIdx];
            if (p->hBtnYearRange) SetWindowTextW (p->hBtnYearRange, kYrLabels[curIdx]);
            if (p->hBtnYearRange) InvalidateRect (p->hBtnYearRange, nullptr, FALSE);
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        }

        // Settings: "How it works" sub-tab selection
        case IDC_BTN_HOW_TAB_0: case IDC_BTN_HOW_TAB_1: case IDC_BTN_HOW_TAB_2:
        case IDC_BTN_HOW_TAB_3: case IDC_BTN_HOW_TAB_4:
            p->activeHowTab = ctrlId - IDC_BTN_HOW_TAB_0;
            for (int i = 0; i < 5; ++i)
                if (p->hBtnHowTabs[i]) InvalidateRect (p->hBtnHowTabs[i], nullptr, FALSE);
            InvalidateRect (hwnd, nullptr, FALSE);
            break;

        // Close
        case IDC_BTN_CLOSE:
            p->dialogRequestedOpen  = false;
            p->suppressNextHideSync = true;
            p->vdjSend ("effect_show_gui off");
            ShowWindow (hwnd, SW_HIDE);
            break;
        }
        return 0;
    }

    // ── Owner-draw buttons ───────────────────────────────────────────────────
    case WM_DRAWITEM:
    {
        auto* di = reinterpret_cast<DRAWITEMSTRUCT*> (lParam);
        if (!di || !p) break;

        if (di->CtlType == ODT_BUTTON)
        {
            // Tab buttons — use fontNormal for inactive state (increased from fontSmall)
            if (di->CtlID == IDC_BTN_TAB_TRACK)
            {
                drawTextToggle (di, L"Track",   p->activeTab == 0, p->fontNormal, p->fontBold, TCol::panel, p->hoveredBtn == di->hwndItem);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_MATCHES)
            {
                drawTextToggle (di, L"Matches", p->activeTab == 1, p->fontNormal, p->fontBold, TCol::panel, p->hoveredBtn == di->hwndItem);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_BROWSE)
            {
                drawTextToggle (di, L"Browse",  p->activeTab == 2, p->fontNormal, p->fontBold, TCol::panel, p->hoveredBtn == di->hwndItem);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_SETTINGS)
            {
                drawTextToggle (di, L"\u26ED",  p->activeTab == 3, p->fontNormal, p->fontBold, TCol::panel, p->hoveredBtn == di->hwndItem);
                return TRUE;
            }

            // Magnifying glass search button
            if (di->CtlID == IDC_BTN_SEARCH)
            {
                COLORREF bg = RGB (35, 50, 70);
                bool pressed = (di->itemState & ODS_SELECTED) != 0;
                bool hovered = (di->itemState & ODS_HOTLIGHT) != 0;
                RECT r = di->rcItem;
                COLORREF fillBg = pressed ? TCol::buttonHover
                                : hovered ? RGB (48, 65, 88)
                                          : bg;

                HRGN rgn2 = CreateRoundRectRgn (r.left, r.top, r.right, r.bottom, 6, 6);
                SelectClipRgn (di->hDC, rgn2);
                fillRect (di->hDC, r, fillBg);
                SelectClipRgn (di->hDC, nullptr);
                DeleteObject (rgn2);

                HPEN pen    = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldPen = (HPEN) SelectObject (di->hDC, pen);
                HBRUSH oldBr2 = (HBRUSH) SelectObject (di->hDC, GetStockObject (NULL_BRUSH));
                RoundRect (di->hDC, r.left, r.top, r.right, r.bottom, 6, 6);
                SelectObject (di->hDC, oldPen);
                SelectObject (di->hDC, oldBr2);
                DeleteObject (pen);

                int cx = (di->rcItem.left + di->rcItem.right)  / 2;
                int cy = (di->rcItem.top  + di->rcItem.bottom) / 2;
                HPEN gp  = CreatePen (PS_SOLID, 2, TCol::textBright);
                HPEN old = (HPEN) SelectObject (di->hDC, gp);
                HBRUSH obr = (HBRUSH) SelectObject (di->hDC, GetStockObject (NULL_BRUSH));
                Ellipse (di->hDC, cx - 7, cy - 7, cx + 3, cy + 3);
                MoveToEx (di->hDC, cx + 1, cy + 1, nullptr);
                LineTo   (di->hDC, cx + 5, cy + 5);
                SelectObject (di->hDC, old);
                SelectObject (di->hDC, obr);
                DeleteObject (gp);
                return TRUE;
            }

            // Prelisten play/pause button
            if (di->CtlID == IDC_BTN_PRELISTEN)
            {
                COLORREF bg  = p->prelistenActive ? TCol::accent    : TCol::buttonBg;
                COLORREF fg  = p->prelistenActive ? TCol::textBright : TCol::accentBrt;
                wchar_t txt[4] = {};
                GetWindowTextW (di->hwndItem, txt, 4);
                drawOwnerButton (di, txt, bg, fg, p->fontNormal, p->hoveredBtn == di->hwndItem);
                return TRUE;
            }

            // Filter toggle buttons
            if (di->CtlID == IDC_CHK_SAME_ARTIST    || di->CtlID == IDC_CHK_SAME_SINGER   ||
                di->CtlID == IDC_CHK_SAME_GROUPING  || di->CtlID == IDC_CHK_SAME_GENRE    ||
                di->CtlID == IDC_CHK_SAME_ORCHESTRA  || di->CtlID == IDC_CHK_SAME_LABEL   ||
                di->CtlID == IDC_CHK_SAME_TRACK)
            {
                bool isOn = false;
                switch (di->CtlID)
                {
                    case IDC_CHK_SAME_ARTIST:    isOn = p->filterSameArtist;    break;
                    case IDC_CHK_SAME_SINGER:    isOn = p->filterSameSinger;    break;
                    case IDC_CHK_SAME_GROUPING:  isOn = p->filterSameGrouping;  break;
                    case IDC_CHK_SAME_GENRE:     isOn = p->filterSameGenre;     break;
                    case IDC_CHK_SAME_ORCHESTRA: isOn = p->filterSameOrchestra; break;
                    case IDC_CHK_SAME_LABEL:     isOn = p->filterSameLabel;     break;
                    case IDC_CHK_SAME_TRACK:     isOn = p->filterSameTrack;     break;
                }
                wchar_t ftxt[64] = {};
                GetWindowTextW (di->hwndItem, ftxt, 64);
                // Darker orange when active (was TCol::accent)
                COLORREF fbg = isOn ? RGB (160, 75, 20) : TCol::buttonBg;
                COLORREF ffg = isOn ? TCol::textBright   : TCol::textDim;
                drawOwnerButton (di, ftxt, fbg, ffg, p->fontSmall, p->hoveredBtn == di->hwndItem);
                return TRUE;
            }

            // "How it works" sub-tab buttons
            if (di->CtlID >= IDC_BTN_HOW_TAB_0 && di->CtlID <= IDC_BTN_HOW_TAB_4)
            {
                int idx = di->CtlID - IDC_BTN_HOW_TAB_0;
                static const wchar_t* kNames[] = { L"Overview", L"Track", L"Matches", L"Browser", L"Filters" };
                drawTextToggle (di, kNames[idx], p->activeHowTab == idx,
                                p->fontSmall, p->fontSmall, TCol::bg, p->hoveredBtn == di->hwndItem);
                return TRUE;
            }

            // Generic buttons
            wchar_t text[128] = {};
            GetWindowTextW (di->hwndItem, text, 128);
            std::wstring label (text);

            bool btnHovered = (di->itemState & ODS_HOTLIGHT) != 0 || p->hoveredBtn == di->hwndItem;
            COLORREF bg = TCol::buttonBg, fg = TCol::textNormal;
            if      (di->CtlID == IDC_BTN_SEARCH_VDJ)   { bg = RGB (35, 50, 70);  fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_CLOSE)
            {
                bg = btnHovered ? RGB (200, 45, 45) : RGB (70, 28, 28);
                fg = TCol::textBright;
            }
            else if (di->CtlID == IDC_BTN_ADD_END)       { bg = RGB (28, 55, 28);  fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_YEAR_TOGGLE)
            {
                bg = p->filterUseYearRange ? RGB (160, 75, 20) : TCol::buttonBg;
                fg = p->filterUseYearRange ? TCol::textBright   : TCol::textDim;
            }
            else if (di->CtlID == IDC_BTN_YEAR_RANGE)
            {
                bg = p->filterUseYearRange ? RGB (160, 75, 20) : TCol::buttonBg;
                fg = p->filterUseYearRange ? TCol::textBright   : TCol::textDim;
            }

            // Pass hover state (close button handles its own color above; others use drawOwnerButton's lighten)
            bool passHover = (di->CtlID != IDC_BTN_CLOSE) && btnHovered;
            HFONT btnFont = (di->CtlID == IDC_BTN_YEAR_TOGGLE || di->CtlID == IDC_BTN_YEAR_RANGE)
                            ? p->fontSmall : p->fontNormal;
            drawOwnerButton (di, label, bg, fg, btnFont, passHover);
            return TRUE;
        }

        // ── Candidates list ───────────────────────────────────────────────────
        if (di->CtlID == IDC_CANDIDATES_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->candidates.size()) break;
            const TgMatchResult& mr  = p->candidates[di->itemID];
            const TgRecord&      rec = mr.record;

            HDC  hdc = di->hDC;
            RECT r   = di->rcItem;
            bool sel       = (di->itemState & ODS_SELECTED) != 0;
            bool confirmed = ((int) di->itemID == p->confirmedIdx);

            fillRect (hdc, r, (sel || confirmed) ? TCol::matchSel : TCol::panel);

            int dotCx = r.left + 10;
            int dotCy = r.top + CAND_ITEM_H / 2;
            drawScoreDot (hdc, dotCx, dotCy, mr.score);

            int tx = r.left + 22;
            std::wstring row1 = rec.title;
            if (!rec.bandleader.empty()) row1 += L"  \u00B7  " + rec.bandleader;
            RECT titleR { tx, r.top + 4, r.right - 4, r.top + 4 + 18 };
            drawText (hdc, titleR, row1, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

            std::wstring row2;
            if (!rec.singer.empty()) row2 += rec.singer;
            if (!rec.year.empty())   { if (!row2.empty()) row2 += L"  \u00B7  "; row2 += rec.year; }
            RECT detailR { tx, r.top + 4 + 18 + 2, r.right - 4, r.bottom - 2 };
            drawText (hdc, detailR, row2, TCol::textDim, p->fontDetail,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

            if (confirmed)
            {
                RECT ck { r.right - 18, r.top, r.right, r.bottom };
                drawText (hdc, ck, L"\u2714", TCol::good, p->fontBold,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, pen);
            MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
            LineTo   (hdc, r.right, r.bottom - 1);
            SelectObject (hdc, old);
            DeleteObject (pen);
            return TRUE;
        }

        // ── Results list ──────────────────────────────────────────────────────
        if (di->CtlID == IDC_RESULTS_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->results.size()) break;
            const TgRecord& rec = p->results[di->itemID];

            HDC  hdc  = di->hDC;
            RECT r    = di->rcItem;
            bool sel  = (di->itemState & ODS_SELECTED) != 0;
            bool even = (di->itemID % 2 == 0);

            fillRect (hdc, r, sel ? TCol::matchSel : even ? TCol::card : TCol::panel);

            int tx = r.left + 5;
            RECT titleR { tx, r.top, r.right - 46, r.bottom };
            drawText (hdc, titleR, rec.title, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Show year only (first 4 chars of date, or rec.year directly)
            std::wstring yearStr = rec.year.empty()
                ? (rec.date.size() >= 4 ? rec.date.substr (0, 4) : rec.date)
                : rec.year;
            if (!yearStr.empty())
            {
                RECT dateR { r.right - 44, r.top, r.right - 4, r.bottom };
                drawText (hdc, dateR, yearStr, TCol::textDim, p->fontNormal,
                          DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }

            HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, pen);
            MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
            LineTo   (hdc, r.right, r.bottom - 1);
            SelectObject (hdc, old);
            DeleteObject (pen);
            return TRUE;
        }

        // ── Browse list (Title | Artist | Year columns) ───────────────────────
        if (di->CtlID == IDC_BROWSE_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->browseItems.size()) break;
            const BrowseItem& bi = p->browseItems[di->itemID];

            HDC  hdc  = di->hDC;
            RECT r    = di->rcItem;
            bool sel  = (di->itemState & ODS_SELECTED) != 0;
            bool even = (di->itemID % 2 == 0);

            fillRect (hdc, r, sel ? TCol::matchSel : even ? TCol::card : TCol::panel);

            const int rw      = r.right - r.left;
            const int yearW   = 36;
            const int artistW = rw * 40 / 100;
            const int titleW  = rw - artistW - yearW - 6;
            const int tx      = r.left + 4;

            RECT titleR  { tx,                        r.top, tx + titleW,                     r.bottom };
            RECT artistR { tx + titleW + 2,            r.top, tx + titleW + 2 + artistW,        r.bottom };
            RECT yearR   { r.right - yearW - 2,        r.top, r.right - 2,                      r.bottom };

            drawText (hdc, titleR,  bi.title,  TCol::textBright, p->fontBold,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, artistR, bi.artist, TCol::textDim,    p->fontSmall,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, yearR,   bi.year,   TCol::textDim,    p->fontSmall,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, pen);
            MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
            LineTo   (hdc, r.right, r.bottom - 1);
            SelectObject (hdc, old);
            DeleteObject (pen);
            return TRUE;
        }

        break;
    }

    // ── Measure items ────────────────────────────────────────────────────────
    case WM_MEASUREITEM:
    {
        auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*> (lParam);
        if (!mi) break;
        if (mi->CtlID == IDC_CANDIDATES_LIST) { mi->itemHeight = CAND_ITEM_H;    return TRUE; }
        if (mi->CtlID == IDC_RESULTS_LIST)    { mi->itemHeight = RESULT_ITEM_H;  return TRUE; }
        if (mi->CtlID == IDC_BROWSE_LIST)     { mi->itemHeight = BROWSE_ITEM_H;  return TRUE; }
        break;
    }

    // ── Control colours ──────────────────────────────────────────────────────
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC) wParam;
        SetBkColor   (hdc, TCol::panel);
        SetTextColor (hdc, TCol::textNormal);
        return (LRESULT) (p ? p->panelBrush : GetStockObject (NULL_BRUSH));
    }
    case WM_CTLCOLOREDIT:
    {
        HDC hdc  = (HDC) wParam;
        HWND hed = (HWND) lParam;
        // Muted background for search input boxes (less harsh than pure black)
        if (p && (hed == p->hEditTitle || hed == p->hEditArtist))
        {
            SetBkColor   (hdc, RGB (32, 36, 52));
            SetTextColor (hdc, TCol::textNormal);
            return (LRESULT) p->searchBoxBrush;
        }
        SetBkColor   (hdc, TCol::card);
        SetTextColor (hdc, TCol::textBright);
        return (LRESULT) (p ? p->cardBrush : GetStockObject (NULL_BRUSH));
    }
    case 0x0109:  // WM_CTLCOLORCOMBOBOX
    {
        HDC hdc = (HDC) wParam;
        SetBkColor   (hdc, TCol::card);
        SetTextColor (hdc, TCol::textBright);
        return (LRESULT) (p ? p->cardBrush : GetStockObject (NULL_BRUSH));
    }
    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = (HDC) wParam;
        SetBkColor   (hdc, TCol::panel);
        SetTextColor (hdc, TCol::textNormal);
        return (LRESULT) (p ? p->panelBrush : GetStockObject (NULL_BRUSH));
    }

    // ── Prelisten waveform click-to-seek ────────────────────────────────────
    case WM_LBUTTONDOWN:
    {
        if (!p) break;
        POINT pt = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
        if (p->prelistenActive && PtInRect (&p->prelistenWaveRect, pt))
        {
            int w = p->prelistenWaveRect.right - p->prelistenWaveRect.left;
            if (w > 0)
            {
                int x = pt.x - p->prelistenWaveRect.left;
                if (x < 0) x = 0;
                if (x > w) x = w;
                p->prelistenPos = (x * 100.0) / w;
                p->prelistenSeeking = true;
                SetCapture (hwnd);
                p->vdjSend ("prelisten_pos " + std::to_string ((int) p->prelistenPos) + "%");
                InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
            }
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (!p || !p->prelistenSeeking || !(wParam & MK_LBUTTON)) break;
        POINT pt = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
        int w = p->prelistenWaveRect.right - p->prelistenWaveRect.left;
        if (w > 0)
        {
            int x = pt.x - p->prelistenWaveRect.left;
            if (x < 0) x = 0;
            if (x > w) x = w;
            p->prelistenPos = (x * 100.0) / w;
            p->vdjSend ("prelisten_pos " + std::to_string ((int) p->prelistenPos) + "%");
            InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (p && p->prelistenSeeking)
        {
            p->prelistenSeeking = false;
            ReleaseCapture();
        }
        break;
    }

    case WM_CAPTURECHANGED:
    {
        if (p) p->prelistenSeeking = false;
        break;
    }

    case WM_DESTROY:
        KillTimer (hwnd, TIMER_BROWSE_POLL);
        KillTimer (hwnd, TIMER_SMART_SEARCH);
        KillTimer (hwnd, TIMER_WAVE_UPDATE);
        if (p && p->hTooltip) { DestroyWindow (p->hTooltip); p->hTooltip = nullptr; }
        return 0;
    }

    return DefWindowProcW (hwnd, msg, wParam, lParam);
}

