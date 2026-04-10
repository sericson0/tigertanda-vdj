//==============================================================================
// TigerTanda VDJ Plugin - Main Window UI
// TandaWndProc: WM_CREATE, WM_PAINT, WM_TIMER, WM_COMMAND, WM_DRAWITEM
//==============================================================================

#include "TigerTanda.h"
#include "CoverArt.h"
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

// Extract the last whitespace-separated token from a name
// "Pedro Laurenz" -> "Laurenz", "Héctor Farrel" -> "Farrel"
static std::wstring lastName (const std::wstring& fullName)
{
    if (fullName.empty()) return fullName;
    size_t pos = fullName.find_last_of (L" \t");
    return (pos == std::wstring::npos) ? fullName : fullName.substr (pos + 1);
}

// Format artist for list rows: "BandleaderLast - SingerLast" (or just bandleader last if no singer)
static std::wstring formatArtist (const TgRecord& rec)
{
    std::wstring out = lastName (rec.bandleader);
    if (!rec.singer.empty())
    {
        std::wstring s = lastName (rec.singer);
        if (!s.empty())
        {
            if (!out.empty()) out += L" - ";
            out += s;
        }
    }
    return out;
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
// Row 1: Title (bold, bright, larger)
// Row 2: Bandleader · Singer
// Row 3: Date (YYYY-mm-dd) · Genre
// Row 4: Label: <label>
// Row 5: Group: <grouping>
static void drawResultDetailBox (HDC hdc, RECT r, const TgRecord& rec,
                                 HFONT fontTitle, HFONT fontBody, HFONT /*fontSmall*/)
{
    fillRect (hdc, r, TCol::card);

    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN old = (HPEN) SelectObject (hdc, pen);
    MoveToEx (hdc, r.left,  r.top, nullptr);
    LineTo   (hdc, r.right, r.top);
    SelectObject (hdc, old);
    DeleteObject (pen);

    int px = r.left + 8;
    int py = r.top + 5;
    const int titleH = 22;
    const int lineH  = 17;

    // Row 1: Track title (prominent)
    RECT rTitle { px, py, r.right - 8, py + titleH };
    drawText (hdc, rTitle, rec.title, TCol::textBright, fontTitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    py += titleH + 1;

    // Row 2: Bandleader · Singer
    std::wstring line2 = rec.bandleader;
    if (!rec.singer.empty()) { if (!line2.empty()) line2 += L"  \u00B7  "; line2 += rec.singer; }
    RECT r2 { px, py, r.right - 8, py + lineH };
    drawText (hdc, r2, line2, TCol::textNormal, fontBody,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    py += lineH + 1;

    // Row 3: Date · Genre
    std::wstring dateStr = formatDateYMD (rec.date);
    std::wstring line3;
    if (!dateStr.empty())   line3 += dateStr;
    if (!rec.genre.empty()) { if (!line3.empty()) line3 += L"  \u00B7  "; line3 += rec.genre; }
    RECT r3 { px, py, r.right - 8, py + lineH };
    drawText (hdc, r3, line3, TCol::textDim, fontBody,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    py += lineH + 1;

    // Row 4: Label
    if (!rec.label.empty())
    {
        std::wstring line4 = L"Label: " + rec.label;
        RECT r4 { px, py, r.right - 8, py + lineH };
        drawText (hdc, r4, line4, TCol::textDim, fontBody,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    py += lineH + 1;

    // Row 5: Group
    if (!rec.grouping.empty())
    {
        std::wstring line5 = L"Group: " + rec.grouping;
        RECT r5 { px, py, r.right - 8, py + lineH };
        drawText (hdc, r5, line5, TCol::textDim, fontBody,
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

    const int leftW  = DLG_W * LEFT_COL_PCT / 100;
    const int rightW = DLG_W - leftW;

    const bool showMain = (p->activeTab == 0);
    const bool showS    = (p->activeTab == 1);

    auto showCtrl = [](HWND h, bool vis)
    {
        if (h) ShowWindow (h, vis ? SW_SHOW : SW_HIDE);
    };

    // Top bar: settings gear + close (always visible)
    const int topY = (TOP_H - TAB_BTN_H) / 2;
    MoveWindow (p->hBtnClose, DLG_W - 26, topY, 22, TAB_BTN_H, FALSE);
    ShowWindow (p->hBtnClose, SW_SHOW);
    const int toggleW = 140;
    MoveWindow (p->hBtnTabSettings, DLG_W - 26 - PAD - toggleW, topY, toggleW, TAB_BTN_H, FALSE);
    ShowWindow (p->hBtnTabSettings, SW_SHOW);

    // Main view (activeTab == 0)
    if (showMain)
    {
        const int lx = PAD;
        const int lw = leftW - PAD * 2;
        // Reserve scrollbar width on right so inputs/candidates align with scrolled results list items
        const int sbW = GetSystemMetrics (SM_CXVSCROLL);
        const int usableW = lw - sbW;

        // Search inputs — below column headers (headers are painted, 14px)
        int ly = TOP_H + PAD + 14;
        const int gap = 4;
        const int yearW = 52;
        const int titleW = (usableW - yearW - gap * 2) * 55 / 100;
        const int artistW = usableW - titleW - yearW - gap * 2;
        MoveWindow (p->hEditTitle,  lx,                       ly, titleW,  EDIT_H, FALSE);
        MoveWindow (p->hEditArtist, lx + titleW + gap,        ly, artistW, EDIT_H, FALSE);
        MoveWindow (p->hEditYear,   lx + usableW - yearW,     ly, yearW,   EDIT_H, FALSE);
        ly += EDIT_H + TRACK_SEARCH_GAP;

        // Candidates list (scrollable, shows 2 rows at a time) — use full lw
        // so its scrollbar eats sbW on the right, aligning item columns with
        // the input row above it.
        int candH = CAND_ITEM_H * 2 + 2;
        MoveWindow (p->hCandList, lx, ly, lw, candH, FALSE);
        ly += candH + 6;  // small gap before matches header

        // "MATCHES (N)" header painted, 14px
        int matchListTop = ly + 14;
        int matchListBot = DLG_H - PAD;
        MoveWindow (p->hResultsList, lx, matchListTop, lw, matchListBot - matchListTop, FALSE);

        // Right column
        const int rx = leftW + PAD;
        const int rw = rightW - PAD * 2;

        // "SELECTED TRACK" header painted, 20px (18px text + 2px bottom gap)
        int ry = TOP_H + PAD + 20;

        // Detail box position (painted in WM_PAINT, not a control)
        int detailBot = ry + DETAIL_BOX_H;

        // "VDJ BROWSER RESULTS" header with FIND IN VDJ button right-aligned.
        // Button is 18px tall to fit nicely with the painted header text.
        const int findBtnW = 90;
        const int findBtnH = 18;
        int headerRowY = detailBot + 4;
        MoveWindow (p->hBtnFindInVdj, rx + rw - findBtnW, headerRowY,
                    findBtnW, findBtnH, FALSE);

        int browseTop = headerRowY + findBtnH + 4;
        // Browse list is exactly 4 items tall — sized to avoid scrollbar.
        int browseH = BROWSE_ITEM_H * 4 + 2;
        int preRowY = DLG_H - PAD - BTN_H;
        MoveWindow (p->hBrowseList, rx, browseTop, rw, browseH, FALSE);

        // Prelisten + ADD row
        const int preBtnW = 28;
        const int addBtnW = 72;
        MoveWindow (p->hBtnPrelisten, rx, preRowY, preBtnW, BTN_H, FALSE);
        MoveWindow (p->hBtnAddEnd, rx + rw - addBtnW, preRowY, addBtnW, BTN_H, FALSE);

        p->prelistenWaveRect = { rx + preBtnW + 4, preRowY,
                                 rx + rw - addBtnW - 4, preRowY + BTN_H };
    }
    showCtrl (p->hEditTitle,     showMain);
    showCtrl (p->hEditArtist,    showMain);
    showCtrl (p->hEditYear,      showMain);
    showCtrl (p->hCandList,      showMain);
    showCtrl (p->hResultsList,   showMain);
    showCtrl (p->hBtnPrelisten,  showMain);
    showCtrl (p->hBtnAddEnd,     showMain);
    showCtrl (p->hBtnFindInVdj,  showMain);
    // Browse list is shown only when it has items; otherwise the main
    // window paints a placeholder in its place.
    if (p->hBrowseList)
        ShowWindow (p->hBrowseList,
                    (showMain && !p->browseItems.empty()) ? SW_SHOW : SW_HIDE);

    // Settings view (activeTab == 1)
    if (showS)
    {
        const int lx = PAD;
        const int lw = DLG_W - PAD * 2;
        const int btnH = BTN_H - 4;
        const int gap  = 4;
        const int colW = (lw - gap * 2) / 3;

        int sy = TOP_H + PAD - 5;

        const int howTabW = lw / 5;
        for (int i = 0; i < 5; ++i)
            MoveWindow (p->hBtnHowTabs[i], lx + i * howTabW, sy, howTabW, 18, FALSE);
        sy += 18 + 4;
        sy += 60 + 13;

        MoveWindow (p->hChkArtist,    lx,                    sy, colW, btnH, FALSE);
        MoveWindow (p->hChkSinger,    lx + colW + gap,       sy, colW, btnH, FALSE);
        MoveWindow (p->hChkGenre,     lx + colW * 2 + gap*2, sy, lw - colW*2 - gap*2, btnH, FALSE);
        sy += btnH + 4;

        MoveWindow (p->hChkGrouping,  lx,                    sy, colW, btnH, FALSE);
        MoveWindow (p->hChkLabel,     lx + colW + gap,       sy, colW, btnH, FALSE);
        MoveWindow (p->hChkOrchestra, lx + colW * 2 + gap*2, sy, lw - colW*2 - gap*2, btnH, FALSE);
        sy += btnH + 4;

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
                p->hBtnClose, p->hBtnPrelisten,
                p->hBtnAddEnd, p->hBtnFindInVdj, p->hBtnYearToggle, p->hBtnYearRange,
                p->hChkArtist, p->hChkSinger,
                p->hChkGrouping, p->hChkGenre, p->hChkOrchestra, p->hChkLabel,
                p->hChkTrack,
                p->hBtnTabSettings,
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

        p->hEditYear = CreateWindowExW (WS_EX_CLIENTEDGE, L"EDIT", L"",
                                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER,
                                        0, 0, 10, 10, hwnd,
                                        (HMENU) IDC_EDIT_YEAR, nullptr, nullptr);
        SendMessageW (p->hEditYear, EM_SETCUEBANNER, TRUE, (LPARAM) L"Year");

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

        // Candidates list — scrollable, shows 2 rows at a time
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

        // Browse list — exactly 4 items, no scrollbar (height is sized to fit)
        p->hBrowseList = CreateWindowW (L"LISTBOX", nullptr,
                                        WS_CHILD | WS_VISIBLE
                                        | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS
                                        | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                        0, 0, 10, 10, hwnd,
                                        (HMENU) IDC_BROWSE_LIST, nullptr, nullptr);

        // Prelisten + action buttons
        p->hBtnPrelisten = mkBtn (IDC_BTN_PRELISTEN, L"\u25B6");  // ▶
        p->hBtnAddEnd    = mkBtn (IDC_BTN_ADD_END,   L"ADD");
        p->hBtnFindInVdj = mkBtn (IDC_BTN_FIND_IN_VDJ, L"FIND IN VDJ");

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
            addTip (p->hBtnPrelisten,    L"Preview selected song");
            addTip (p->hBtnAddEnd,       L"Add selected browse result to automix bottom");
            addTip (p->hBtnFindInVdj,    L"Search VDJ library for the selected tanda match");
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

        // "Tiger Tanda" brand in top bar
        RECT brandR { PAD, 0, DLG_W / 2, TOP_H };
        drawText (hdc, brandR, L"Tiger Tanda", TCol::accent, p->fontTitle,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT bodyR { 0, TOP_H, DLG_W, DLG_H };
        fillRect (hdc, bodyR, TCol::bg);

        // Main view
        if (p->activeTab == 0)
        {
            const int leftW = DLG_W * LEFT_COL_PCT / 100;
            const int lx = PAD;
            const int lw = leftW - PAD * 2;
            const int rx = leftW + PAD;
            const int rw = DLG_W - leftW - PAD * 2;

            // Left column headers — aligned with usable width (reserving scrollbar space)
            const int sbW = GetSystemMetrics (SM_CXVSCROLL);
            const int usableW = lw - sbW;
            int headerY = TOP_H + PAD;
            const int gap = 4;
            const int yearW = 52;
            const int titleW = (usableW - yearW - gap * 2) * 55 / 100;
            const int artistW = usableW - titleW - yearW - gap * 2;
            RECT htR { lx, headerY, lx + titleW, headerY + 12 };
            RECT haR { lx + titleW + gap, headerY, lx + titleW + gap + artistW, headerY + 12 };
            RECT hyR { lx + usableW - yearW, headerY, lx + usableW, headerY + 12 };
            drawText (hdc, htR, L"TITLE",  TCol::textDim, p->fontSmall, DT_LEFT | DT_TOP | DT_SINGLELINE);
            drawText (hdc, haR, L"ARTIST", TCol::textDim, p->fontSmall, DT_LEFT | DT_TOP | DT_SINGLELINE);
            drawText (hdc, hyR, L"YEAR",   TCol::textDim, p->fontSmall, DT_CENTER | DT_TOP | DT_SINGLELINE);

            // "MATCHES (N)" header (no separator line above it)
            int candBot = TOP_H + PAD + 14 + EDIT_H + TRACK_SEARCH_GAP + CAND_ITEM_H * 2 + 2;
            int matchHeaderY = candBot + 4;
            std::wstring matchLabel = L"MATCHES (" + std::to_wstring (p->results.size()) + L")";
            RECT mhR { lx, matchHeaderY, lx + lw, matchHeaderY + 12 };
            drawText (hdc, mhR, matchLabel, TCol::textDim, p->fontSmall, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // Right column: "SELECTED TRACK" header — larger, bolder
            int rHeaderY = TOP_H + PAD;
            RECT stR { rx, rHeaderY, rx + rw, rHeaderY + 18 };
            drawText (hdc, stR, L"SELECTED TRACK", TCol::textNormal, p->fontBold,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Detail box
            int detailY = rHeaderY + 20;
            RECT detR { rx, detailY, rx + rw, detailY + DETAIL_BOX_H };
            if (p->selectedResultIdx >= 0 && p->selectedResultIdx < (int) p->results.size())
                drawResultDetailBox (hdc, detR, p->results[p->selectedResultIdx],
                                     p->fontDetail, p->fontNormal, p->fontSmall);
            else
            {
                fillRect (hdc, detR, TCol::card);
                HPEN pen2 = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN old2 = (HPEN) SelectObject (hdc, pen2);
                MoveToEx (hdc, detR.left, detR.top, nullptr);
                LineTo   (hdc, detR.right, detR.top);
                SelectObject (hdc, old2);
                DeleteObject (pen2);
                RECT txtR { detR.left + 6, detR.top + 4, detR.right - 6, detR.bottom - 4 };
                drawText (hdc, txtR, L"Select a match to see details", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            // "VDJ BROWSER RESULTS" header — vertically centered in the 18px
            // button row next to FIND IN VDJ. Leave room on the right so the
            // label doesn't overlap the button.
            const int findBtnW2 = 90;
            int browseHeaderY = detailY + DETAIL_BOX_H + 4;
            RECT bhR { rx, browseHeaderY, rx + rw - findBtnW2 - 6, browseHeaderY + 18 };
            drawText (hdc, bhR, L"VDJ BROWSER RESULTS", TCol::textDim, p->fontSmall,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // When browseItems is empty the listbox is hidden via
            // syncBrowseListVisibility, leaving the main window background
            // in its place — draw a placeholder string there so the user
            // knows what state they're in.
            if (p->browseItems.empty() && p->hBrowseList)
            {
                RECT blR;
                GetWindowRect (p->hBrowseList, &blR);
                POINT tl = { blR.left, blR.top };
                POINT br = { blR.right, blR.bottom };
                ScreenToClient (hwnd, &tl);
                ScreenToClient (hwnd, &br);
                RECT placeholder { tl.x, tl.y, br.x, br.y };

                // Border so the placeholder area is visually distinct
                fillRect (hdc, placeholder, TCol::panel);
                HPEN pp = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldPp = (HPEN) SelectObject (hdc, pp);
                HBRUSH oldB = (HBRUSH) SelectObject (hdc, GetStockObject (NULL_BRUSH));
                Rectangle (hdc, placeholder.left, placeholder.top,
                           placeholder.right, placeholder.bottom);
                SelectObject (hdc, oldPp);
                SelectObject (hdc, oldB);
                DeleteObject (pp);

                const wchar_t* msg = p->smartSearchPending
                    ? L"Searching VDJ library\u2026"
                    : (p->selectedResultIdx >= 0
                       && p->selectedResultIdx < (int) p->results.size()
                        ? L"Click FIND IN VDJ to search for this match"
                        : L"Select a match, then click FIND IN VDJ");
                drawText (hdc, placeholder, msg, TCol::textDim, p->fontSmall,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Prelisten waveform
            RECT wr = p->prelistenWaveRect;
            if (wr.right > wr.left)
                drawPrelistenWave (hdc, wr, p->prelistenWaveBins, p->prelistenPos);
        }

        // Settings view
        if (p->activeTab == 1)
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

        // ── Search debounce timer (live search from edit fields) ──────────────
        if (wParam == TIMER_SEARCH_DEBOUNCE)
        {
            KillTimer (hwnd, TIMER_SEARCH_DEBOUNCE);
            // If a browser search is in flight, postpone — running
            // identification now would clobber smart-search state mid-cycle.
            if (p->smartSearchPending)
            {
                SetTimer (hwnd, TIMER_SEARCH_DEBOUNCE, 200, nullptr);
                return 0;
            }
            wchar_t title[512] = {}, artist[512] = {};
            GetWindowTextW (p->hEditTitle,  title,  512);
            GetWindowTextW (p->hEditArtist, artist, 512);
            p->runIdentification (title, artist);
            return 0;
        }

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

        // When the dialog is closed (or hidden), do no more work. No polling,
        // no identification, no VDJ commands. This prevents the plugin from
        // disturbing the user's browser after they've closed the GUI.
        if (!p->dialogRequestedOpen || !IsWindowVisible (hwnd))
            return 0;

        // ── Poll song from browser ────────────────────────────────────────────
        // Only react to VDJ browse changes when the user is actually browsing
        // a real folder. When curFolder is empty, VDJ is showing search results
        // — likely results we triggered ourselves via smart search — and
        // reading get_browsed_song would cause an identify loop:
        //   browse → auto-confirm → auto-select match → vdjSend("search") →
        //   VDJ parks on a search hit → polling reads that hit as "new song" →
        //   re-identify → search again → ...
        // Gate on curFolder + smartSearchPending to break the cycle.
        std::wstring pollFolder = p->vdjGetString ("get_browsed_folder_path");
        if (!pollFolder.empty() && !p->smartSearchPending)
        {
            std::wstring newTitle  = p->vdjGetString ("get_browsed_song 'title'");
            std::wstring newArtist = p->vdjGetString ("get_browsed_song 'artist'");
            std::wstring newYear   = p->vdjGetString ("get_browsed_song 'year'");

            if (!newTitle.empty()
                && (newTitle != p->lastSeenTitle || newArtist != p->lastSeenArtist))
            {
                p->lastSeenTitle  = newTitle;
                p->lastSeenArtist = newArtist;

                // Suppress EN_CHANGE during programmatic text updates so the
                // debounce timer doesn't fire a duplicate identification.
                p->suppressEditChange = true;
                if (p->hEditTitle)  SetWindowTextW (p->hEditTitle,  newTitle.c_str());
                if (p->hEditArtist) SetWindowTextW (p->hEditArtist, newArtist.c_str());
                if (p->hEditYear)   SetWindowTextW (p->hEditYear,   newYear.c_str());
                p->suppressEditChange = false;

                p->runIdentification (newTitle, newArtist);
            }
        }

        // ── Poll prelisten filepath for waveform ──────────────────────────────
        std::wstring browsePath = p->vdjGetString ("get_browsed_filepath");
        if (!browsePath.empty() && browsePath != p->lastSeenBrowsePath)
        {
            p->lastSeenBrowsePath = browsePath;
            rebuildPrelistenWaveBins (p->prelistenWaveBins, browsePath);
            if (p->activeTab == 0)
                InvalidateRect (hwnd, &p->prelistenWaveRect, FALSE);
        }

        // Note: the browse list is populated exclusively by runSmartSearch
        // now. We no longer enumerate the user's current VDJ folder into
        // the browse list — it's reserved for ranked search hits.

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
        // Settings toggle
        case IDC_BTN_TAB_SETTINGS:
            p->activeTab = (p->activeTab == 1) ? 0 : 1;
            p->saveSettings();
            applyLayout (p, hwnd);
            break;

        // Live search from edit fields
        case IDC_EDIT_TITLE:
        case IDC_EDIT_ARTIST:
        case IDC_EDIT_YEAR:
            // Ignore programmatic updates from polling — only debounce real
            // keystrokes. Without this, every polling-driven SetWindowTextW
            // would schedule a duplicate runIdentification 300ms later.
            if (notifCode == EN_CHANGE && !p->suppressEditChange)
                SetTimer (hwnd, TIMER_SEARCH_DEBOUNCE, 300, nullptr);
            break;

        // Candidate selection
        case IDC_CANDIDATES_LIST:
            if (notifCode == LBN_SELCHANGE || notifCode == LBN_DBLCLK)
            {
                int sel = (int) SendMessageW (p->hCandList, LB_GETCURSEL, 0, 0);
                if (sel >= 0)
                    p->confirmCandidate (sel);
            }
            break;

        // Result list selection — visual only, no VDJ round trip.
        // User must click "Find in VDJ" to actually search the library.
        case IDC_RESULTS_LIST:
            if (notifCode == LBN_SELCHANGE)
            {
                int sel = (int) SendMessageW (p->hResultsList, LB_GETCURSEL, 0, 0);
                p->selectedResultIdx = sel;
                if (p->hResultsList) InvalidateRect (p->hResultsList, nullptr, FALSE);
                if (p->hDlg)         InvalidateRect (p->hDlg,         nullptr, FALSE);

                // Bump the smart-search token so any in-flight runSmartSearch
                // will discard its results when it reaches the token check.
                p->smartSearchActiveToken = ++p->smartSearchToken;

                // Clear stale browse results from the previous match.
                if (!p->browseItems.empty())
                {
                    p->browseItems.clear();
                    p->selectedBrowseIdx = -1;
                    if (p->hBrowseList)
                        SendMessageW (p->hBrowseList, LB_RESETCONTENT, 0, 0);
                }
                p->syncBrowseListVisibility();
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

        // Browse list: clicking a row only updates prelisten + selection highlight.
        // We intentionally do NOT move VDJ's browser — the user's cursor stays
        // on whatever song they were originally on. If they want to actually
        // load one of the browse results, that's what the ADD button is for
        // (and we could extend this later to send a "browse to file" command
        // if VDJ exposes one).
        case IDC_BROWSE_LIST:
            if (notifCode == LBN_SELCHANGE || notifCode == LBN_DBLCLK)
            {
                int sel = (int) SendMessageW (p->hBrowseList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int) p->browseItems.size())
                {
                    p->selectedBrowseIdx = sel;
                    InvalidateRect (p->hBrowseList, nullptr, FALSE);

                    // Update local prelisten waveform if path available
                    const BrowseItem& bi = p->browseItems[sel];
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

        // Browse: add the selected browse result to the automix bottom.
        // Re-issues the last browser search, scrolls to the saved index,
        // sends playlist_add, then restores the user's folder.
        case IDC_BTN_ADD_END:
            p->addSelectedBrowseToAutomix();
            break;

        // Find in VDJ — searches library for the currently selected tanda
        // match, populates browse list with top 4 ranked results, restores
        // the user's folder afterwards.
        case IDC_BTN_FIND_IN_VDJ:
        {
            if (p->smartSearchPending) break;  // already searching
            int sel = p->selectedResultIdx;
            if (sel >= 0 && sel < (int) p->results.size())
            {
                // Force re-fire even if target matches last search — the user
                // explicitly asked for it (e.g., they cleared the list earlier
                // or want to retry).
                p->lastSmartSearchTitle.clear();
                p->lastSmartSearchArtist.clear();
                p->triggerBrowserSearch (p->results[sel]);
                // Repaint the button so its "pending" state shows immediately
                if (p->hBtnFindInVdj) InvalidateRect (p->hBtnFindInVdj, nullptr, FALSE);
                if (p->hBrowseList)   InvalidateRect (p->hBrowseList,   nullptr, FALSE);
            }
            break;
        }

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
            // Segmented pill toggle: Tanda | Settings
            if (di->CtlID == IDC_BTN_TAB_SETTINGS)
            {
                HDC hdc = di->hDC;
                RECT r = di->rcItem;
                bool hovered = (di->itemState & ODS_HOTLIGHT) != 0 || p->hoveredBtn == di->hwndItem;
                bool pressed = (di->itemState & ODS_SELECTED) != 0;

                COLORREF bg = hovered ? TCol::buttonHover : TCol::buttonBg;
                if (pressed) { BYTE b = GetRValue (bg) + 8; bg = RGB (b > 255 ? 255 : b, GetGValue (bg) + 8, GetBValue (bg) + 8); }

                HRGN rgn = CreateRoundRectRgn (r.left, r.top, r.right, r.bottom, 10, 10);
                SelectClipRgn (hdc, rgn);
                fillRect (hdc, r, bg);
                SelectClipRgn (hdc, nullptr);
                DeleteObject (rgn);

                HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldPen = (HPEN) SelectObject (hdc, pen);
                HBRUSH oldBr = (HBRUSH) SelectObject (hdc, GetStockObject (NULL_BRUSH));
                RoundRect (hdc, r.left, r.top, r.right, r.bottom, 10, 10);
                SelectObject (hdc, oldPen);
                SelectObject (hdc, oldBr);
                DeleteObject (pen);

                int midX = (r.left + r.right) / 2;
                bool onSettings = (p->activeTab == 1);

                // Left half: "Tanda"
                RECT leftR { r.left, r.top, midX, r.bottom };
                COLORREF leftFg = onSettings ? TCol::textDim : TCol::accentBrt;
                HFONT leftFont  = onSettings ? p->fontSmall  : p->fontBold;
                drawText (hdc, leftR, L"Tanda", leftFg, leftFont, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                // Right half: "Settings"
                RECT rightR { midX, r.top, r.right, r.bottom };
                COLORREF rightFg = onSettings ? TCol::accentBrt : TCol::textDim;
                HFONT rightFont  = onSettings ? p->fontBold     : p->fontSmall;
                drawText (hdc, rightR, L"Settings", rightFg, rightFont, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                // Center divider
                HPEN divPen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldDiv = (HPEN) SelectObject (hdc, divPen);
                MoveToEx (hdc, midX, r.top + 4, nullptr);
                LineTo   (hdc, midX, r.bottom - 4);
                SelectObject (hdc, oldDiv);
                DeleteObject (divPen);

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
            if (di->CtlID == IDC_BTN_CLOSE)
            {
                bg = btnHovered ? RGB (200, 45, 45) : RGB (70, 28, 28);
                fg = TCol::textBright;
            }
            else if (di->CtlID == IDC_BTN_ADD_END)       { bg = RGB (28, 55, 28);  fg = TCol::textBright; }
            else if (di->CtlID == IDC_BTN_FIND_IN_VDJ)
            {
                bool hasSel = (p->selectedResultIdx >= 0
                               && p->selectedResultIdx < (int) p->results.size());
                if (p->smartSearchPending)
                { bg = RGB (60, 40, 15); fg = TCol::textDim; }
                else if (!hasSel)
                { bg = RGB (24, 28, 42); fg = TCol::textDim; }
                else
                { bg = RGB (35, 50, 70); fg = TCol::textBright; }
            }
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
            HFONT btnFont = (di->CtlID == IDC_BTN_YEAR_TOGGLE
                          || di->CtlID == IDC_BTN_YEAR_RANGE
                          || di->CtlID == IDC_BTN_FIND_IN_VDJ)
                            ? p->fontSmall : p->fontNormal;
            drawOwnerButton (di, label, bg, fg, btnFont, passHover);
            return TRUE;
        }

        // ── Candidates list (3-column: title | artist | year) ──────────────────
        if (di->CtlID == IDC_CANDIDATES_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->candidates.size()) break;
            const TgMatchResult& mr  = p->candidates[di->itemID];
            const TgRecord&      rec = mr.record;

            HDC  hdc = di->hDC;
            RECT r   = di->rcItem;
            // Draw based on confirmedIdx only — ignore transient ODS_SELECTED so
            // selection persists when focus moves to another listbox.
            bool confirmed = ((int) di->itemID == p->confirmedIdx);

            fillRect (hdc, r, confirmed ? TCol::matchSel : TCol::panel);

            if (confirmed)
            {
                RECT accentR { r.left, r.top, r.left + 3, r.bottom };
                fillRect (hdc, accentR, TCol::accent);
            }

            const int rw = r.right - r.left;
            const int yearW = 52;
            const int gap = 4;
            const int titleW = (rw - yearW - gap * 2 - 6) * 55 / 100;
            const int artistW = rw - titleW - yearW - gap * 2 - 6;
            const int tx = r.left + 6;

            COLORREF titleCol  = confirmed ? TCol::textBright : TCol::textNormal;
            COLORREF otherCol  = confirmed ? TCol::textNormal : TCol::textDim;

            RECT titleR  { tx,                     r.top, tx + titleW,                     r.bottom };
            RECT artistR { tx + titleW + gap,       r.top, tx + titleW + gap + artistW,     r.bottom };
            RECT yearR   { r.right - yearW - 2,    r.top, r.right - 2,                     r.bottom };

            // Title, artist, year — all use fontSmall for uniform row density.
            // Confirmed candidate uses a bold variant on the title for emphasis.
            drawText (hdc, titleR,  rec.title,         titleCol, confirmed ? p->fontSmallBold : p->fontSmall,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, artistR, formatArtist (rec), otherCol, p->fontSmall,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, yearR,   rec.year,           otherCol, p->fontSmall,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, pen);
            MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
            LineTo   (hdc, r.right, r.bottom - 1);
            SelectObject (hdc, old);
            DeleteObject (pen);
            return TRUE;
        }

        // ── Results list (title | artist | year) ─────────────────────────────
        if (di->CtlID == IDC_RESULTS_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->results.size()) break;
            const TgRecord& rec = p->results[di->itemID];

            HDC  hdc  = di->hDC;
            RECT r    = di->rcItem;
            // Draw based on selectedResultIdx — persists across focus changes
            bool sel  = ((int) di->itemID == p->selectedResultIdx);
            bool even = (di->itemID % 2 == 0);

            fillRect (hdc, r, sel ? TCol::matchSel : even ? TCol::card : TCol::panel);

            const int rw      = r.right - r.left;
            const int yearW   = 52;
            const int gap     = 4;
            const int titleW  = (rw - yearW - gap * 2 - 6) * 55 / 100;
            const int artistW = rw - titleW - yearW - gap * 2 - 6;
            const int tx      = r.left + 6;

            RECT titleR  { tx,                     r.top, tx + titleW,                     r.bottom };
            RECT artistR { tx + titleW + gap,       r.top, tx + titleW + gap + artistW,     r.bottom };
            RECT yearR   { r.right - yearW - 2,    r.top, r.right - 2,                     r.bottom };

            // All three columns use fontSmall for a clean, uniform row density
            drawText (hdc, titleR,  rec.title,         TCol::textBright, p->fontSmallBold,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, artistR, formatArtist (rec), TCol::textDim,    p->fontSmall,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            std::wstring yearStr = rec.year.empty()
                ? (rec.date.size() >= 4 ? rec.date.substr (0, 4) : rec.date)
                : rec.year;
            drawText (hdc, yearR,   yearStr,            TCol::textDim,    p->fontSmall,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, pen);
            MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
            LineTo   (hdc, r.right, r.bottom - 1);
            SelectObject (hdc, old);
            DeleteObject (pen);
            return TRUE;
        }

        // ── Browse list (2 text rows + album art thumbnail on right) ───────────
        if (di->CtlID == IDC_BROWSE_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->browseItems.size()) break;
            const BrowseItem& bi = p->browseItems[di->itemID];

            HDC  hdc  = di->hDC;
            RECT r    = di->rcItem;
            // Draw based on selectedBrowseIdx — persists across focus changes
            bool sel  = ((int) di->itemID == p->selectedBrowseIdx);
            bool even = (di->itemID % 2 == 0);

            fillRect (hdc, r, sel ? TCol::matchSel : even ? TCol::card : TCol::panel);

            // Album art thumbnail on right (square, full row height minus padding)
            const int itemH   = r.bottom - r.top;
            const int artPad  = 4;
            const int artSize = itemH - artPad * 2;
            const int artX    = r.right - artSize - artPad;
            const int artY    = r.top + artPad;

            auto* coverBmp = reinterpret_cast<Gdiplus::Bitmap*> (
                CoverArt::getForPath (bi.filePath));
            if (coverBmp)
            {
                Gdiplus::Graphics g (hdc);
                g.SetInterpolationMode (Gdiplus::InterpolationModeHighQualityBicubic);
                g.DrawImage (coverBmp, artX, artY, artSize, artSize);
            }
            else
            {
                // Placeholder: dark rounded square with faint border
                RECT ar { artX, artY, artX + artSize, artY + artSize };
                fillRect (hdc, ar, RGB (24, 28, 40));
                HPEN pn  = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldP = (HPEN) SelectObject (hdc, pn);
                HBRUSH oldB = (HBRUSH) SelectObject (hdc, GetStockObject (NULL_BRUSH));
                Rectangle (hdc, ar.left, ar.top, ar.right, ar.bottom);
                SelectObject (hdc, oldP);
                SelectObject (hdc, oldB);
                DeleteObject (pn);
            }

            // Text area to the left of the art
            const int tx       = r.left + 6;
            const int textRight = artX - 6;
            const int yearW    = 40;
            const int titleW   = textRight - tx - yearW - 4;

            // Row 1 (top half): Title (left) + Year (right)
            int row1Top = r.top + 4;
            int row1Bot = r.top + itemH / 2;
            RECT titleR { tx,                   row1Top, tx + titleW,    row1Bot };
            RECT yearR  { textRight - yearW,    row1Top, textRight,       row1Bot };

            drawText (hdc, titleR, bi.title, TCol::textBright, p->fontBold,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, yearR,  bi.year,  TCol::textDim,    p->fontSmall,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            // Row 2 (bottom half): Artist, full width of text area
            RECT artistR { tx, row1Bot, textRight, r.bottom - 4 };
            drawText (hdc, artistR, bi.artist, TCol::textDim, p->fontSmall,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

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
        if (p && (hed == p->hEditTitle || hed == p->hEditArtist || hed == p->hEditYear))
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
        KillTimer (hwnd, TIMER_SEARCH_DEBOUNCE);
        if (p && p->hTooltip) { DestroyWindow (p->hTooltip); p->hTooltip = nullptr; }
        return 0;
    }

    return DefWindowProcW (hwnd, msg, wParam, lParam);
}

