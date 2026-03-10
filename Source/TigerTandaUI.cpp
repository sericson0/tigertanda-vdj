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
                            COLORREF bgColor = TCol::panel)
{
    HDC hdc = di->hDC;
    RECT r = di->rcItem;
    bool pressed = (di->itemState & ODS_SELECTED) != 0;

    fillRect (hdc, r, bgColor);

    COLORREF fg = (isActive || pressed) ? TCol::accentBrt : TCol::textDim;
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

// Metadata detail box for selected result
// Row 1: Bandleader · Singer   Row 2: Date · Genre · Label
static void drawResultDetailBox (HDC hdc, RECT r, const TgRecord& rec, HFONT fontSmall, HFONT fontBold)
{
    fillRect (hdc, r, TCol::card);

    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN old = (HPEN) SelectObject (hdc, pen);
    MoveToEx (hdc, r.left,  r.top, nullptr);
    LineTo   (hdc, r.right, r.top);
    SelectObject (hdc, old);
    DeleteObject (pen);

    int px = r.left + 6;
    int py = r.top + 6;
    const int lineH1 = 15;
    const int lineH2 = 14;

    // Row 1: Bandleader · Singer
    std::wstring line1 = rec.bandleader;
    if (!rec.singer.empty()) { if (!line1.empty()) line1 += L"  \u00B7  "; line1 += rec.singer; }
    RECT r1 { px, py, r.right - 6, py + lineH1 };
    drawText (hdc, r1, line1, TCol::textBright, fontBold,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    py += lineH1 + 3;

    // Row 2: Date · Genre · Label
    std::wstring line2;
    if (!rec.year.empty())  line2 += rec.year;
    if (!rec.genre.empty()) { if (!line2.empty()) line2 += L"  \u00B7  "; line2 += rec.genre; }
    if (!rec.label.empty()) { if (!line2.empty()) line2 += L"  \u00B7  "; line2 += rec.label; }
    RECT r2 { px, py, r.right - 6, py + lineH2 };
    drawText (hdc, r2, line2, TCol::textDim, fontSmall,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
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
    const int topY = (TOP_H - BTN_H) / 2;

    // Close button — right-anchored in top bar
    MoveWindow (p->hBtnClose, DLG_W - 26, topY, 22, BTN_H, FALSE);
    ShowWindow (p->hBtnClose, SW_SHOW);

    // 4 tab buttons occupy remaining top bar left of close
    {
        const int rightBtnsW = 22 + PAD;     // close + right pad
        const int tabAreaW   = DLG_W - rightBtnsW;
        const int tabW       = tabAreaW / 4;
        MoveWindow (p->hBtnTabTrack,    0,         topY, tabW,              BTN_H, FALSE);
        MoveWindow (p->hBtnTabMatches,  tabW,      topY, tabW,              BTN_H, FALSE);
        MoveWindow (p->hBtnTabBrowse,   tabW * 2,  topY, tabW,              BTN_H, FALSE);
        MoveWindow (p->hBtnTabSettings, tabW * 3,  topY, tabAreaW - tabW*3, BTN_H, FALSE);
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

        // Source row: [Browser] [Deck] [DeckSel▾]
        const int bW = lw * 45 / 100;
        const int dW = lw * 27 / 100;
        const int cW = lw - bW - dW;
        MoveWindow (p->hBtnSrcBrowser, lx,          ly, bW, BTN_H, FALSE);
        MoveWindow (p->hBtnSrcDeck,    lx + bW,     ly, dW, BTN_H, FALSE);
        MoveWindow (p->hBtnDeckSel,    lx + bW + dW, ly, cW, BTN_H, FALSE);
        ly += BTN_H + 6;

        // Search row: [Title][Artist][Q]
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
    showCtrl (p->hBtnSrcBrowser, showT);
    showCtrl (p->hBtnSrcDeck,    showT);
    showCtrl (p->hBtnDeckSel,    showT);
    showCtrl (p->hEditTitle,     showT);
    showCtrl (p->hEditArtist,    showT);
    showCtrl (p->hBtnSearch,     showT);
    showCtrl (p->hCandList,      showT);

    // ── Matches tab ───────────────────────────────────────────────────────────
    if (showM)
    {
        int ry = bodyY + PAD;
        const int bottomRow  = DLG_H - PAD - BTN_H;
        const int vdjBtnW    = 100;
        MoveWindow (p->hBtnSearchVdj, lx + lw - vdjBtnW, bottomRow, vdjBtnW, BTN_H, FALSE);

        const int detailTop = bottomRow - DETAIL_BOX_H - 4;
        MoveWindow (p->hResultsList, lx, ry, lw, detailTop - ry - 2, FALSE);
    }
    showCtrl (p->hResultsList,  showM);
    showCtrl (p->hBtnSearchVdj, showM);

    // ── Browse tab ────────────────────────────────────────────────────────────
    if (showB)
    {
        const int actBtnY  = DLG_H - PAD - BTN_H;
        const int preY     = actBtnY - 6 - PRE_WAVE_H;
        const int listBot  = preY - 6;
        const int listH    = listBot - (bodyY + PAD);

        MoveWindow (p->hBrowseList, lx, bodyY + PAD, lw, listH, FALSE);

        // Prelisten: [▶ btn][waveform]
        const int preBtnW = 28;
        MoveWindow (p->hBtnPrelisten, lx, preY, preBtnW, PRE_WAVE_H, FALSE);

        // Store waveform rect for painting
        p->prelistenWaveRect = { lx + preBtnW + 4, preY,
                                 lx + lw,          preY + PRE_WAVE_H };

        // Action buttons: [Add to End] [Add After Current]
        const int abW = lw / 2;
        MoveWindow (p->hBtnAddEnd,   lx,        actBtnY, abW,      BTN_H, FALSE);
        MoveWindow (p->hBtnAddAfter, lx + abW,  actBtnY, lw - abW, BTN_H, FALSE);
    }
    showCtrl (p->hBrowseList,    showB);
    showCtrl (p->hBtnPrelisten,  showB);
    showCtrl (p->hBtnAddEnd,     showB);
    showCtrl (p->hBtnAddAfter,   showB);

    // ── Settings tab ──────────────────────────────────────────────────────────
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
    showCtrl (p->hChkArtist,     showS);
    showCtrl (p->hChkSinger,     showS);
    showCtrl (p->hChkGrouping,   showS);
    showCtrl (p->hChkGenre,      showS);
    showCtrl (p->hChkOrchestra,  showS);
    showCtrl (p->hChkLabel,      showS);
    showCtrl (p->hEditYearRange, showS);
    showCtrl (p->hSpinYear,      showS);

    InvalidateRect (hwnd, nullptr, TRUE);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Source mode helpers
// ─────────────────────────────────────────────────────────────────────────────

static void updateSourceToggle (TigerTandaPlugin* p, int newMode, HWND /*hwnd*/)
{
    p->sourceMode = newMode;
    if (newMode != 0) p->lastDeckMode = newMode;
    p->lastSeenTitle.clear();
    p->lastSeenArtist.clear();
    if (p->hBtnSrcBrowser) InvalidateRect (p->hBtnSrcBrowser, nullptr, FALSE);
    if (p->hBtnSrcDeck)    InvalidateRect (p->hBtnSrcDeck,    nullptr, FALSE);
    if (p->hBtnDeckSel)    InvalidateRect (p->hBtnDeckSel,    nullptr, FALSE);
    p->saveSettings();
}

// Label for deck sub-mode
static const wchar_t* deckModeLabel (int mode)
{
    switch (mode)
    {
        case 1:  return L"Left";
        case 2:  return L"Right";
        case 3:  return L"Active";
        case 4:  return L"Inact.";
        default: return L"Active";
    }
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
        p->hBtnTabSettings  = mkBtn (IDC_BTN_TAB_SETTINGS, L"\u2699");  // ⚙

        // Source toggle: [Browser] [Deck] [DeckSel▾]
        p->hBtnSrcBrowser = mkBtn (IDC_BTN_SRC_BROWSER, L"Browser");
        p->hBtnSrcDeck    = mkBtn (IDC_BTN_SRC_DECK,    L"Deck");
        p->hBtnDeckSel    = mkBtn (IDC_BTN_DECK_SEL,    L"");

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

        // Filter buttons
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

        // Browse list
        p->hBrowseList = CreateWindowW (L"LISTBOX", nullptr,
                                        WS_CHILD | WS_VISIBLE | WS_VSCROLL
                                        | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS
                                        | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                        0, 0, 10, 10, hwnd,
                                        (HMENU) IDC_BROWSE_LIST, nullptr, nullptr);

        // Prelisten + action buttons
        p->hBtnPrelisten = mkBtn (IDC_BTN_PRELISTEN, L"\u25B6");  // ▶
        p->hBtnAddEnd    = mkBtn (IDC_BTN_ADD_END,   L"Add to End");
        p->hBtnAddAfter  = mkBtn (IDC_BTN_ADD_AFTER, L"Add After");

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

        // ── Track tab paint ───────────────────────────────────────────────────
        // (no extra painting needed, all in controls)

        // ── Matches tab paint ─────────────────────────────────────────────────
        if (p->activeTab == 1)
        {
            const int lx = PAD;
            const int lw = DLG_W - PAD * 2;
            const int bottomRow  = DLG_H - PAD - BTN_H;
            const int detailTop  = bottomRow - DETAIL_BOX_H - 4;
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

            // Brand label next to Search VDJ button
            RECT labelR { lx, DLG_H - PAD - BTN_H, lx + lw - 104, DLG_H - PAD };
            drawText (hdc, labelR, L"Tiger Tanda", TCol::accent, p->fontTitle,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
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
            const int lx = PAD;
            const int bodyY = TOP_H;
            const int lw = DLG_W - PAD * 2;

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
        }

        EndPaint (hwnd, &ps);
        return 0;
    }

    // ── Timer: browser/deck polling + visibility sync ────────────────────────
    case WM_TIMER:
    {
        if (wParam != TIMER_BROWSE_POLL) break;
        if (!p) break;

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

        // ── Poll song from browser or deck ────────────────────────────────────
        std::wstring newTitle, newArtist;
        if (p->sourceMode == 0)
        {
            newTitle  = p->vdjGetString ("get_browsed_song 'title'");
            newArtist = p->vdjGetString ("get_browsed_song 'artist'");
        }
        else
        {
            int deckIdx = 0;
            if      (p->sourceMode == 1) deckIdx = 0;
            else if (p->sourceMode == 2) deckIdx = 1;
            else if (p->sourceMode == 3) deckIdx = (int) p->vdjGetValue ("get_active_deck");
            else                          deckIdx = 1 - (int) p->vdjGetValue ("get_active_deck");
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

        // ── Poll browser file path for browse history + prelisten ─────────────
        std::wstring browsePath = p->vdjGetString ("get_browsed_filepath");
        if (!browsePath.empty() && browsePath != p->lastSeenBrowsePath)
        {
            p->lastSeenBrowsePath = browsePath;

            // Update prelisten waveform bins
            rebuildPrelistenWaveBins (p->prelistenWaveBins, browsePath);
            if (p->activeTab == 2)
                InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);

            // Add to browse history (most-recent first, deduplicate, cap 50)
            std::wstring browseTitle  = p->vdjGetString ("get_browsed_song 'title'");
            std::wstring browseArtist = p->vdjGetString ("get_browsed_song 'artist'");
            if (!browseTitle.empty())
            {
                // Remove duplicate (same path)
                p->browseItems.erase (
                    std::remove_if (p->browseItems.begin(), p->browseItems.end(),
                        [&](const BrowseItem& b){ return b.filePath == browsePath; }),
                    p->browseItems.end());

                BrowseItem bi;
                bi.title    = browseTitle;
                bi.artist   = browseArtist;
                bi.filePath = browsePath;
                p->browseItems.insert (p->browseItems.begin(), bi);
                if ((int) p->browseItems.size() > 50)
                    p->browseItems.resize (50);

                // Refresh browse listbox
                if (p->hBrowseList)
                {
                    SendMessageW (p->hBrowseList, LB_RESETCONTENT, 0, 0);
                    for (int i = 0; i < (int) p->browseItems.size(); ++i)
                        SendMessageW (p->hBrowseList, LB_ADDSTRING, 0, (LPARAM) L"");
                }
            }
        }

        // ── Prelisten position update ─────────────────────────────────────────
        if (p->prelistenActive && p->activeTab == 2)
        {
            double pos = p->vdjGetValue ("prelisten_pos");
            if (pos <= 0.0) pos = p->vdjGetValue ("get_prelisten_pos");
            if (pos < 0.0) pos = 0.0;
            if (pos > 100.0) pos = 100.0;
            if (std::abs (pos - p->prelistenPos) > 0.4)
            {
                p->prelistenPos = pos;
                InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
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

        // Source: Browser
        case IDC_BTN_SRC_BROWSER:
            updateSourceToggle (p, 0, hwnd);
            break;

        // Source: Deck (use lastDeckMode)
        case IDC_BTN_SRC_DECK:
            updateSourceToggle (p, p->lastDeckMode, hwnd);
            break;

        // Deck sub-mode dropdown button
        case IDC_BTN_DECK_SEL:
        {
            HMENU menu = CreatePopupMenu();
            AppendMenuW (menu, MF_STRING | (p->lastDeckMode == 3 ? MF_CHECKED : 0), 3, L"Active");
            AppendMenuW (menu, MF_STRING | (p->lastDeckMode == 4 ? MF_CHECKED : 0), 4, L"Inactive");
            AppendMenuW (menu, MF_STRING | (p->lastDeckMode == 1 ? MF_CHECKED : 0), 1, L"Left");
            AppendMenuW (menu, MF_STRING | (p->lastDeckMode == 2 ? MF_CHECKED : 0), 2, L"Right");
            RECT btnR;
            GetWindowRect (p->hBtnDeckSel, &btnR);
            int sel = TrackPopupMenu (menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
                                     btnR.left, btnR.bottom, 0, hwnd, nullptr);
            DestroyMenu (menu);
            if (sel >= 1 && sel <= 4)
                updateSourceToggle (p, sel, hwnd);
            break;
        }

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

        // Search VDJ browser (with diacritic normalization)
        case IDC_BTN_SEARCH_VDJ:
        {
            int sel = (int) SendMessageW (p->hResultsList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int) p->results.size())
            {
                const TgRecord& rec = p->results[sel];
                std::wstring query = normalizeForSearch (rec.title);
                if (!rec.bandleader.empty()) query += L" " + normalizeForSearch (rec.bandleader);
                std::string cmd = "search \"" + toUtf8 (query) + "\"";
                p->vdjSend (cmd);
            }
            break;
        }

        // Prelisten toggle
        case IDC_BTN_PRELISTEN:
            p->prelistenActive = !p->prelistenActive;
            p->vdjSend (p->prelistenActive ? "prelisten" : "prelisten_stop");
            if (!p->prelistenActive) p->prelistenPos = 0.0;
            SetWindowTextW (p->hBtnPrelisten, p->prelistenActive ? L"\u23F8" : L"\u25B6");
            InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
            break;

        // Browse list: clicking a row navigates VDJ browser to that file
        case IDC_BROWSE_LIST:
            if (notifCode == LBN_SELCHANGE || notifCode == LBN_DBLCLK)
            {
                int sel = (int) SendMessageW (p->hBrowseList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int) p->browseItems.size())
                {
                    const BrowseItem& bi = p->browseItems[sel];
                    // Navigate VDJ browser via normalized search
                    std::wstring query = normalizeForSearch (bi.title);
                    if (!bi.artist.empty()) query += L" " + normalizeForSearch (bi.artist);
                    p->vdjSend ("search \"" + toUtf8 (query) + "\"");

                    // Immediately update local prelisten waveform (timer will also sync)
                    rebuildPrelistenWaveBins (p->prelistenWaveBins, bi.filePath);
                    p->prelistenWavePath = bi.filePath;
                    p->prelistenPos = 0.0;
                    InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
                }
            }
            break;

        // Browse: Add to End of playlist
        case IDC_BTN_ADD_END:
            p->vdjSend ("playlist_add");
            break;

        // Browse: Add After current in automix
        case IDC_BTN_ADD_AFTER:
            p->vdjSend ("automix_add_next");
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
            // Source toggle: Browser / Deck
            if (di->CtlID == IDC_BTN_SRC_BROWSER)
            {
                drawTextToggle (di, L"Browser", p->sourceMode == 0, p->fontSmall, p->fontBold);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_SRC_DECK)
            {
                bool deckActive = (p->sourceMode != 0);
                drawTextToggle (di, L"Deck", deckActive, p->fontSmall, p->fontBold);
                return TRUE;
            }
            // Deck sub-mode selector
            if (di->CtlID == IDC_BTN_DECK_SEL)
            {
                bool pressed = (di->itemState & ODS_SELECTED) != 0;
                RECT r = di->rcItem;
                fillRect (di->hDC, r, pressed ? TCol::buttonHover : TCol::buttonBg);
                HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldPen = (HPEN) SelectObject (di->hDC, pen);
                HBRUSH oldBr = (HBRUSH) SelectObject (di->hDC, GetStockObject (NULL_BRUSH));
                RoundRect (di->hDC, r.left, r.top, r.right, r.bottom, 4, 4);
                SelectObject (di->hDC, oldPen); SelectObject (di->hDC, oldBr);
                DeleteObject (pen);

                // Label: "Active ▾"
                std::wstring lbl = deckModeLabel (p->lastDeckMode);
                lbl += L" \u25BE";  // ▾
                COLORREF fg = (p->sourceMode != 0) ? TCol::textBright : TCol::textDim;
                drawText (di->hDC, r, lbl, fg, p->fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }

            // Tab buttons
            if (di->CtlID == IDC_BTN_TAB_TRACK)
            {
                drawTextToggle (di, L"Track",   p->activeTab == 0, p->fontSmall, p->fontBold);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_MATCHES)
            {
                drawTextToggle (di, L"Matches", p->activeTab == 1, p->fontSmall, p->fontBold);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_BROWSE)
            {
                drawTextToggle (di, L"Browse",  p->activeTab == 2, p->fontSmall, p->fontBold);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_TAB_SETTINGS)
            {
                // Gear icon — use fontNormal for better glyph rendering at this size
                drawTextToggle (di, L"\u2699",  p->activeTab == 3, p->fontNormal, p->fontBold);
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

            // Prelisten play/pause button
            if (di->CtlID == IDC_BTN_PRELISTEN)
            {
                COLORREF bg  = p->prelistenActive ? TCol::accent    : TCol::buttonBg;
                COLORREF fg  = p->prelistenActive ? TCol::textBright : TCol::accentBrt;
                wchar_t txt[4] = {};
                GetWindowTextW (di->hwndItem, txt, 4);
                drawOwnerButton (di, txt, bg, fg, p->fontNormal);
                return TRUE;
            }

            // Filter toggle buttons
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

            // Generic buttons
            wchar_t text[128] = {};
            GetWindowTextW (di->hwndItem, text, 128);
            std::wstring label (text);

            COLORREF bg = TCol::buttonBg, fg = TCol::textNormal;
            if      (di->CtlID == IDC_BTN_SEARCH_VDJ) { bg = RGB (35, 50, 70);  fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_CLOSE)       { bg = RGB (70, 28, 28);  fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_ADD_END)     { bg = RGB (28, 55, 28);  fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_ADD_AFTER)   { bg = RGB (28, 48, 68);  fg = TCol::textBright; }

            drawOwnerButton (di, label, bg, fg, p->fontSmall);
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
            RECT titleR { tx, r.top + 3, r.right - 4, r.top + 3 + 16 };
            drawText (hdc, titleR, row1, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

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
            RECT titleR { tx, r.top, r.right - 50, r.bottom };
            drawText (hdc, titleR, rec.title, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

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

        // ── Browse list ───────────────────────────────────────────────────────
        if (di->CtlID == IDC_BROWSE_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->browseItems.size()) break;
            const BrowseItem& bi = p->browseItems[di->itemID];

            HDC  hdc  = di->hDC;
            RECT r    = di->rcItem;
            bool sel  = (di->itemState & ODS_SELECTED) != 0;
            bool even = (di->itemID % 2 == 0);

            fillRect (hdc, r, sel ? TCol::matchSel : even ? TCol::card : TCol::panel);

            int tx = r.left + 6;
            // Title
            RECT titleR { tx, r.top, r.right - 4, r.bottom };
            drawText (hdc, titleR, bi.title, TCol::textBright, p->fontSmall,
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
        if (mi->CtlID == IDC_BROWSE_LIST)     { mi->itemHeight = BROWSE_ITEM_H;  return TRUE; }
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
