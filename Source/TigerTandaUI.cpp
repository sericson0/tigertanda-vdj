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

// Draw a flat "tag" badge at (x,y) with given width
static void drawBadge (HDC hdc, int x, int y, int w, int h,
                       const std::wstring& text, COLORREF bg, COLORREF fg, HFONT font)
{
    RECT r { x, y, x + w, y + h };
    fillRect (hdc, r, bg);
    drawText (hdc, r, text, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
                : disabled ? RGB(24, 28, 42)
                           : bgColor;
    fillRect (hdc, r, bg);

    // border
    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN oldPen = (HPEN) SelectObject (hdc, pen);
    MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
    LineTo (hdc, r.right - 1, r.bottom - 1);
    LineTo (hdc, r.right - 1, r.top);
    LineTo (hdc, r.left, r.top);
    LineTo (hdc, r.left, r.bottom - 1);
    SelectObject (hdc, oldPen);
    DeleteObject (pen);

    COLORREF fg = disabled ? TCol::textDim : fgColor;
    drawText (hdc, r, label, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// Draw a segment of the 3-part toggle
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

    // border
    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN oldPen = (HPEN) SelectObject (hdc, pen);
    // Top and bottom always
    MoveToEx (hdc, r.left, r.top, nullptr);
    LineTo (hdc, r.right, r.top);
    MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
    LineTo (hdc, r.right, r.bottom - 1);
    // Left edge
    if (isLeft)
    {
        MoveToEx (hdc, r.left, r.top, nullptr);
        LineTo (hdc, r.left, r.bottom);
    }
    // Right edge
    if (isRight)
    {
        MoveToEx (hdc, r.right - 1, r.top, nullptr);
        LineTo (hdc, r.right - 1, r.bottom);
    }
    // Inner divider on the right side (if not rightmost)
    if (!isRight)
    {
        MoveToEx (hdc, r.right - 1, r.top, nullptr);
        LineTo (hdc, r.right - 1, r.bottom);
    }
    SelectObject (hdc, oldPen);
    DeleteObject (pen);

    drawText (hdc, r, label, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Paint the reference-song card area
// ─────────────────────────────────────────────────────────────────────────────

static void paintRefCard (TigerTandaPlugin* p, HDC hdc)
{
    // The ref card sits in the left panel, below the search controls
    const int labelH = 18;
    int y0 = TOP_H + PAD
           + labelH + PAD / 2         // "IDENTIFY SONG" section label
           + BTN_H  + PAD             // 3-part toggle
           + EDIT_H + PAD             // search row (title + artist + btn)
           + (CAND_ITEM_H * 5)        // candidates list (approx)
           + PAD + labelH + PAD;      // "REFERENCE SONG" section label

    RECT cardR { PAD, y0, PAD + LEFT_W - PAD, y0 + REF_CARD_H };

    if (p->confirmedIdx < 0 || p->confirmedIdx >= (int) p->candidates.size())
    {
        // placeholder
        fillRect (hdc, cardR, TCol::card);
        HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
        HPEN old = (HPEN) SelectObject (hdc, pen);
        RECT rr = cardR;
        MoveToEx (hdc, rr.left, rr.bottom - 1, nullptr);
        LineTo (hdc, rr.right - 1, rr.bottom - 1);
        LineTo (hdc, rr.right - 1, rr.top);
        LineTo (hdc, rr.left, rr.top);
        LineTo (hdc, rr.left, rr.bottom - 1);
        SelectObject (hdc, old);
        DeleteObject (pen);

        RECT tr = { cardR.left + 8, cardR.top, cardR.right - 8, cardR.bottom };
        drawText (hdc, tr, L"Click a candidate below to confirm", TCol::textDim, p->fontSmall,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    const TgRecord& rec = p->candidates[p->confirmedIdx].record;

    fillRect (hdc, cardR, TCol::card);

    // border accent line on the left
    RECT accentBar { cardR.left, cardR.top, cardR.left + 3, cardR.bottom };
    fillRect (hdc, accentBar, TCol::accent);

    int tx = cardR.left + 10;
    int ty = cardR.top + 6;
    int tw = cardR.right - tx - 6;

    // Title (bold white)
    RECT titleR { tx, ty, tx + tw, ty + 18 };
    std::wstring displayTitle = rec.title.empty() ? L"(no title)" : rec.title;
    drawText (hdc, titleR, displayTitle, TCol::textBright, p->fontBold,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    ty += 20;

    // Bandleader (accent)
    RECT blR { tx, ty, tx + tw, ty + 16 };
    drawText (hdc, blR, rec.bandleader, TCol::accentBrt, p->fontNormal,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    ty += 18;

    // Singer . Year . Genre . Grouping (dim text)
    std::wstring detail;
    if (!rec.singer.empty()) detail += rec.singer;
    if (!rec.year.empty())   { if (!detail.empty()) detail += L"  \u00B7  "; detail += rec.year; }
    if (!rec.genre.empty())  { if (!detail.empty()) detail += L"  \u00B7  "; detail += rec.genre; }
    if (!rec.grouping.empty()){ if (!detail.empty()) detail += L"  \u00B7  "; detail += rec.grouping; }

    RECT detR { tx, ty, tx + tw, ty + 14 };
    drawText (hdc, detR, detail, TCol::textDim, p->fontSmall,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: set source mode and update segment button highlights
// ─────────────────────────────────────────────────────────────────────────────

static void updateSourceToggle (TigerTandaPlugin* p, int newMode, HWND hwnd)
{
    p->sourceMode = newMode;
    p->lastSeenTitle.clear();
    p->lastSeenArtist.clear();

    // Force repaint of all 3 segment buttons
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
            p->dialogRequestedOpen = false;
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

        // ── Top bar controls ──────────────────────────────────────────────────
        p->hBtnClose = CreateWindowW (L"BUTTON", L"X",
                                      WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                      DLG_W - 36, (TOP_H - 24) / 2, 26, 24,
                                      hwnd, (HMENU) IDC_BTN_CLOSE, nullptr, nullptr);
        p->hBtnReset = CreateWindowW (L"BUTTON", L"Reset",
                                      WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                      DLG_W - 100, (TOP_H - 24) / 2, 58, 24,
                                      hwnd, (HMENU) IDC_BTN_RESET, nullptr, nullptr);

        // Filters inline in top bar (x starts at 178, checkboxes then year spinner)
        const int chkY = (TOP_H - 18) / 2;
        int cx = 178;

        p->hChkArtist = CreateWindowW (L"BUTTON", L"Artist",
                                       WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                       cx, chkY, 58, 18,
                                       hwnd, (HMENU) IDC_CHK_SAME_ARTIST, nullptr, nullptr);
        SendMessageW (p->hChkArtist, BM_SETCHECK, p->filterSameArtist ? BST_CHECKED : BST_UNCHECKED, 0);
        SetWindowTheme (p->hChkArtist, L"", L"");
        cx += 62;

        p->hChkSinger = CreateWindowW (L"BUTTON", L"Singer",
                                       WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                       cx, chkY, 58, 18,
                                       hwnd, (HMENU) IDC_CHK_SAME_SINGER, nullptr, nullptr);
        SendMessageW (p->hChkSinger, BM_SETCHECK, p->filterSameSinger ? BST_CHECKED : BST_UNCHECKED, 0);
        SetWindowTheme (p->hChkSinger, L"", L"");
        cx += 62;

        p->hChkGrouping = CreateWindowW (L"BUTTON", L"Grouping",
                                         WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                         cx, chkY, 72, 18,
                                         hwnd, (HMENU) IDC_CHK_SAME_GROUPING, nullptr, nullptr);
        SendMessageW (p->hChkGrouping, BM_SETCHECK, p->filterSameGrouping ? BST_CHECKED : BST_UNCHECKED, 0);
        SetWindowTheme (p->hChkGrouping, L"", L"");
        cx += 76;

        p->hChkGenre = CreateWindowW (L"BUTTON", L"Genre",
                                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                      cx, chkY, 58, 18,
                                      hwnd, (HMENU) IDC_CHK_SAME_GENRE, nullptr, nullptr);
        SendMessageW (p->hChkGenre, BM_SETCHECK, p->filterSameGenre ? BST_CHECKED : BST_UNCHECKED, 0);
        SetWindowTheme (p->hChkGenre, L"", L"");
        cx += 62;

        p->hChkOrchestra = CreateWindowW (L"BUTTON", L"Orchestra",
                                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                          cx, chkY, 78, 18,
                                          hwnd, (HMENU) IDC_CHK_SAME_ORCHESTRA, nullptr, nullptr);
        SendMessageW (p->hChkOrchestra, BM_SETCHECK, p->filterSameOrchestra ? BST_CHECKED : BST_UNCHECKED, 0);
        SetWindowTheme (p->hChkOrchestra, L"", L"");
        cx += 82;

        p->hChkLabel = CreateWindowW (L"BUTTON", L"Label",
                                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                      cx, chkY, 52, 18,
                                      hwnd, (HMENU) IDC_CHK_SAME_LABEL, nullptr, nullptr);
        SendMessageW (p->hChkLabel, BM_SETCHECK, p->filterSameLabel ? BST_CHECKED : BST_UNCHECKED, 0);
        SetWindowTheme (p->hChkLabel, L"", L"");
        cx += 56;

        // Year range spinner (label "yr+-" painted at cx, edit at cx+24)
        int spinEditW = 42;
        int spinY = (TOP_H - EDIT_H) / 2;
        p->hEditYearRange = CreateWindowExW (WS_EX_CLIENTEDGE, L"EDIT", L"5",
                                             WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
                                             cx + 24, spinY, spinEditW, EDIT_H,
                                             hwnd, (HMENU) IDC_EDIT_YEAR_RANGE, nullptr, nullptr);
        p->hSpinYear = CreateWindowW (UPDOWN_CLASS, nullptr,
                                      WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT
                                      | UDS_ARROWKEYS | UDS_HOTTRACK,
                                      cx + 24 + spinEditW, spinY, 18, EDIT_H,
                                      hwnd, (HMENU) IDC_SPIN_YEAR_RANGE, nullptr, nullptr);
        SendMessageW (p->hSpinYear, UDM_SETBUDDY,   (WPARAM) p->hEditYearRange, 0);
        SendMessageW (p->hSpinYear, UDM_SETRANGE32, 0, 20);
        SendMessageW (p->hSpinYear, UDM_SETPOS32,   0, p->yearRange);

        // ── Left panel ────────────────────────────────────────────────────────
        const int lx = PAD;
        const int lw = LEFT_W - PAD;
        int ly = TOP_H + PAD;

        // Section label "IDENTIFY SONG" is painted — skip 18px
        ly += 18 + PAD / 2;

        // 3-part segmented source toggle [Browser | Deck (Active) | Deck (Other)]
        {
            int segW = lw / 3;
            int segRemainder = lw - segW * 3;  // distribute extra pixels to last
            p->hBtnSrcBrowser = CreateWindowW (L"BUTTON", L"Browser",
                                               WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                               lx, ly, segW, BTN_H,
                                               hwnd, (HMENU) IDC_BTN_SRC_BROWSER, nullptr, nullptr);
            p->hBtnSrcDeckAct = CreateWindowW (L"BUTTON", L"Deck (Active)",
                                               WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                               lx + segW, ly, segW, BTN_H,
                                               hwnd, (HMENU) IDC_BTN_SRC_DECK_ACT, nullptr, nullptr);
            p->hBtnSrcDeckOth = CreateWindowW (L"BUTTON", L"Deck (Other)",
                                               WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                               lx + segW * 2, ly, segW + segRemainder, BTN_H,
                                               hwnd, (HMENU) IDC_BTN_SRC_DECK_OTH, nullptr, nullptr);
        }
        ly += BTN_H + PAD;

        // Search row: Title edit + Artist edit + magnifying glass button
        {
            int searchBtnW = 30;  // square magnifying glass button
            int gap = 4;
            int editsTotalW = lw - searchBtnW - gap;
            int titleW = editsTotalW * 55 / 100;
            int artistW = editsTotalW - titleW - gap;

            p->hEditTitle = CreateWindowExW (WS_EX_CLIENTEDGE, L"EDIT", L"",
                                             WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                             lx, ly, titleW, EDIT_H,
                                             hwnd, (HMENU) IDC_EDIT_TITLE, nullptr, nullptr);
            SendMessageW (p->hEditTitle, EM_SETCUEBANNER, TRUE, (LPARAM) L"Title...");

            p->hEditArtist = CreateWindowExW (WS_EX_CLIENTEDGE, L"EDIT", L"",
                                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                              lx + titleW + gap, ly, artistW, EDIT_H,
                                              hwnd, (HMENU) IDC_EDIT_ARTIST, nullptr, nullptr);
            SendMessageW (p->hEditArtist, EM_SETCUEBANNER, TRUE, (LPARAM) L"Artist...");

            // Magnifying glass search button (vertically centered with edits)
            p->hBtnSearch = CreateWindowW (L"BUTTON", L"",
                                           WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                           lx + lw - searchBtnW, ly - (BTN_H - EDIT_H) / 2, searchBtnW, BTN_H,
                                           hwnd, (HMENU) IDC_BTN_SEARCH, nullptr, nullptr);
        }
        ly += EDIT_H + PAD;

        // Section label "CANDIDATES" is painted — skip 18px
        ly += 18 + PAD / 2;

        // Candidates list — owner-draw fixed, fills remaining height
        int candListH = DLG_H - ly - PAD;
        p->hCandList = CreateWindowW (L"LISTBOX", nullptr,
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL
                                      | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                      lx, ly, lw, candListH,
                                      hwnd, (HMENU) IDC_CANDIDATES_LIST, nullptr, nullptr);

        // ── Right panel ───────────────────────────────────────────────────────
        const int rx = RIGHT_X;
        const int rw = RIGHT_W;
        int ry = TOP_H + PAD;

        // Section label "SIMILAR SONGS" is painted — skip 18px
        ry += 18 + PAD / 2;

        // "Search in VDJ" button at bottom of right panel
        int vdjBtnH = BTN_H;
        int vdjBtnY = DLG_H - PAD - vdjBtnH;

        p->hBtnSearchVdj = CreateWindowW (L"BUTTON", L"Search in VDJ",
                                          WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                          rx, vdjBtnY, rw, vdjBtnH,
                                          hwnd, (HMENU) IDC_BTN_SEARCH_VDJ, nullptr, nullptr);

        // Results list — owner-draw fixed, fills height above VDJ button
        int resultsH = vdjBtnY - ry - PAD;
        p->hResultsList = CreateWindowW (L"LISTBOX", nullptr,
                                         WS_CHILD | WS_VISIBLE | WS_VSCROLL
                                         | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                         rx, ry, rw, resultsH,
                                         hwnd, (HMENU) IDC_RESULTS_LIST, nullptr, nullptr);

        // ── Apply font to all children ────────────────────────────────────────
        if (p->fontNormal)
        {
            EnumChildWindows (hwnd, [] (HWND child, LPARAM lp) -> BOOL {
                SendMessageW (child, WM_SETFONT, (WPARAM) lp, TRUE);
                return TRUE;
            }, (LPARAM) p->fontNormal);
        }

        // ── Start 250ms polling timer ─────────────────────────────────────────
        SetTimer (hwnd, TIMER_BROWSE_POLL, 250, nullptr);
        return 0;
    }

    // ── Paint ────────────────────────────────────────────────────────────────
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint (hwnd, &ps);

        // Background
        RECT clientR;
        GetClientRect (hwnd, &clientR);
        fillRect (hdc, clientR, TCol::bg);

        // ── Top bar ───────────────────────────────────────────────────────────
        RECT topR { 0, 0, DLG_W, TOP_H };
        fillRect (hdc, topR, TCol::panel);

        // "TigerTanda" title
        RECT titleR { 10, 0, 175, TOP_H };
        if (p) drawText (hdc, titleR, L"TigerTanda", TCol::accentBrt, p->fontTitle,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // "yr+-" label for year spinner
        // cx after 6 checkboxes: 178 + 62 + 62 + 76 + 62 + 82 + 56 = 578
        if (p)
        {
            RECT yrR { 578, 0, 602, TOP_H };
            drawText (hdc, yrR, L"yr\u00B1", TCol::textDim, p->fontSmall,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // ── Left panel background ─────────────────────────────────────────────
        RECT leftPanelR { 0, TOP_H, LEFT_W, DLG_H };
        fillRect (hdc, leftPanelR, TCol::panel);

        // Vertical divider
        RECT divR { LEFT_W, TOP_H, LEFT_W + 1, DLG_H };
        fillRect (hdc, divR, TCol::cardBorder);

        // ── Section labels ────────────────────────────────────────────────────
        if (p)
        {
            // Left panel labels
            {
                int ly = TOP_H + PAD;
                RECT r { PAD, ly, LEFT_W - PAD, ly + 18 };
                drawText (hdc, r, L"IDENTIFY SONG", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                // CANDIDATES label: after section label + toggle + search row
                ly += 18 + PAD / 2 + BTN_H + PAD + EDIT_H + PAD;
                r = { PAD, ly, LEFT_W - PAD, ly + 18 };
                drawText (hdc, r, L"CANDIDATES", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            // Right panel labels
            {
                int ry = TOP_H + PAD;
                RECT r { RIGHT_X, ry, RIGHT_X + RIGHT_W, ry + 18 };
                drawText (hdc, r, L"SIMILAR SONGS", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }

        EndPaint (hwnd, &ps);
        return 0;
    }

    // ── Timer: browser/deck polling ──────────────────────────────────────────
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
            // Determine deck index: Active (1) or Other (2)
            int activeDeck = (int) p->vdjGetValue ("get_active_deck");
            int deckIdx = (p->sourceMode == 1) ? activeDeck : (1 - activeDeck);
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

            // Auto-populate edit boxes
            if (p->hEditTitle)  SetWindowTextW (p->hEditTitle,  newTitle.c_str());
            if (p->hEditArtist) SetWindowTextW (p->hEditArtist, newArtist.c_str());

            // Auto-run identification
            p->runIdentification (newTitle, newArtist);
        }
        return 0;
    }

    // ── Commands ─────────────────────────────────────────────────────────────
    case WM_COMMAND:
    {
        if (!p) break;
        int ctrlId   = LOWORD (wParam);
        int notifCode = HIWORD (wParam);

        switch (ctrlId)
        {
        // 3-part segmented toggle
        case IDC_BTN_SRC_BROWSER:
            updateSourceToggle (p, 0, hwnd);
            break;
        case IDC_BTN_SRC_DECK_ACT:
            updateSourceToggle (p, 1, hwnd);
            break;
        case IDC_BTN_SRC_DECK_OTH:
            updateSourceToggle (p, 2, hwnd);
            break;

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
                    p->confirmCandidate (sel);
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
            // Search for selected result in VDJ
            int sel = (int) SendMessageW (p->hResultsList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int) p->results.size())
            {
                const TgRecord& rec = p->results[sel];
                std::wstring query = rec.title;
                if (!rec.bandleader.empty())
                    query += L" " + rec.bandleader;
                std::string cmd = "search \"" + toUtf8 (query) + "\"";
                p->vdjSend (cmd);
            }
            break;
        }

        case IDC_BTN_RESET:
            p->resetAll();
            break;

        case IDC_BTN_CLOSE:
            p->dialogRequestedOpen = false;
            p->vdjSend ("effect_show_gui off");
            p->suppressNextHideSync = true;
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
            // 3-part segmented toggle buttons
            if (di->CtlID == IDC_BTN_SRC_BROWSER)
            {
                drawSegmentButton (di, L"Browser", p->sourceMode == 0, p->fontSmall, true, false);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_SRC_DECK_ACT)
            {
                drawSegmentButton (di, L"Deck (Active)", p->sourceMode == 1, p->fontSmall, false, false);
                return TRUE;
            }
            if (di->CtlID == IDC_BTN_SRC_DECK_OTH)
            {
                drawSegmentButton (di, L"Deck (Other)", p->sourceMode == 2, p->fontSmall, false, true);
                return TRUE;
            }

            wchar_t text[128] = {};
            GetWindowTextW (di->hwndItem, text, 128);
            std::wstring label (text);

            COLORREF bg = TCol::buttonBg;
            COLORREF fg = TCol::textNormal;

            if (di->CtlID == IDC_BTN_SEARCH)
            {
                // Magnifying glass button - draw custom
                bg = RGB(35, 50, 70); fg = TCol::textBright;
                bool pressed = (di->itemState & ODS_SELECTED) != 0;
                COLORREF drawBg = pressed ? TCol::buttonHover : bg;
                fillRect (di->hDC, di->rcItem, drawBg);

                // Draw border
                HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldPen = (HPEN) SelectObject (di->hDC, pen);
                RECT r = di->rcItem;
                MoveToEx (di->hDC, r.left, r.bottom - 1, nullptr);
                LineTo (di->hDC, r.right - 1, r.bottom - 1);
                LineTo (di->hDC, r.right - 1, r.top);
                LineTo (di->hDC, r.left, r.top);
                LineTo (di->hDC, r.left, r.bottom - 1);
                SelectObject (di->hDC, oldPen);
                DeleteObject (pen);

                // Draw magnifying glass using Unicode U+2315 or simple circle+line via GDI
                // Use a simple circle + line approach
                int cx = (di->rcItem.left + di->rcItem.right) / 2;
                int cy = (di->rcItem.top + di->rcItem.bottom) / 2;
                int radius = 5;

                HPEN glassPen = CreatePen (PS_SOLID, 2, fg);
                HPEN oldP = (HPEN) SelectObject (di->hDC, glassPen);
                HBRUSH oldBr = (HBRUSH) SelectObject (di->hDC, GetStockObject (NULL_BRUSH));
                Ellipse (di->hDC, cx - radius - 2, cy - radius - 2, cx + radius - 2, cy + radius - 2);
                // Handle (line from bottom-right of circle)
                MoveToEx (di->hDC, cx + radius - 4, cy + radius - 4, nullptr);
                LineTo (di->hDC, cx + radius + 2, cy + radius + 2);
                SelectObject (di->hDC, oldP);
                SelectObject (di->hDC, oldBr);
                DeleteObject (glassPen);

                return TRUE;
            }
            else if (di->CtlID == IDC_BTN_SEARCH_VDJ)
                { bg = RGB(35, 50, 70); fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_RESET)
                { bg = RGB(60, 28, 28); fg = TCol::textNormal; }
            else if (di->CtlID == IDC_BTN_CLOSE)
                { bg = RGB(70, 28, 28); fg = TCol::textBright; }

            drawOwnerButton (di, label, bg, fg, p->fontNormal);
            return TRUE;
        }

        // ── Owner-draw candidates list ────────────────────────────────────────
        if (di->CtlID == IDC_CANDIDATES_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->candidates.size()) break;
            const TgMatchResult& mr = p->candidates[di->itemID];
            const TgRecord& rec = mr.record;

            HDC hdc = di->hDC;
            RECT r = di->rcItem;
            bool sel = (di->itemState & ODS_SELECTED) != 0;
            bool confirmed = ((int) di->itemID == p->confirmedIdx);

            COLORREF itemBg = confirmed ? TCol::matchSel
                            : sel       ? TCol::matchSel
                                       : TCol::panel;
            fillRect (hdc, r, itemBg);

            // Score badge (40x14)
            wchar_t scoreBuf[16];
            swprintf_s (scoreBuf, L"%.0f%%", mr.score);
            int badgeW = 40, badgeH = 14;
            int bx = r.left + 4;
            int by = r.top + (CAND_ITEM_H - badgeH) / 2;
            drawBadge (hdc, bx, by, badgeW, badgeH, scoreBuf,
                       TCol::scoreBg (mr.score), TCol::scoreColor (mr.score), p->fontSmall);

            // Title (bold)
            int tx = bx + badgeW + 6;
            RECT titleR { tx, r.top + 4, r.right - 4, r.top + 18 };
            drawText (hdc, titleR, rec.title, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Detail: Bandleader . Singer . Year
            std::wstring detail = rec.bandleader;
            if (!rec.singer.empty()) detail += L"  \u00B7  " + rec.singer;
            if (!rec.year.empty())   detail += L"  \u00B7  " + rec.year;
            RECT detR { tx, r.top + 19, r.right - 4, r.bottom - 3 };
            drawText (hdc, detR, detail, TCol::textDim, p->fontSmall,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Confirmed marker
            if (confirmed)
            {
                RECT markR { r.right - 20, r.top, r.right, r.bottom };
                drawText (hdc, markR, L"\u2714", TCol::good, p->fontBold,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Bottom separator
            HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, pen);
            MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
            LineTo   (hdc, r.right, r.bottom - 1);
            SelectObject (hdc, old);
            DeleteObject (pen);

            return TRUE;
        }

        // ── Owner-draw results list (no row numbers) ─────────────────────────
        if (di->CtlID == IDC_RESULTS_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->results.size()) break;
            const TgRecord& rec = p->results[di->itemID];

            HDC hdc = di->hDC;
            RECT r = di->rcItem;
            bool sel = (di->itemState & ODS_SELECTED) != 0;
            bool even = (di->itemID % 2 == 0);

            COLORREF itemBg = sel  ? TCol::matchSel
                            : even ? TCol::card
                                   : TCol::panel;
            fillRect (hdc, r, itemBg);

            int tx = r.left + 6;
            int availW = r.right - tx - 4;

            // Measure title width with fontBold (cap at 55% of available)
            RECT tm = { 0, 0, 9999, 100 };
            HFONT oldFont = (HFONT) SelectObject (hdc, p->fontBold);
            DrawTextW (hdc, rec.title.c_str(), -1, &tm, DT_CALCRECT | DT_SINGLELINE);
            SelectObject (hdc, oldFont);
            int titleW = (int) (tm.right - tm.left);
            int maxTitleW = availW * 55 / 100;
            if (titleW > maxTitleW) titleW = maxTitleW;

            // Title (bold, left-aligned)
            RECT titleR { tx, r.top, tx + titleW, r.bottom };
            drawText (hdc, titleR, rec.title, TCol::textBright, p->fontBold,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Metadata on same line: Bandleader . Singer . Genre . Year
            int metaX = tx + titleW + 12;
            std::wstring meta;
            if (!rec.bandleader.empty()) meta += rec.bandleader;
            if (!rec.singer.empty())     { if (!meta.empty()) meta += L"  \u00B7  "; meta += rec.singer; }
            if (!rec.genre.empty())      { if (!meta.empty()) meta += L"  \u00B7  "; meta += rec.genre; }
            if (!rec.year.empty())       { if (!meta.empty()) meta += L"  \u00B7  "; meta += rec.year; }

            RECT metaR { metaX, r.top, r.right - 4, r.bottom };
            drawText (hdc, metaR, meta, TCol::textDim, p->fontSmall,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Bottom separator
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
        if (mi->CtlID == IDC_CANDIDATES_LIST) { mi->itemHeight = CAND_ITEM_H; return TRUE; }
        if (mi->CtlID == IDC_RESULTS_LIST)    { mi->itemHeight = RESULT_ITEM_H; return TRUE; }
        break;
    }

    // ── Updown spinner (year range) ──────────────────────────────────────────
    case WM_NOTIFY:
    {
        auto* nm = reinterpret_cast<NMHDR*> (lParam);
        if (!nm || !p) break;
        if (nm->idFrom == IDC_SPIN_YEAR_RANGE && nm->code == UDN_DELTAPOS)
        {
            auto* ud = reinterpret_cast<NMUPDOWN*> (lParam);
            int newVal = ud->iPos + ud->iDelta;
            if (newVal < 0)  newVal = 0;
            if (newVal > 20) newVal = 20;
            p->yearRange = newVal;
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
        }
        break;
    }

    // ── Colour checkboxes / buttons ──────────────────────────────────────────
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC) wParam;
        SetBkColor (hdc, TCol::panel);
        SetTextColor (hdc, TCol::textNormal);
        return (LRESULT) (p ? p->panelBrush : GetStockObject (NULL_BRUSH));
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC) wParam;
        SetBkColor (hdc, TCol::card);
        SetTextColor (hdc, TCol::textBright);
        return (LRESULT) (p ? p->cardBrush : GetStockObject (NULL_BRUSH));
    }

    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = (HDC) wParam;
        SetBkColor (hdc, TCol::panel);
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
//  UI helper implementations (called from matching code)
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::repaintRefCard()
{
    if (!hDlg) return;
    // Invalidate the lower portion of the left panel where the ref card lives
    RECT r { 0, TOP_H + 200, LEFT_W, DLG_H };
    InvalidateRect (hDlg, &r, FALSE);
}

void TigerTandaPlugin::repaintTopBar()
{
    if (!hDlg) return;
    RECT r { 0, 0, DLG_W, TOP_H };
    InvalidateRect (hDlg, &r, FALSE);
}
