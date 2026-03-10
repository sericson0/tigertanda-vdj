//==============================================================================
// TigerTanda VDJ Plugin - Main Window UI
// TandaWndProc: WM_CREATE, WM_PAINT, WM_TIMER, WM_COMMAND, WM_DRAWITEM
//==============================================================================

#include "TigerTanda.h"
#include <commctrl.h>
#include <uxtheme.h>

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
    fillRect (hdc, r, bg);

    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN oldPen = (HPEN) SelectObject (hdc, pen);
    MoveToEx (hdc, r.left,     r.bottom - 1, nullptr);
    LineTo   (hdc, r.right - 1, r.bottom - 1);
    LineTo   (hdc, r.right - 1, r.top);
    LineTo   (hdc, r.left,      r.top);
    LineTo   (hdc, r.left,      r.bottom - 1);
    SelectObject (hdc, oldPen);
    DeleteObject (pen);

    COLORREF fg = disabled ? TCol::textDim : fgColor;
    drawText (hdc, r, label, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// Draw a segment of the 3-part source toggle
static void drawSegmentButton (const DRAWITEMSTRUCT* di, const std::wstring& label,
                               bool isActive, HFONT font, bool isLeft, bool isRight)
{
    HDC hdc = di->hDC;
    RECT r = di->rcItem;
    bool pressed = (di->itemState & ODS_SELECTED) != 0;

    COLORREF bg = isActive ? TCol::accent
                : pressed  ? TCol::buttonHover
                           : TCol::buttonBg;
    COLORREF fg = isActive ? TCol::textBright : TCol::textDim;

    fillRect (hdc, r, bg);

    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN oldPen = (HPEN) SelectObject (hdc, pen);
    MoveToEx (hdc, r.left,  r.top,        nullptr);
    LineTo   (hdc, r.right, r.top);
    MoveToEx (hdc, r.left,  r.bottom - 1, nullptr);
    LineTo   (hdc, r.right, r.bottom - 1);
    if (isLeft)  { MoveToEx (hdc, r.left,      r.top, nullptr); LineTo (hdc, r.left,      r.bottom); }
    if (isRight) { MoveToEx (hdc, r.right - 1, r.top, nullptr); LineTo (hdc, r.right - 1, r.bottom); }
    // inner divider on the right
    if (!isRight) { MoveToEx (hdc, r.right - 1, r.top, nullptr); LineTo (hdc, r.right - 1, r.bottom); }
    SelectObject (hdc, oldPen);
    DeleteObject (pen);

    drawText (hdc, r, label, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// Draw a tab button (3-way tab strip in compact mode)
static void drawTabButton (const DRAWITEMSTRUCT* di, const std::wstring& label,
                           bool isActive, HFONT font, bool isLeft, bool isRight)
{
    HDC hdc = di->hDC;
    RECT r = di->rcItem;
    bool pressed = (di->itemState & ODS_SELECTED) != 0;

    COLORREF bg = isActive ? TCol::card
                : pressed  ? TCol::buttonHover
                           : TCol::panel;
    COLORREF fg = isActive ? TCol::accentBrt : TCol::textDim;

    fillRect (hdc, r, bg);

    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN oldPen = (HPEN) SelectObject (hdc, pen);
    // Top border (accent colour when active)
    HPEN accentPen = CreatePen (PS_SOLID, 2, isActive ? TCol::accent : TCol::cardBorder);
    SelectObject (hdc, accentPen);
    MoveToEx (hdc, r.left,  r.top, nullptr);
    LineTo   (hdc, r.right, r.top);
    SelectObject (hdc, pen);
    DeleteObject (accentPen);
    // Bottom
    MoveToEx (hdc, r.left,  r.bottom - 1, nullptr);
    LineTo   (hdc, r.right, r.bottom - 1);
    // Sides
    if (isLeft)  { MoveToEx (hdc, r.left,      r.top, nullptr); LineTo (hdc, r.left,      r.bottom); }
    if (isRight) { MoveToEx (hdc, r.right - 1, r.top, nullptr); LineTo (hdc, r.right - 1, r.bottom); }
    if (!isRight) { MoveToEx (hdc, r.right - 1, r.top, nullptr); LineTo (hdc, r.right - 1, r.bottom); }
    SelectObject (hdc, oldPen);
    DeleteObject (pen);

    drawText (hdc, r, label, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
    const int topY = (TOP_H - BTN_H) / 2;           // = 6
    // Close [×]
    MoveWindow (p->hBtnClose,        cw - 28,        topY, 22, BTN_H, FALSE);
    // Reset
    MoveWindow (p->hBtnReset,        cw - 28 - 4 - 54, topY, 54, BTN_H, FALSE);
    // Layout toggle ("Compact" / "Wide")
    MoveWindow (p->hBtnLayoutToggle, cw - 28 - 4 - 54 - 4 - 76, topY, 76, BTN_H, FALSE);
    SetWindowTextW (p->hBtnLayoutToggle, p->viewMode == 0 ? L"Compact" : L"Wide");

    // ── Tab strip (compact only) ────────────────────────────────────────────
    if (p->viewMode == 1)
    {
        const int tabW = DLG_W_COMPACT / 3;          // = 120
        MoveWindow (p->hBtnTabIdentify, 0,       TOP_H, tabW,                     TAB_H, FALSE);
        MoveWindow (p->hBtnTabResults,  tabW,    TOP_H, tabW,                     TAB_H, FALSE);
        MoveWindow (p->hBtnTabSettings, tabW*2,  TOP_H, DLG_W_COMPACT - tabW*2,  TAB_H, FALSE);
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
        const int lx = PAD;
        const int lw = LEFT_W - PAD;   // 262
        int ly = TOP_H + PAD;           // 44

        // "IDENTIFY SONG" section label painted in WM_PAINT — skip 14+4
        ly += 14 + 4;                   // 62

        // 3-part source toggle
        {
            const int segW = lw / 3;
            const int segR = lw - segW * 3;
            MoveWindow (p->hBtnSrcBrowser, lx,            ly, segW,         BTN_H, FALSE);
            MoveWindow (p->hBtnSrcDeckAct, lx + segW,     ly, segW,         BTN_H, FALSE);
            MoveWindow (p->hBtnSrcDeckOth, lx + segW * 2, ly, segW + segR,  BTN_H, FALSE);
        }
        ly += BTN_H + 6;                // 92

        // Search row
        {
            const int sbW = BTN_H;      // square search button
            const int gap = 4;
            const int etW = lw - sbW - gap;
            const int tW  = etW * 55 / 100;
            const int aW  = etW - tW - gap;
            MoveWindow (p->hEditTitle,  lx,              ly, tW,  EDIT_H, FALSE);
            MoveWindow (p->hEditArtist, lx + tW + gap,   ly, aW,  EDIT_H, FALSE);
            MoveWindow (p->hBtnSearch,  lx + lw - sbW,   ly - (BTN_H - EDIT_H) / 2, sbW, BTN_H, FALSE);
        }
        ly += EDIT_H + 6;               // 116

        // Filter checkboxes — 2 rows × 3 columns
        {
            const int colW = lw / 3;    // ≈ 87
            const int ckH  = 17;
            MoveWindow (p->hChkArtist,    lx,              ly, colW,          ckH, FALSE);
            MoveWindow (p->hChkSinger,    lx + colW,       ly, colW,          ckH, FALSE);
            MoveWindow (p->hChkGrouping,  lx + colW * 2,   ly, lw - colW * 2, ckH, FALSE);
            ly += ckH + 4;              // 137
            MoveWindow (p->hChkGenre,     lx,              ly, colW,          ckH, FALSE);
            MoveWindow (p->hChkOrchestra, lx + colW,       ly, colW,          ckH, FALSE);
            MoveWindow (p->hChkLabel,     lx + colW * 2,   ly, lw - colW * 2, ckH, FALSE);
            ly += ckH + 4;              // 158
        }

        // Year range — "yr±" label painted in WM_PAINT at lx, ly
        MoveWindow (p->hEditYearRange, lx + 28,      ly, 38, EDIT_H, FALSE);
        MoveWindow (p->hSpinYear,      lx + 28 + 38, ly, 16, EDIT_H, FALSE);
        ly += EDIT_H + 6;              // 182

        // "CANDIDATES" label painted in WM_PAINT — skip 14+4
        ly += 14 + 4;                  // 200

        // Candidates list — fills to bottom
        MoveWindow (p->hCandList, lx, ly, lw, DLG_H - ly - PAD, FALSE);

        // Show all identify-side controls
        showCtrl (p->hBtnSrcBrowser, true);
        showCtrl (p->hBtnSrcDeckAct, true);
        showCtrl (p->hBtnSrcDeckOth, true);
        showCtrl (p->hEditTitle,     true);
        showCtrl (p->hEditArtist,    true);
        showCtrl (p->hBtnSearch,     true);
        showCtrl (p->hChkArtist,     true);
        showCtrl (p->hChkSinger,     true);
        showCtrl (p->hChkGrouping,   true);
        showCtrl (p->hChkGenre,      true);
        showCtrl (p->hChkOrchestra,  true);
        showCtrl (p->hChkLabel,      true);
        showCtrl (p->hEditYearRange, true);
        showCtrl (p->hSpinYear,      true);
        showCtrl (p->hCandList,      true);

        // Right panel
        const int rx = RIGHT_X;
        const int rw = DLG_W_WIDE - rx - PAD;
        int ry = TOP_H + PAD + 14 + 4; // 62 (below "SIMILAR SONGS" label)
        const int vdjBtnY = DLG_H - PAD - BTN_H;
        MoveWindow (p->hResultsList, rx, ry, rw, vdjBtnY - ry - PAD, FALSE);
        MoveWindow (p->hBtnSearchVdj, rx, vdjBtnY, rw, BTN_H, FALSE);
        showCtrl (p->hResultsList,  true);
        showCtrl (p->hBtnSearchVdj, true);
    }
    // ── COMPACT MODE ─────────────────────────────────────────────────────────
    else
    {
        const int lx     = PAD;
        const int lw     = DLG_W_COMPACT - PAD * 2;  // 344
        const int bodyY  = TOP_H + TAB_H;             // 64
        const bool showI = (p->activeTab == 0);
        const bool showR = (p->activeTab == 1);
        const bool showS = (p->activeTab == 2);

        // ── Identify tab ─────────────────────────────────────────────────────
        if (showI)
        {
            int ly = bodyY + PAD;   // 72

            const int segW = lw / 3;
            const int segR = lw - segW * 3;
            MoveWindow (p->hBtnSrcBrowser, lx,            ly, segW,         BTN_H, FALSE);
            MoveWindow (p->hBtnSrcDeckAct, lx + segW,     ly, segW,         BTN_H, FALSE);
            MoveWindow (p->hBtnSrcDeckOth, lx + segW * 2, ly, segW + segR,  BTN_H, FALSE);
            ly += BTN_H + 6;        // 102

            const int sbW = BTN_H;
            const int gap = 4;
            const int etW = lw - sbW - gap;
            const int tW  = etW * 55 / 100;
            const int aW  = etW - tW - gap;
            MoveWindow (p->hEditTitle,  lx,            ly, tW,  EDIT_H, FALSE);
            MoveWindow (p->hEditArtist, lx + tW + gap, ly, aW,  EDIT_H, FALSE);
            MoveWindow (p->hBtnSearch,  lx + lw - sbW, ly - (BTN_H - EDIT_H) / 2, sbW, BTN_H, FALSE);
            ly += EDIT_H + 6;       // 126

            MoveWindow (p->hCandList, lx, ly, lw, DLG_H - ly - PAD, FALSE);
        }
        showCtrl (p->hBtnSrcBrowser, showI);
        showCtrl (p->hBtnSrcDeckAct, showI);
        showCtrl (p->hBtnSrcDeckOth, showI);
        showCtrl (p->hEditTitle,     showI);
        showCtrl (p->hEditArtist,    showI);
        showCtrl (p->hBtnSearch,     showI);
        showCtrl (p->hCandList,      showI);

        // ── Results tab ──────────────────────────────────────────────────────
        if (showR)
        {
            int ry = bodyY + PAD;   // 72
            const int vdjBtnY = DLG_H - PAD - BTN_H;
            MoveWindow (p->hResultsList,  lx, ry,      lw, vdjBtnY - ry - PAD, FALSE);
            MoveWindow (p->hBtnSearchVdj, lx, vdjBtnY, lw, BTN_H,              FALSE);
        }
        showCtrl (p->hResultsList,  showR);
        showCtrl (p->hBtnSearchVdj, showR);

        // ── Settings tab ─────────────────────────────────────────────────────
        if (showS)
        {
            int sy = bodyY + PAD;   // 72
            const int colW = lw / 3;    // ≈ 114
            const int ckH  = 18;
            MoveWindow (p->hChkArtist,    lx,            sy, colW,          ckH, FALSE);
            MoveWindow (p->hChkSinger,    lx + colW,     sy, colW,          ckH, FALSE);
            MoveWindow (p->hChkGrouping,  lx + colW * 2, sy, lw - colW * 2, ckH, FALSE);
            sy += ckH + 6;          // 96
            MoveWindow (p->hChkGenre,     lx,            sy, colW,          ckH, FALSE);
            MoveWindow (p->hChkOrchestra, lx + colW,     sy, colW,          ckH, FALSE);
            MoveWindow (p->hChkLabel,     lx + colW * 2, sy, lw - colW * 2, ckH, FALSE);
            sy += ckH + 8;          // 122
            // "yr±" painted in WM_PAINT
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
    if (p->hBtnSrcDeckAct) InvalidateRect (p->hBtnSrcDeckAct, nullptr, FALSE);
    if (p->hBtnSrcDeckOth) InvalidateRect (p->hBtnSrcDeckOth, nullptr, FALSE);
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

        // Create all controls at a dummy position; applyLayout will move them.
        auto mkBtn = [&](int id, const wchar_t* text, DWORD style = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW) -> HWND
        {
            return CreateWindowW (L"BUTTON", text, style,
                                  0, 0, 10, 10, hwnd, (HMENU)(intptr_t)id, nullptr, nullptr);
        };
        auto mkChk = [&](int id, const wchar_t* text) -> HWND
        {
            HWND h = CreateWindowW (L"BUTTON", text,
                                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                    0, 0, 10, 10, hwnd, (HMENU)(intptr_t)id, nullptr, nullptr);
            SetWindowTheme (h, L"", L"");
            return h;
        };

        // Top-bar buttons
        p->hBtnClose        = mkBtn (IDC_BTN_CLOSE,        L"X");
        p->hBtnReset        = mkBtn (IDC_BTN_RESET,        L"Reset");
        p->hBtnLayoutToggle = mkBtn (IDC_BTN_LAYOUT_TOGGLE, L"Compact");

        // Compact-mode tab buttons (hidden until layout applied)
        p->hBtnTabIdentify  = mkBtn (IDC_BTN_TAB_IDENTIFY, L"Identify");
        p->hBtnTabResults   = mkBtn (IDC_BTN_TAB_RESULTS,  L"Results");
        p->hBtnTabSettings  = mkBtn (IDC_BTN_TAB_SETTINGS, L"Settings");

        // Source toggle
        p->hBtnSrcBrowser   = mkBtn (IDC_BTN_SRC_BROWSER,  L"Browser");
        p->hBtnSrcDeckAct   = mkBtn (IDC_BTN_SRC_DECK_ACT, L"Deck (Active)");
        p->hBtnSrcDeckOth   = mkBtn (IDC_BTN_SRC_DECK_OTH, L"Deck (Other)");

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

        // Filter checkboxes
        p->hChkArtist    = mkChk (IDC_CHK_SAME_ARTIST,    L"Artist");
        p->hChkSinger    = mkChk (IDC_CHK_SAME_SINGER,    L"Singer");
        p->hChkGrouping  = mkChk (IDC_CHK_SAME_GROUPING,  L"Grouping");
        p->hChkGenre     = mkChk (IDC_CHK_SAME_GENRE,     L"Genre");
        p->hChkOrchestra = mkChk (IDC_CHK_SAME_ORCHESTRA, L"Orchestra");
        p->hChkLabel     = mkChk (IDC_CHK_SAME_LABEL,     L"Label");

        SendMessageW (p->hChkArtist,    BM_SETCHECK, p->filterSameArtist    ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW (p->hChkSinger,    BM_SETCHECK, p->filterSameSinger    ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW (p->hChkGrouping,  BM_SETCHECK, p->filterSameGrouping  ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW (p->hChkGenre,     BM_SETCHECK, p->filterSameGenre     ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW (p->hChkOrchestra, BM_SETCHECK, p->filterSameOrchestra ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW (p->hChkLabel,     BM_SETCHECK, p->filterSameLabel     ? BST_CHECKED : BST_UNCHECKED, 0);

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

        // Title
        if (p)
        {
            RECT titleR { 10, 0, 180, TOP_H };
            drawText (hdc, titleR, L"TigerTanda", TCol::accentBrt, p->fontTitle,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        if (!p) { EndPaint (hwnd, &ps); return 0; }

        // ── WIDE MODE ────────────────────────────────────────────────────────
        if (p->viewMode == 0)
        {
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

                // CANDIDATES label: offset by toggle + search + 2 filter rows + year row
                ly += 14 + 4 + BTN_H + 6 + EDIT_H + 6 + (17 + 4) * 2 + EDIT_H + 6;
                r = { lx, ly, LEFT_W - PAD, ly + 14 };
                drawText (hdc, r, L"CANDIDATES", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            // "yr±" label — painted just left of the year edit
            if (p->hEditYearRange && IsWindowVisible (p->hEditYearRange))
            {
                RECT wr;
                GetWindowRect (p->hEditYearRange, &wr);
                POINT tl { wr.left, wr.top };
                ScreenToClient (hwnd, &tl);
                RECT yrR { lx, tl.y, lx + 26, tl.y + EDIT_H };
                drawText (hdc, yrR, L"yr\u00B1", TCol::textDim, p->fontSmall,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Right panel label
            {
                int ry = TOP_H + PAD;
                RECT r { RIGHT_X, ry, RIGHT_X + RIGHT_W, ry + 14 };
                drawText (hdc, r, L"SIMILAR SONGS", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        // ── COMPACT MODE ─────────────────────────────────────────────────────
        else
        {
            // Tab strip background
            RECT tabBgR { 0, TOP_H, DLG_W_COMPACT, TOP_H + TAB_H };
            fillRect (hdc, tabBgR, TCol::panel);

            // Body background
            RECT bodyR { 0, TOP_H + TAB_H, DLG_W_COMPACT, DLG_H };
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
        std::wstring newTitle, newArtist;
        if (p->sourceMode == 0)
        {
            newTitle  = p->vdjGetString ("get_browsed_song 'title'");
            newArtist = p->vdjGetString ("get_browsed_song 'artist'");
        }
        else
        {
            int activeDeck = (int) p->vdjGetValue ("get_active_deck");
            int deckIdx    = (p->sourceMode == 1) ? activeDeck : (1 - activeDeck);
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

        // Visibility sync: show/hide with VDJ foreground state
        if (p->dialogRequestedOpen)
        {
            if (isVdjHostForeground())
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

        // 3-part segmented source toggle
        case IDC_BTN_SRC_BROWSER:  updateSourceToggle (p, 0, hwnd); break;
        case IDC_BTN_SRC_DECK_ACT: updateSourceToggle (p, 1, hwnd); break;
        case IDC_BTN_SRC_DECK_OTH: updateSourceToggle (p, 2, hwnd); break;

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

        case IDC_CHK_SAME_ARTIST:
            p->filterSameArtist = SendMessageW (p->hChkArtist, BM_GETCHECK, 0, 0) == BST_CHECKED;
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_SINGER:
            p->filterSameSinger = SendMessageW (p->hChkSinger, BM_GETCHECK, 0, 0) == BST_CHECKED;
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_GROUPING:
            p->filterSameGrouping = SendMessageW (p->hChkGrouping, BM_GETCHECK, 0, 0) == BST_CHECKED;
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_GENRE:
            p->filterSameGenre = SendMessageW (p->hChkGenre, BM_GETCHECK, 0, 0) == BST_CHECKED;
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_ORCHESTRA:
            p->filterSameOrchestra = SendMessageW (p->hChkOrchestra, BM_GETCHECK, 0, 0) == BST_CHECKED;
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        case IDC_CHK_SAME_LABEL:
            p->filterSameLabel = SendMessageW (p->hChkLabel, BM_GETCHECK, 0, 0) == BST_CHECKED;
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;

        case IDC_BTN_SEARCH_VDJ:
        {
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

        case IDC_BTN_RESET:
            p->resetAll();
            break;

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
            // Source toggle segments
            if (di->CtlID == IDC_BTN_SRC_BROWSER)
            {
                drawSegmentButton (di, L"Browser",      p->sourceMode == 0, p->fontSmall, true,  false);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_SRC_DECK_ACT)
            {
                drawSegmentButton (di, L"Deck (Active)",p->sourceMode == 1, p->fontSmall, false, false);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_SRC_DECK_OTH)
            {
                drawSegmentButton (di, L"Deck (Other)", p->sourceMode == 2, p->fontSmall, false, true);
                return TRUE;
            }

            // Compact-mode tab buttons
            if (di->CtlID == IDC_BTN_TAB_IDENTIFY)
            {
                drawTabButton (di, L"Identify", p->activeTab == 0, p->fontSmall, true,  false);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_RESULTS)
            {
                drawTabButton (di, L"Results",  p->activeTab == 1, p->fontSmall, false, false);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_SETTINGS)
            {
                drawTabButton (di, L"Settings", p->activeTab == 2, p->fontSmall, false, true);
                return TRUE;
            }

            // Magnifying glass search button
            if (di->CtlID == IDC_BTN_SEARCH)
            {
                COLORREF bg = RGB (35, 50, 70);
                bool pressed = (di->itemState & ODS_SELECTED) != 0;
                fillRect (di->hDC, di->rcItem, pressed ? TCol::buttonHover : bg);

                HPEN pen    = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldPen = (HPEN) SelectObject (di->hDC, pen);
                RECT r = di->rcItem;
                MoveToEx (di->hDC, r.left,     r.bottom - 1, nullptr);
                LineTo   (di->hDC, r.right - 1, r.bottom - 1);
                LineTo   (di->hDC, r.right - 1, r.top);
                LineTo   (di->hDC, r.left,       r.top);
                LineTo   (di->hDC, r.left,       r.bottom - 1);
                SelectObject (di->hDC, oldPen);
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

            // Generic button styling
            wchar_t text[128] = {};
            GetWindowTextW (di->hwndItem, text, 128);
            std::wstring label (text);

            COLORREF bg = TCol::buttonBg, fg = TCol::textNormal;
            if      (di->CtlID == IDC_BTN_SEARCH_VDJ)    { bg = RGB (35, 50, 70);  fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_RESET)          { bg = RGB (60, 28, 28);  fg = TCol::textNormal; }
            else if (di->CtlID == IDC_BTN_CLOSE)          { bg = RGB (70, 28, 28);  fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_LAYOUT_TOGGLE)  { bg = TCol::buttonBg;    fg = TCol::accentBrt; }

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

            // Score badge
            wchar_t scoreBuf[16];
            swprintf_s (scoreBuf, L"%.0f%%", mr.score);
            const int badgeW = 38, badgeH = 13;
            const int bx = r.left + 4;
            const int by = r.top + (CAND_ITEM_H - badgeH) / 2;
            {
                RECT br { bx, by, bx + badgeW, by + badgeH };
                fillRect (hdc, br, TCol::scoreBg (mr.score));
                drawText (hdc, br, scoreBuf, TCol::scoreColor (mr.score), p->fontSmall,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            int tx = bx + badgeW + 5;
            RECT titleR  { tx,     r.top + 3,     r.right - 4, r.top + 14 };
            RECT detailR { tx,     r.top + 14,    r.right - 4, r.bottom - 2 };
            drawText (hdc, titleR, rec.title, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

            std::wstring det = rec.bandleader;
            if (!rec.singer.empty()) det += L"  \u00B7  " + rec.singer;
            if (!rec.year.empty())   det += L"  \u00B7  " + rec.year;
            drawText (hdc, detailR, det, TCol::textDim, p->fontSmall,
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

        // ── Owner-draw results list ───────────────────────────────────────────
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
            int avW = r.right - tx - 4;

            RECT tm = { 0, 0, 9999, 100 };
            HFONT oldFont = (HFONT) SelectObject (hdc, p->fontBold);
            DrawTextW (hdc, rec.title.c_str(), -1, &tm, DT_CALCRECT | DT_SINGLELINE);
            SelectObject (hdc, oldFont);
            int titleW = (std::min) ((int)(tm.right - tm.left), avW * 55 / 100);

            RECT titleR { tx, r.top, tx + titleW, r.bottom };
            drawText (hdc, titleR, rec.title, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            int metaX = tx + titleW + 10;
            std::wstring meta;
            if (!rec.bandleader.empty()) meta += rec.bandleader;
            if (!rec.singer.empty())     { if (!meta.empty()) meta += L"  \u00B7  "; meta += rec.singer; }
            if (!rec.genre.empty())      { if (!meta.empty()) meta += L"  \u00B7  "; meta += rec.genre; }
            if (!rec.year.empty())       { if (!meta.empty()) meta += L"  \u00B7  "; meta += rec.year; }

            RECT metaR { metaX, r.top, r.right - 4, r.bottom };
            drawText (hdc, metaR, meta, TCol::textDim, p->fontSmall,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

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
