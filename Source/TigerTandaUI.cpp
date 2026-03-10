//==============================================================================
// TigerTanda VDJ Plugin - Main Window UI
// TandaWndProc: WM_CREATE, WM_PAINT, WM_TIMER, WM_COMMAND, WM_DRAWITEM
//==============================================================================

#include "TigerTanda.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <cmath>

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
                             COLORREF bgColor, COLORREF fgColor, HFONT font)
{
    HDC hdc = di->hDC;
    RECT r = di->rcItem;
    bool pressed  = (di->itemState & ODS_SELECTED) != 0;
    bool disabled = (di->itemState & ODS_DISABLED) != 0;

    COLORREF bg = pressed  ? TCol::buttonHover
                : disabled ? RGB (24, 28, 42)
                           : bgColor;

    // Rounded fill via clip region
    HRGN rgn = CreateRoundRectRgn (r.left, r.top, r.right, r.bottom, 6, 6);
    SelectClipRgn (hdc, rgn);
    fillRect (hdc, r, bg);
    SelectClipRgn (hdc, nullptr);
    DeleteObject (rgn);

    // Rounded border outline only (null brush)
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

// Draw a flat text-only toggle button — orange+bold when active, gray+normal otherwise
// Matches the Hisstory Analyzer/Spectrogram toggle style: no background fill change, just text
static void drawTextToggle (const DRAWITEMSTRUCT* di, const std::wstring& label,
                            bool isActive, HFONT fontNormal, HFONT fontBold,
                            COLORREF bgColor = TCol::panel)
{
    HDC hdc = di->hDC;
    RECT r = di->rcItem;
    bool pressed = (di->itemState & ODS_SELECTED) != 0;

    fillRect (hdc, r, bgColor);

    COLORREF fg = (isActive || pressed) ? TCol::accentBrt : TCol::textDim;
    HFONT font  = (isActive || pressed) ? fontBold : fontNormal;
    drawText (hdc, r, label, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Accent bottom line when active
    HPEN pen = CreatePen (PS_SOLID, isActive ? 2 : 1,
                          isActive ? TCol::accent : TCol::cardBorder);
    HPEN old = (HPEN) SelectObject (hdc, pen);
    MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
    LineTo   (hdc, r.right, r.bottom - 1);
    SelectObject (hdc, old);
    DeleteObject (pen);
}

// Draw collapse/expand arrow glyph (ported from hisstory-vst)
static void drawArrowGlyph (HDC hdc, RECT r, bool isExpanded)
{
    int cx = (r.left + r.right)  / 2;
    int cy = (r.top  + r.bottom) / 2;
    int s  = 4;  // half-size of the arrow pattern

    HPEN pen = CreatePen (PS_SOLID, 2, TCol::accentBrt);
    HPEN old = (HPEN) SelectObject (hdc, pen);

    auto drawArrow = [&](int x0, int y0, int x1, int y1)
    {
        MoveToEx (hdc, x0, y0, nullptr);
        LineTo   (hdc, x1, y1);
        // arrowhead
        float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
        float len = sqrtf (dx * dx + dy * dy);
        if (len > 0.001f)
        {
            float nx = dx / len, ny = dy / len;
            float tx = -ny, ty = nx;
            int h = 3;
            MoveToEx (hdc, x1, y1, nullptr);
            LineTo   (hdc, x1 - (int)(nx * h + tx * h * 0.75f),
                           y1 - (int)(ny * h + ty * h * 0.75f));
            MoveToEx (hdc, x1, y1, nullptr);
            LineTo   (hdc, x1 - (int)(nx * h - tx * h * 0.75f),
                           y1 - (int)(ny * h - ty * h * 0.75f));
        }
    };

    if (isExpanded)
    {
        // Collapse icon: two diagonal arrows pointing inwards
        drawArrow (cx - s - 3, cy + s + 3, cx - 1, cy + 1);
        drawArrow (cx + s + 3, cy - s - 3, cx + 1, cy - 1);
    }
    else
    {
        // Expand icon: two diagonal arrows pointing outwards
        drawArrow (cx - 1, cy + 1, cx - s - 3, cy + s + 3);
        drawArrow (cx + 1, cy - 1, cx + s + 3, cy - s - 3);
    }

    SelectObject (hdc, old);
    DeleteObject (pen);
}

// Draw a small score dot (green/yellow/red) replacing the % badge
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

// Draw the metadata detail box for selected result
static void drawResultDetailBox (HDC hdc, RECT r, const TgRecord& rec, HFONT fontSmall, HFONT fontBold)
{
    fillRect (hdc, r, TCol::card);

    // Border
    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN old = (HPEN) SelectObject (hdc, pen);
    MoveToEx (hdc, r.left,  r.top, nullptr);
    LineTo   (hdc, r.right, r.top);
    SelectObject (hdc, old);
    DeleteObject (pen);

    int px = r.left + 6;
    int py = r.top + 5;
    const int lineH1 = 14;
    const int lineH2 = 13;

    // Row 1: Bandleader (bright, bold)
    RECT r1 { px, py, r.right - 6, py + lineH1 };
    drawText (hdc, r1, rec.bandleader, TCol::textBright, fontBold,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    py += lineH1 + 2;

    // Row 2: Singer  ·  Date
    std::wstring line2;
    if (!rec.singer.empty()) line2 += rec.singer;
    if (!rec.year.empty()) { if (!line2.empty()) line2 += L"  \u00B7  "; line2 += rec.year; }
    RECT r2 { px, py, r.right - 6, py + lineH2 };
    drawText (hdc, r2, line2, TCol::textNormal, fontSmall,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    py += lineH2 + 2;

    // Row 3: Genre  ·  Label
    std::wstring line3;
    if (!rec.genre.empty()) line3 += rec.genre;
    if (!rec.label.empty()) { if (!line3.empty()) line3 += L"  \u00B7  "; line3 += rec.label; }
    RECT r3 { px, py, r.right - 6, py + lineH2 };
    drawText (hdc, r3, line3, TCol::textDim, fontSmall,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
}

// ─────────────────────────────────────────────────────────────────────────────
//  applyLayout – reposition + show/hide all controls for the current mode/tab
// ─────────────────────────────────────────────────────────────────────────────

static void applyLayout (TigerTandaPlugin* p, HWND hwnd)
{
    if (!p || !hwnd) return;

    const int cw = (p->viewMode == 0) ? DLG_W_WIDE : DLG_W_COMPACT;

    // Resize the window
    SetWindowPos (hwnd, nullptr, 0, 0, cw, DLG_H,
                  SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // ── Top-bar buttons (right-anchored) ────────────────────────────────────
    const int topY = (TOP_H - BTN_H) / 2;
    // Close [x]
    MoveWindow (p->hBtnClose, cw - 28, topY, 22, BTN_H, FALSE);
    ShowWindow (p->hBtnClose, SW_SHOW);  // always visible in both modes
    // Layout toggle (arrow glyph button — small square)
    MoveWindow (p->hBtnLayoutToggle, cw - 28 - 4 - 26, topY, 26, BTN_H, FALSE);

    // ── Tab strip (compact only) — lives in top bar alongside Close/Toggle ──
    if (p->viewMode == 1)
    {
        // Tabs occupy top bar left of the layout-toggle button
        const int rightBtnsW = 26 + 4 + 22 + PAD;  // toggle + gap + close + right-pad
        const int tabAreaW   = cw - rightBtnsW;
        const int tabW       = tabAreaW / 3;
        MoveWindow (p->hBtnTabIdentify, 0,       topY, tabW,              BTN_H, FALSE);
        MoveWindow (p->hBtnTabResults,  tabW,    topY, tabW,              BTN_H, FALSE);
        MoveWindow (p->hBtnTabSettings, tabW*2,  topY, tabAreaW - tabW*2, BTN_H, FALSE);
        ShowWindow (p->hBtnTabIdentify,  SW_SHOW);
        ShowWindow (p->hBtnTabResults,   SW_SHOW);
        ShowWindow (p->hBtnTabSettings,  SW_SHOW);
    }
    else
    {
        ShowWindow (p->hBtnTabIdentify,  SW_HIDE);
        ShowWindow (p->hBtnTabResults,   SW_HIDE);
        ShowWindow (p->hBtnTabSettings,  SW_HIDE);
    }

    // ── Helper lambdas ───────────────────────────────────────────────────────
    auto showCtrl = [](HWND h, bool vis)
    {
        if (h) ShowWindow (h, vis ? SW_SHOW : SW_HIDE);
    };

    // ── WIDE MODE ────────────────────────────────────────────────────────────
    if (p->viewMode == 0)
    {
        // Filter toggle buttons in top bar (left side), same row as Close/LayoutToggle
        {
            const int ckW = 60;
            int fx = PAD;
            MoveWindow (p->hChkArtist,    fx, topY, ckW, BTN_H, FALSE); fx += ckW;
            MoveWindow (p->hChkSinger,    fx, topY, ckW, BTN_H, FALSE); fx += ckW;
            MoveWindow (p->hChkGrouping,  fx, topY, ckW, BTN_H, FALSE); fx += ckW;
            MoveWindow (p->hChkGenre,     fx, topY, ckW, BTN_H, FALSE); fx += ckW;
            MoveWindow (p->hChkOrchestra, fx, topY, ckW, BTN_H, FALSE); fx += ckW;
            MoveWindow (p->hChkLabel,     fx, topY, ckW, BTN_H, FALSE); fx += ckW + 4;
            // yr± edit + spin (vertically centred in top bar)
            const int editY = (TOP_H - 18) / 2;
            MoveWindow (p->hEditYearRange, fx + 22, editY, 30, 18, FALSE);
            MoveWindow (p->hSpinYear,      fx + 52, editY, 14, 18, FALSE);
        }
        showCtrl (p->hChkArtist,     true);
        showCtrl (p->hChkSinger,     true);
        showCtrl (p->hChkGrouping,   true);
        showCtrl (p->hChkGenre,      true);
        showCtrl (p->hChkOrchestra,  true);
        showCtrl (p->hChkLabel,      true);
        showCtrl (p->hEditYearRange, true);
        showCtrl (p->hSpinYear,      true);

        const int lx = PAD;
        const int lw = LEFT_W - PAD;
        int ly = TOP_H + PAD;

        // "IDENTIFY SONG" label painted in WM_PAINT — skip 14+4
        ly += 14 + 4;

        // 2-part source toggle: [Browser | Deck]
        {
            const int segW = lw / 2;
            MoveWindow (p->hBtnSrcBrowser, lx,        ly, segW,      BTN_H, FALSE);
            MoveWindow (p->hBtnSrcDeck,    lx + segW, ly, lw - segW, BTN_H, FALSE);
        }
        ly += BTN_H + 6;

        // Search row — edit heights match BTN_H
        {
            const int sbW = BTN_H;
            const int gap = 4;
            const int etW = lw - sbW - gap;
            const int tW  = etW * 55 / 100;
            const int aW  = etW - tW - gap;
            MoveWindow (p->hEditTitle,  lx,              ly, tW,  EDIT_H, FALSE);
            MoveWindow (p->hEditArtist, lx + tW + gap,   ly, aW,  EDIT_H, FALSE);
            MoveWindow (p->hBtnSearch,  lx + lw - sbW,   ly, sbW, BTN_H,  FALSE);
        }
        ly += EDIT_H + 6;

        // "CANDIDATES" label painted in WM_PAINT — skip 14+4
        ly += 14 + 4;

        // Candidates list — fills to bottom
        MoveWindow (p->hCandList, lx, ly, lw, DLG_H - ly - PAD, FALSE);

        showCtrl (p->hBtnSrcBrowser, true);
        showCtrl (p->hBtnSrcDeck,    true);
        showCtrl (p->hEditTitle,     true);
        showCtrl (p->hEditArtist,    true);
        showCtrl (p->hBtnSearch,     true);
        showCtrl (p->hCandList,      true);

        // Right panel
        const int rx = RIGHT_X;
        const int rw = DLG_W_WIDE - rx - PAD;
        int ry = TOP_H + PAD + 14 + 4; // below "SIMILAR SONGS" label

        // "TigerTanda" label + Search VDJ button at bottom
        const int bottomRow = DLG_H - PAD - BTN_H;
        const int vdjBtnW = 120;
        // TigerTanda label is painted in WM_PAINT at (rx, bottomRow)
        MoveWindow (p->hBtnSearchVdj, rx + rw - vdjBtnW, bottomRow, vdjBtnW, BTN_H, FALSE);

        // Detail box above the bottom row
        const int detailTop = bottomRow - DETAIL_BOX_H - 4;
        // Results list
        MoveWindow (p->hResultsList, rx, ry, rw, detailTop - ry - 2, FALSE);
        // Detail box is painted in WM_PAINT

        showCtrl (p->hResultsList,  true);
        showCtrl (p->hBtnSearchVdj, true);
    }
    // ── COMPACT MODE ─────────────────────────────────────────────────────────
    else
    {
        const int lx     = PAD;
        const int lw     = DLG_W_COMPACT - PAD * 2;
        const int bodyY  = TOP_H;  // tabs now live in top bar, no extra strip
        const bool showI = (p->activeTab == 0);
        const bool showR = (p->activeTab == 1);
        const bool showS = (p->activeTab == 2);

        // ── Identify tab ─────────────────────────────────────────────────────
        if (showI)
        {
            int ly = bodyY + PAD;

            const int segW = lw / 2;
            MoveWindow (p->hBtnSrcBrowser, lx,        ly, segW,      BTN_H, FALSE);
            MoveWindow (p->hBtnSrcDeck,    lx + segW, ly, lw - segW, BTN_H, FALSE);
            ly += BTN_H + 6;

            const int sbW = BTN_H;
            const int gap = 4;
            const int etW = lw - sbW - gap;
            const int tW  = etW * 55 / 100;
            const int aW  = etW - tW - gap;
            MoveWindow (p->hEditTitle,  lx,            ly, tW,  EDIT_H, FALSE);
            MoveWindow (p->hEditArtist, lx + tW + gap, ly, aW,  EDIT_H, FALSE);
            MoveWindow (p->hBtnSearch,  lx + lw - sbW, ly, sbW, BTN_H,  FALSE);
            ly += EDIT_H + 6;

            MoveWindow (p->hCandList, lx, ly, lw, DLG_H - ly - PAD, FALSE);
        }
        showCtrl (p->hBtnSrcBrowser, showI);
        showCtrl (p->hBtnSrcDeck,    showI);
        showCtrl (p->hEditTitle,     showI);
        showCtrl (p->hEditArtist,    showI);
        showCtrl (p->hBtnSearch,     showI);
        showCtrl (p->hCandList,      showI);

        // ── Results tab ──────────────────────────────────────────────────────
        if (showR)
        {
            int ry = bodyY + PAD;
            const int bottomRow = DLG_H - PAD - BTN_H;
            const int vdjBtnW = 100;
            MoveWindow (p->hBtnSearchVdj, lx + lw - vdjBtnW, bottomRow, vdjBtnW, BTN_H, FALSE);

            const int detailTop = bottomRow - DETAIL_BOX_H - 4;
            MoveWindow (p->hResultsList, lx, ry, lw, detailTop - ry - 2, FALSE);
        }
        showCtrl (p->hResultsList,  showR);
        showCtrl (p->hBtnSearchVdj, showR);

        // ── Settings tab ─────────────────────────────────────────────────────
        if (showS)
        {
            int sy = bodyY + PAD;
            const int colW = lw / 3;
            MoveWindow (p->hChkArtist,    lx,            sy, colW,          BTN_H, FALSE);
            MoveWindow (p->hChkSinger,    lx + colW,     sy, colW,          BTN_H, FALSE);
            MoveWindow (p->hChkGrouping,  lx + colW * 2, sy, lw - colW * 2, BTN_H, FALSE);
            sy += BTN_H + 6;
            MoveWindow (p->hChkGenre,     lx,            sy, colW,          BTN_H, FALSE);
            MoveWindow (p->hChkOrchestra, lx + colW,     sy, colW,          BTN_H, FALSE);
            MoveWindow (p->hChkLabel,     lx + colW * 2, sy, lw - colW * 2, BTN_H, FALSE);
            sy += BTN_H + 8;
            MoveWindow (p->hEditYearRange, lx + 28,      sy, 38, EDIT_H, FALSE);
            MoveWindow (p->hSpinYear,      lx + 28 + 38, sy, 16, EDIT_H, FALSE);
        }
        showCtrl (p->hChkArtist,    showS);
        showCtrl (p->hChkSinger,    showS);
        showCtrl (p->hChkGrouping,  showS);
        showCtrl (p->hChkGenre,     showS);
        showCtrl (p->hChkOrchestra, showS);
        showCtrl (p->hChkLabel,     showS);
        showCtrl (p->hEditYearRange, showS);
        showCtrl (p->hSpinYear,      showS);
    }

    // Force full repaint
    InvalidateRect (hwnd, nullptr, TRUE);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: set source mode and repaint toggle buttons
// ─────────────────────────────────────────────────────────────────────────────

static void updateSourceToggle (TigerTandaPlugin* p, int newMode, HWND /*hwnd*/)
{
    p->sourceMode = newMode;
    p->lastSeenTitle.clear();
    p->lastSeenArtist.clear();
    if (p->hBtnSrcBrowser) InvalidateRect (p->hBtnSrcBrowser, nullptr, FALSE);
    if (p->hBtnSrcDeck)    InvalidateRect (p->hBtnSrcDeck,    nullptr, FALSE);
    p->saveSettings();
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

    // ── Create controls ──────────────────────────────────────────────────────
    case WM_CREATE:
    {
        if (!p) break;

        auto mkBtn = [&](int id, const wchar_t* text, DWORD style = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW) -> HWND
        {
            return CreateWindowW (L"BUTTON", text, style,
                                  0, 0, 10, 10, hwnd, (HMENU)(intptr_t)id, nullptr, nullptr);
        };
        // filter buttons use the same owner-draw helper as regular buttons

        // Top-bar buttons
        p->hBtnClose        = mkBtn (IDC_BTN_CLOSE,        L"X");
        p->hBtnLayoutToggle = mkBtn (IDC_BTN_LAYOUT_TOGGLE, L"");

        // Compact-mode tab buttons
        p->hBtnTabIdentify  = mkBtn (IDC_BTN_TAB_IDENTIFY, L"Identify");
        p->hBtnTabResults   = mkBtn (IDC_BTN_TAB_RESULTS,  L"Results");
        p->hBtnTabSettings  = mkBtn (IDC_BTN_TAB_SETTINGS, L"Settings");

        // Source toggle: [Browser | Deck ▾] — Deck opens popup menu
        p->hBtnSrcBrowser   = mkBtn (IDC_BTN_SRC_BROWSER, L"Browser");
        p->hBtnSrcDeck      = mkBtn (IDC_BTN_SRC_DECK,    L"Deck");

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

        // Filter toggle buttons (owner-draw, colored when active)
        p->hChkArtist    = mkBtn (IDC_CHK_SAME_ARTIST,    L"Artist");
        p->hChkSinger    = mkBtn (IDC_CHK_SAME_SINGER,    L"Singer");
        p->hChkGrouping  = mkBtn (IDC_CHK_SAME_GROUPING,  L"Grouping");
        p->hChkGenre     = mkBtn (IDC_CHK_SAME_GENRE,     L"Genre");
        p->hChkOrchestra = mkBtn (IDC_CHK_SAME_ORCHESTRA, L"Orchestra");
        p->hChkLabel     = mkBtn (IDC_CHK_SAME_LABEL,     L"Label");

        // Year range spinner
        p->hEditYearRange = CreateWindowExW (WS_EX_CLIENTEDGE, L"EDIT", L"5",
                                             WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
                                             0, 0, 10, 10, hwnd,
                                             (HMENU) IDC_EDIT_YEAR_RANGE, nullptr, nullptr);
        p->hSpinYear = CreateWindowW (UPDOWN_CLASS, nullptr,
                                      WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT
                                      | UDS_ARROWKEYS | UDS_HOTTRACK,
                                      0, 0, 10, 10, hwnd,
                                      (HMENU) IDC_SPIN_YEAR_RANGE, nullptr, nullptr);
        SendMessageW (p->hSpinYear, UDM_SETBUDDY,   (WPARAM) p->hEditYearRange, 0);
        SendMessageW (p->hSpinYear, UDM_SETRANGE32, 0, 20);
        SendMessageW (p->hSpinYear, UDM_SETPOS32,   0, p->yearRange);

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

        // Search VDJ button
        p->hBtnSearchVdj = mkBtn (IDC_BTN_SEARCH_VDJ, L"Search in VDJ");

        // Apply font to all children
        if (p->fontNormal)
        {
            EnumChildWindows (hwnd, [] (HWND child, LPARAM lp) -> BOOL {
                SendMessageW (child, WM_SETFONT, (WPARAM) lp, TRUE);
                return TRUE;
            }, (LPARAM) p->fontNormal);
        }

        // Apply initial layout
        applyLayout (p, hwnd);

        // Start 250ms polling timer
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
        const int cw = clientR.right;

        // Background
        fillRect (hdc, clientR, TCol::bg);

        // Top bar
        RECT topR { 0, 0, cw, TOP_H };
        fillRect (hdc, topR, TCol::panel);

        if (!p) { EndPaint (hwnd, &ps); return 0; }

        // ── WIDE MODE ────────────────────────────────────────────────────────
        if (p->viewMode == 0)
        {
            // "yr±" label in top bar (left of year edit)
            if (p->hEditYearRange && IsWindowVisible (p->hEditYearRange))
            {
                RECT wr;
                GetWindowRect (p->hEditYearRange, &wr);
                POINT tl { wr.left, wr.top };
                ScreenToClient (hwnd, &tl);
                RECT yrR { tl.x - 22, 0, tl.x, TOP_H };
                drawText (hdc, yrR, L"yr\u00B1", TCol::textDim, p->fontSmall,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Left panel background
            RECT leftR { 0, TOP_H, LEFT_W, DLG_H };
            fillRect (hdc, leftR, TCol::panel);

            // Vertical divider
            RECT divR { LEFT_W, TOP_H, LEFT_W + 1, DLG_H };
            fillRect (hdc, divR, TCol::cardBorder);

            // Section labels
            const int lx = PAD;
            {
                int ly = TOP_H + PAD;
                RECT r { lx, ly, LEFT_W - PAD, ly + 14 };
                drawText (hdc, r, L"IDENTIFY SONG", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                // CANDIDATES label
                ly += 14 + 4 + BTN_H + 6 + EDIT_H + 6;
                r = { lx, ly, LEFT_W - PAD, ly + 14 };
                drawText (hdc, r, L"CANDIDATES", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            // Right panel label
            {
                int ry = TOP_H + PAD;
                RECT r { RIGHT_X, ry, RIGHT_X + RIGHT_W, ry + 14 };
                drawText (hdc, r, L"SIMILAR SONGS", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            // "Tiger Tanda" label at bottom-left of right panel, next to Search VDJ button
            {
                const int bottomRow = DLG_H - PAD - BTN_H;
                RECT labelR { RIGHT_X, bottomRow, RIGHT_X + RIGHT_W - 124, bottomRow + BTN_H };
                drawText (hdc, labelR, L"Tiger Tanda", TCol::accent, p->fontTitle,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            // Detail box for selected result
            {
                const int rx = RIGHT_X;
                const int rw = DLG_W_WIDE - rx - PAD;
                const int bottomRow = DLG_H - PAD - BTN_H;
                const int detailTop = bottomRow - DETAIL_BOX_H - 4;
                RECT detR { rx, detailTop, rx + rw, detailTop + DETAIL_BOX_H };

                if (p->selectedResultIdx >= 0 && p->selectedResultIdx < (int) p->results.size())
                    drawResultDetailBox (hdc, detR, p->results[p->selectedResultIdx], p->fontSmall, p->fontBold);
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
        }
        // ── COMPACT MODE ─────────────────────────────────────────────────────
        else
        {
            // Body background (tabs are now in the top bar, body starts at TOP_H)
            RECT bodyR { 0, TOP_H, DLG_W_COMPACT, DLG_H };
            fillRect (hdc, bodyR, TCol::bg);

            // "yr±" label in Settings tab
            if (p->activeTab == 2 && p->hEditYearRange && IsWindowVisible (p->hEditYearRange))
            {
                RECT wr;
                GetWindowRect (p->hEditYearRange, &wr);
                POINT tl { wr.left, wr.top };
                ScreenToClient (hwnd, &tl);
                RECT yrR { PAD, tl.y, PAD + 26, tl.y + EDIT_H };
                drawText (hdc, yrR, L"yr\u00B1", TCol::textDim, p->fontSmall,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Detail box in Results tab
            if (p->activeTab == 1)
            {
                const int lx = PAD;
                const int lw = DLG_W_COMPACT - PAD * 2;
                const int bottomRow = DLG_H - PAD - BTN_H;
                const int detailTop = bottomRow - DETAIL_BOX_H - 4;
                RECT detR { lx, detailTop, lx + lw, detailTop + DETAIL_BOX_H };

                if (p->selectedResultIdx >= 0 && p->selectedResultIdx < (int) p->results.size())
                    drawResultDetailBox (hdc, detR, p->results[p->selectedResultIdx], p->fontSmall, p->fontBold);
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

                // "Tiger Tanda" label next to Search VDJ button
                RECT labelR { lx, bottomRow, lx + lw - 104, bottomRow + BTN_H };
                drawText (hdc, labelR, L"Tiger Tanda", TCol::accent, p->fontTitle,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }

        EndPaint (hwnd, &ps);
        return 0;
    }

    // ── Timer: browser/deck polling + visibility sync ─────────────────────────
    case WM_TIMER:
    {
        if (wParam != TIMER_BROWSE_POLL) break;
        if (!p) break;

        // Poll song from browser or deck
        // sourceMode: 0=Browser, 1=Left(deck0), 2=Right(deck1), 3=Active, 4=Inactive
        std::wstring newTitle, newArtist;
        if (p->sourceMode == 0)
        {
            newTitle  = p->vdjGetString ("get_browsed_song 'title'");
            newArtist = p->vdjGetString ("get_browsed_song 'artist'");
        }
        else
        {
            int deckIdx = 0;
            if      (p->sourceMode == 1) deckIdx = 0;   // Left
            else if (p->sourceMode == 2) deckIdx = 1;   // Right
            else if (p->sourceMode == 3) deckIdx = (int) p->vdjGetValue ("get_active_deck");   // Active
            else                          deckIdx = 1 - (int) p->vdjGetValue ("get_active_deck"); // Inactive
            char titleQ[32], artistQ[32];
            snprintf (titleQ,  sizeof (titleQ),  "deck%d_title",  deckIdx);
            snprintf (artistQ, sizeof (artistQ), "deck%d_artist", deckIdx);
            newTitle  = p->vdjGetString (titleQ);
            newArtist = p->vdjGetString (artistQ);
        }

        if (!newTitle.empty()
            && (newTitle != p->lastSeenTitle || newArtist != p->lastSeenArtist))
        {
            p->lastSeenTitle  = newTitle;
            p->lastSeenArtist = newArtist;
            if (p->hEditTitle)  SetWindowTextW (p->hEditTitle,  newTitle.c_str());
            if (p->hEditArtist) SetWindowTextW (p->hEditArtist, newArtist.c_str());
            p->runIdentification (newTitle, newArtist);
        }

        // Visibility sync: show unless VDJ owner window is minimised.
        // As an owned WS_POPUP window, Windows keeps us above VDJ in z-order
        // but allows other unrelated apps to sit on top — exactly the desired behaviour.
        if (p->dialogRequestedOpen)
        {
            HWND ownerHwnd   = GetWindow (hwnd, GW_OWNER);
            bool vdjMinimised = ownerHwnd && IsIconic (ownerHwnd);

            if (!vdjMinimised)
            {
                if (!IsWindowVisible (hwnd))
                    ShowWindow (hwnd, SW_SHOWNOACTIVATE);
                // No HWND_TOP / HWND_TOPMOST — owned window z-order handles it
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
        // Layout toggle
        case IDC_BTN_LAYOUT_TOGGLE:
            p->viewMode = (p->viewMode == 0) ? 1 : 0;
            p->saveSettings();
            applyLayout (p, hwnd);
            break;

        // Compact-mode tab buttons
        case IDC_BTN_TAB_IDENTIFY:
            p->activeTab = 0;
            p->saveSettings();
            applyLayout (p, hwnd);
            if (p->hBtnTabIdentify) InvalidateRect (p->hBtnTabIdentify, nullptr, FALSE);
            if (p->hBtnTabResults)  InvalidateRect (p->hBtnTabResults,  nullptr, FALSE);
            if (p->hBtnTabSettings) InvalidateRect (p->hBtnTabSettings, nullptr, FALSE);
            break;
        case IDC_BTN_TAB_RESULTS:
            p->activeTab = 1;
            p->saveSettings();
            applyLayout (p, hwnd);
            if (p->hBtnTabIdentify) InvalidateRect (p->hBtnTabIdentify, nullptr, FALSE);
            if (p->hBtnTabResults)  InvalidateRect (p->hBtnTabResults,  nullptr, FALSE);
            if (p->hBtnTabSettings) InvalidateRect (p->hBtnTabSettings, nullptr, FALSE);
            break;
        case IDC_BTN_TAB_SETTINGS:
            p->activeTab = 2;
            p->saveSettings();
            applyLayout (p, hwnd);
            if (p->hBtnTabIdentify) InvalidateRect (p->hBtnTabIdentify, nullptr, FALSE);
            if (p->hBtnTabResults)  InvalidateRect (p->hBtnTabResults,  nullptr, FALSE);
            if (p->hBtnTabSettings) InvalidateRect (p->hBtnTabSettings, nullptr, FALSE);
            break;

        // Source toggle: Browser | Deck (Deck opens popup to pick Left/Right/Active/Inactive)
        case IDC_BTN_SRC_BROWSER:
            updateSourceToggle (p, 0, hwnd);
            break;
        case IDC_BTN_SRC_DECK:
        {
            // Build popup menu — check the currently selected deck sub-mode
            HMENU menu = CreatePopupMenu();
            AppendMenuW (menu, MF_STRING | (p->sourceMode == 1 ? MF_CHECKED : 0), 1, L"Left");
            AppendMenuW (menu, MF_STRING | (p->sourceMode == 2 ? MF_CHECKED : 0), 2, L"Right");
            AppendMenuW (menu, MF_STRING | (p->sourceMode == 3 ? MF_CHECKED : 0), 3, L"Active");
            AppendMenuW (menu, MF_STRING | (p->sourceMode == 4 ? MF_CHECKED : 0), 4, L"Inactive");
            RECT btnR;
            GetWindowRect (p->hBtnSrcDeck, &btnR);
            int sel = TrackPopupMenu (menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
                                     btnR.left, btnR.bottom, 0, hwnd, nullptr);
            DestroyMenu (menu);
            if (sel >= 1 && sel <= 4)
                updateSourceToggle (p, sel, hwnd);
            break;
        }

        case IDC_BTN_SEARCH:
        {
            wchar_t title[512] = {}, artist[512] = {};
            GetWindowTextW (p->hEditTitle,  title,  512);
            GetWindowTextW (p->hEditArtist, artist, 512);
            p->runIdentification (title, artist);
            break;
        }

        case IDC_CANDIDATES_LIST:
            if (notifCode == LBN_SELCHANGE || notifCode == LBN_DBLCLK)
            {
                int sel = (int) SendMessageW (p->hCandList, LB_GETCURSEL, 0, 0);
                if (sel >= 0)
                {
                    p->confirmCandidate (sel);
                    // In compact mode, auto-switch to Results tab
                    if (p->viewMode == 1 && p->activeTab != 1)
                    {
                        p->activeTab = 1;
                        applyLayout (p, hwnd);
                        if (p->hBtnTabIdentify) InvalidateRect (p->hBtnTabIdentify, nullptr, FALSE);
                        if (p->hBtnTabResults)  InvalidateRect (p->hBtnTabResults,  nullptr, FALSE);
                        if (p->hBtnTabSettings) InvalidateRect (p->hBtnTabSettings, nullptr, FALSE);
                    }
                }
            }
            break;

        // Result list selection — update detail box only, don't trigger tanda search
        case IDC_RESULTS_LIST:
            if (notifCode == LBN_SELCHANGE)
            {
                int sel = (int) SendMessageW (p->hResultsList, LB_GETCURSEL, 0, 0);
                p->selectedResultIdx = sel;
                // Repaint detail box area
                InvalidateRect (hwnd, nullptr, FALSE);
            }
            break;

        case IDC_CHK_SAME_ARTIST:
            p->filterSameArtist = !p->filterSameArtist;
            if (p->hChkArtist) InvalidateRect (p->hChkArtist, nullptr, FALSE);
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_SINGER:
            p->filterSameSinger = !p->filterSameSinger;
            if (p->hChkSinger) InvalidateRect (p->hChkSinger, nullptr, FALSE);
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_GROUPING:
            p->filterSameGrouping = !p->filterSameGrouping;
            if (p->hChkGrouping) InvalidateRect (p->hChkGrouping, nullptr, FALSE);
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_GENRE:
            p->filterSameGenre = !p->filterSameGenre;
            if (p->hChkGenre) InvalidateRect (p->hChkGenre, nullptr, FALSE);
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_ORCHESTRA:
            p->filterSameOrchestra = !p->filterSameOrchestra;
            if (p->hChkOrchestra) InvalidateRect (p->hChkOrchestra, nullptr, FALSE);
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_LABEL:
            p->filterSameLabel = !p->filterSameLabel;
            if (p->hChkLabel) InvalidateRect (p->hChkLabel, nullptr, FALSE);
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;

        case IDC_BTN_SEARCH_VDJ:
        {
            // Search VDJ browser without updating similar songs list (#9)
            int sel = (int) SendMessageW (p->hResultsList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int) p->results.size())
            {
                const TgRecord& rec = p->results[sel];
                std::wstring query = rec.title;
                if (!rec.bandleader.empty()) query += L" " + rec.bandleader;
                std::string cmd = "search \"" + toUtf8 (query) + "\"";
                p->vdjSend (cmd);
            }
            break;
        }

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
            // Source toggle: Browser | Deck — flat text toggle style
            if (di->CtlID == IDC_BTN_SRC_BROWSER)
            {
                drawTextToggle (di, L"Browser", p->sourceMode == 0, p->fontSmall, p->fontBold, TCol::panel);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_SRC_DECK)
            {
                // Show current deck sub-mode in label when active
                static const wchar_t* deckNames[] = { L"", L"Left", L"Right", L"Active", L"Inactive" };
                bool deckActive = (p->sourceMode != 0);
                std::wstring label = deckActive
                    ? (std::wstring(L"Deck: ") + deckNames[p->sourceMode])
                    : L"Deck";
                drawTextToggle (di, label, deckActive, p->fontSmall, p->fontBold, TCol::panel);
                return TRUE;
            }

            // Compact-mode tab buttons — flat text toggle style
            if (di->CtlID == IDC_BTN_TAB_IDENTIFY)
            {
                drawTextToggle (di, L"Identify", p->activeTab == 0, p->fontSmall, p->fontBold, TCol::panel);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_RESULTS)
            {
                drawTextToggle (di, L"Results",  p->activeTab == 1, p->fontSmall, p->fontBold, TCol::panel);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_SETTINGS)
            {
                drawTextToggle (di, L"Settings", p->activeTab == 2, p->fontSmall, p->fontBold, TCol::panel);
                return TRUE;
            }

            // Layout toggle — arrow glyph on flat panel background, orange colour
            if (di->CtlID == IDC_BTN_LAYOUT_TOGGLE)
            {
                bool pressed = (di->itemState & ODS_SELECTED) != 0;
                fillRect (di->hDC, di->rcItem, pressed ? TCol::buttonHover : TCol::panel);
                drawArrowGlyph (di->hDC, di->rcItem, p->viewMode == 0);
                return TRUE;
            }

            // Magnifying glass search button
            if (di->CtlID == IDC_BTN_SEARCH)
            {
                COLORREF bg = RGB (35, 50, 70);
                bool pressed = (di->itemState & ODS_SELECTED) != 0;
                RECT r = di->rcItem;
                COLORREF fillBg = pressed ? TCol::buttonHover : bg;

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

            // Filter toggle buttons — colored when active
            if (di->CtlID == IDC_CHK_SAME_ARTIST    || di->CtlID == IDC_CHK_SAME_SINGER   ||
                di->CtlID == IDC_CHK_SAME_GROUPING  || di->CtlID == IDC_CHK_SAME_GENRE    ||
                di->CtlID == IDC_CHK_SAME_ORCHESTRA  || di->CtlID == IDC_CHK_SAME_LABEL)
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
                }
                wchar_t ftxt[64] = {};
                GetWindowTextW (di->hwndItem, ftxt, 64);
                COLORREF fbg = isOn ? TCol::accent    : TCol::buttonBg;
                COLORREF ffg = isOn ? TCol::textBright : TCol::textDim;
                drawOwnerButton (di, ftxt, fbg, ffg, p->fontSmall);
                return TRUE;
            }

            // Generic button styling
            wchar_t text[128] = {};
            GetWindowTextW (di->hwndItem, text, 128);
            std::wstring label (text);

            COLORREF bg = TCol::buttonBg, fg = TCol::textNormal;
            if      (di->CtlID == IDC_BTN_SEARCH_VDJ) { bg = RGB (35, 50, 70);  fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_CLOSE)       { bg = RGB (70, 28, 28);  fg = TCol::textBright; }

            drawOwnerButton (di, label, bg, fg, p->fontNormal);
            return TRUE;
        }

        // ── Owner-draw candidates list ────────────────────────────────────────
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

            // Small score dot (green/yellow/red) replacing % badge
            int dotCx = r.left + 10;
            int dotCy = r.top + CAND_ITEM_H / 2;
            drawScoreDot (hdc, dotCx, dotCy, mr.score);

            int tx = r.left + 22;
            // Row 1: Title  ·  Bandleader
            std::wstring row1 = rec.title;
            if (!rec.bandleader.empty()) row1 += L"  \u00B7  " + rec.bandleader;
            RECT titleR { tx, r.top + 3, r.right - 4, r.top + 3 + 16 };
            drawText (hdc, titleR, row1, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Row 2: Singer  ·  Year
            std::wstring row2;
            if (!rec.singer.empty()) row2 += rec.singer;
            if (!rec.year.empty())   { if (!row2.empty()) row2 += L"  \u00B7  "; row2 += rec.year; }
            RECT detailR { tx, r.top + 3 + 16 + 2, r.right - 4, r.bottom - 2 };
            drawText (hdc, detailR, row2, TCol::textDim, p->fontSmall,
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

        // ── Owner-draw results list — name + year only ───────────────────────
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

            // Title
            RECT titleR { tx, r.top, r.right - 50, r.bottom };
            drawText (hdc, titleR, rec.title, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Year (right-aligned)
            if (!rec.year.empty())
            {
                RECT yearR { r.right - 48, r.top, r.right - 4, r.bottom };
                drawText (hdc, yearR, rec.year, TCol::textDim, p->fontSmall,
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

        break;
    }

    // ── Measure items ────────────────────────────────────────────────────────
    case WM_MEASUREITEM:
    {
        auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*> (lParam);
        if (!mi) break;
        if (mi->CtlID == IDC_CANDIDATES_LIST) { mi->itemHeight = CAND_ITEM_H;    return TRUE; }
        if (mi->CtlID == IDC_RESULTS_LIST)    { mi->itemHeight = RESULT_ITEM_H;  return TRUE; }
        break;
    }

    // ── Year range spinner ───────────────────────────────────────────────────
    case WM_NOTIFY:
    {
        auto* nm = reinterpret_cast<NMHDR*> (lParam);
        if (!nm || !p) break;
        if (nm->idFrom == IDC_SPIN_YEAR_RANGE && nm->code == UDN_DELTAPOS)
        {
            auto* ud = reinterpret_cast<NMUPDOWN*> (lParam);
            int nv = ud->iPos + ud->iDelta;
            if (nv < 0)  nv = 0;
            if (nv > 20) nv = 20;
            p->yearRange = nv;
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
        }
        break;
    }

    // ── Checkbox / edit colours ──────────────────────────────────────────────
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC) wParam;
        SetBkColor   (hdc, TCol::panel);
        SetTextColor (hdc, TCol::textNormal);
        return (LRESULT) (p ? p->panelBrush : GetStockObject (NULL_BRUSH));
    }
    case WM_CTLCOLOREDIT:
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

    case WM_DESTROY:
        KillTimer (hwnd, TIMER_BROWSE_POLL);
        return 0;
    }

    return DefWindowProcW (hwnd, msg, wParam, lParam);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI helper implementations
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::repaintTopBar()
{
    if (!hDlg) return;
    RECT r { 0, 0, DLG_W, TOP_H };
    InvalidateRect (hDlg, &r, FALSE);
}
