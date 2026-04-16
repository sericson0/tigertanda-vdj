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
    // WM_PAINT fully fills the client area each frame, so we don't need
    // the window class to supply a background brush (avoids a leaked
    // HBRUSH handle that was never deleted at unregister time).
    wc.hbrBackground = nullptr;
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

// ── Hover popup (custom themed window for browse-row metadata) ────────────
//
// Shows album name and star rating for the hovered browse result after a
// 1-second dwell. Clamped to stay inside the plugin window bounds.
// ─────────────────────────────────────────────────────────────────────────

static constexpr UINT HOVER_POPUP_DWELL_MS = 750;

static const wchar_t* HOVER_POPUP_CLASS = L"TigerTandaHoverPopup";

// Compute the popup size. Always renders a fixed layout so the popup
// has a consistent footprint regardless of which fields are populated
// (missing fields render as an em-dash instead of collapsing the row).
// Rows:
//   1) Filename     (bold small, 1 line)
//   2) Folder path  (small dim, up to 2 lines via DT_WORDBREAK)
//   3) Album        (small, up to 2 lines via DT_WORDBREAK)
//   4) Stars+plays  (small, 1 line)
// Total text rows = 6 (rows 2 & 3 each counted twice for wrapping).
static constexpr int HOVER_POPUP_TEXT_LINES = 6;
static SIZE hoverPopupSize (const BrowseItem& /*bi*/)
{
    const int lineH  = 15;
    const int padY   = 6;
    SIZE sz;
    // Match the right column width so the popup aligns with the browse
    // list cards. Computed from the same layout constants as applyLayout:
    //   rightW = DLG_W - DLG_W * LEFT_COL_PCT / 100
    //   rw     = rightW - PAD - COL_GAP / 2   (COL_GAP = 4)
    const int rightW = DLG_W - DLG_W * LEFT_COL_PCT / 100;
    sz.cx = rightW - PAD - 4 / 2;
    sz.cy = padY * 2 + HOVER_POPUP_TEXT_LINES * lineH + 4;  // +4: small gaps
    return sz;
}

// Extract just the filename component from a full path (wchar_t).
static std::wstring fileNameFromPath (const std::wstring& path)
{
    if (path.empty()) return {};
    size_t slash = path.find_last_of (L"\\/");
    return (slash == std::wstring::npos) ? path : path.substr (slash + 1);
}

// Extract just the parent folder (everything up to the last slash).
static std::wstring parentFolderFromPath (const std::wstring& path)
{
    if (path.empty()) return {};
    size_t slash = path.find_last_of (L"\\/");
    return (slash == std::wstring::npos) ? std::wstring() : path.substr (0, slash);
}

// Collapse the current user's profile directory prefix (e.g.
// "C:\Users\seric") to "~" and normalize backslashes to forward slashes
// for the remainder — so "C:\Users\seric\Music\Tango\foo" becomes
// "~/Music/Tango/foo". Paths outside the profile are returned unchanged.
static std::wstring shortenUserProfilePath (const std::wstring& path)
{
    if (path.empty()) return {};

    wchar_t userProfile[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW (L"USERPROFILE", userProfile, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return path;

    size_t ulen = (size_t) len;
    if (path.size() <= ulen)
        return path;

    // Case-insensitive prefix match, Windows path style. Require the
    // char right after the prefix to be a slash so we don't collapse
    // "C:\Users\sericOther" into "~Other".
    if (_wcsnicmp (path.c_str(), userProfile, ulen) != 0)
        return path;
    if (path[ulen] != L'\\' && path[ulen] != L'/')
        return path;

    std::wstring rest = path.substr (ulen);
    for (wchar_t& c : rest) if (c == L'\\') c = L'/';
    return L"~" + rest;
}

// Build a 5-star visual: filled stars for bi.stars, hollow for the rest.
// Uses ASCII ★ ☆ (BMP characters, safe for MSVC).
static std::wstring buildStarString (int stars)
{
    if (stars < 0) stars = 0;
    if (stars > 5) stars = 5;
    std::wstring s;
    for (int i = 0; i < 5; ++i)
        s += (i < stars) ? L"\u2605" : L"\u2606";
    return s;
}

// Paint the popup body: dark bg, card border, Album row, stars row.
static void hoverPopupPaint (HWND popup, TigerTandaPlugin* p)
{
    if (!p || p->hoverPopupItem < 0
        || p->hoverPopupItem >= (int) p->browseItems.size())
        return;

    const BrowseItem& bi = p->browseItems[p->hoverPopupItem];

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint (popup, &ps);

    RECT client;
    GetClientRect (popup, &client);

    // Background + 1px border
    fillRect (hdc, client, TCol::card);
    HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
    HPEN oldPen = (HPEN) SelectObject (hdc, pen);
    HBRUSH oldBr = (HBRUSH) SelectObject (hdc, GetStockObject (NULL_BRUSH));
    Rectangle (hdc, client.left, client.top, client.right, client.bottom);
    SelectObject (hdc, oldPen);
    SelectObject (hdc, oldBr);
    DeleteObject (pen);

    const int padX  = 10;
    const int padY  = 6;
    const int lineH = 15;
    int y = client.top + padY;

    const std::wstring kDash = L"\u2014";

    // Row 1: Filename (bold small, bright) — no label column.
    // Use fontSmallBold (11pt) to keep the popup compact; it's tall enough
    // for the filename to remain readable without dominating the panel.
    std::wstring fname = fileNameFromPath (bi.filePath);
    RECT r1 { client.left + padX, y, client.right - padX, y + lineH };
    drawText (hdc, r1,
              fname.empty() ? kDash : fname,
              fname.empty() ? TCol::textDim : TCol::textBright,
              p->fontSmallBold,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += lineH;

    // Row 2: Parent folder path (dim, small, allowed to wrap to 2 lines).
    // Long paths like "C:\Users\...\Tango music (from Peace)\Anibal Troilo"
    // would otherwise collapse with ellipsis and hide WHERE the file lives.
    // DT_WORDBREAK lets the renderer break on spaces / backslashes; the
    // 2-line slot prevents the popup from growing unpredictably tall.
    // shortenUserProfilePath collapses "C:\Users\<me>\..." to "~/..." so
    // the interesting part of the path fits without wrapping in most cases.
    std::wstring folder = shortenUserProfilePath (parentFolderFromPath (bi.filePath));
    RECT r2 { client.left + padX, y, client.right - padX, y + lineH * 2 };
    drawText (hdc, r2,
              folder.empty() ? kDash : folder,
              folder.empty() ? TCol::textDim : TCol::textNormal,
              p->fontSmall,
              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
    y += lineH * 2 + 1;

    // Row 3: Album name, prefixed with "Album: " so it's unambiguous
    // alongside the filename/folder rows above. Allow 2 lines for long
    // album names (same approach as the folder path row above).
    RECT r3 { client.left + padX, y, client.right - padX, y + lineH * 2 };
    std::wstring albumText = bi.album.empty()
        ? (L"Album: " + kDash)
        : (L"Album: " + bi.album);
    drawText (hdc, r3, albumText,
              bi.album.empty() ? TCol::textDim : TCol::textBright,
              p->fontSmall,
              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
    y += lineH * 2 + 2;  // small visual gap before the meta row

    // Row 3: Stars in accent, separator + plays in dim gray, all fontSmall.
    // Draw in three adjacent rects so we can color the segments independently.
    std::wstring starsStr = (bi.stars > 0) ? buildStarString (bi.stars) : L"no rating";
    std::wstring playsStr = std::to_wstring (bi.playCount) + L" plays";
    const std::wstring sepStr = L"  \u2022  ";  // spaced middle dot

    HDC mdc = hdc;  // measure with the real DC's font metrics
    HFONT oldF = (HFONT) SelectObject (mdc, p->fontSmall);
    SIZE szStars {}, szSep {}, szPlays {};
    GetTextExtentPoint32W (mdc, starsStr.c_str(), (int) starsStr.size(), &szStars);
    GetTextExtentPoint32W (mdc, sepStr.c_str(),   (int) sepStr.size(),   &szSep);
    GetTextExtentPoint32W (mdc, playsStr.c_str(), (int) playsStr.size(), &szPlays);
    SelectObject (mdc, oldF);

    int x = client.left + padX;
    RECT rStars { x, y, x + szStars.cx, y + lineH };
    drawText (hdc, rStars, starsStr,
              (bi.stars > 0) ? TCol::accentBrt : TCol::textDim,
              p->fontSmall,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    x += szStars.cx;
    RECT rSep { x, y, x + szSep.cx, y + lineH };
    drawText (hdc, rSep, sepStr, TCol::textDim, p->fontSmall,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    x += szSep.cx;
    RECT rPlays { x, y, client.right - padX, y + lineH };
    drawText (hdc, rPlays, playsStr, TCol::textDim, p->fontSmall,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    EndPaint (popup, &ps);
}

static LRESULT CALLBACK hoverPopupWndProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* p = reinterpret_cast<TigerTandaPlugin*> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CREATE:
    {
        auto cs = reinterpret_cast<CREATESTRUCT*> (lp);
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, (LONG_PTR) cs->lpCreateParams);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;  // we paint the entire client area in WM_PAINT
    case WM_PAINT:
        hoverPopupPaint (hwnd, p);
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT;  // never catch mouse — popup is display-only
    }
    return DefWindowProcW (hwnd, msg, wp, lp);
}

static void ensureHoverPopupClass (HINSTANCE hInst)
{
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc {};
    wc.cbSize = sizeof (wc);
    wc.lpfnWndProc = hoverPopupWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor (nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // WM_PAINT does the fill
    wc.lpszClassName = HOVER_POPUP_CLASS;
    RegisterClassExW (&wc);
    registered = true;
}

// Position the popup next to a screen point but clamped to the plugin
// window's right column so it never covers the matches list on the left.
// If the default below-cursor placement would clip the bottom of the plugin
// rect, flip the popup above the cursor instead.
static void positionHoverPopup (TigerTandaPlugin* p, HWND popup, POINT cursorScreen, SIZE sz)
{
    if (!p || !p->hDlg) return;
    RECT pluginRect;
    GetWindowRect (p->hDlg, &pluginRect);

    // Right column x range in screen coords. Mirrors the math in applyLayout:
    // leftW = DLG_W * LEFT_COL_PCT / 100, rx = leftW + COL_GAP/2.
    const int leftW = DLG_W * LEFT_COL_PCT / 100;
    const int rxClient = leftW + 4 / 2;  // COL_GAP==4 in applyLayout
    int rightColLeft = pluginRect.left + rxClient;

    int x = cursorScreen.x + 14;
    int y;
    if (cursorScreen.y + 18 + sz.cy <= pluginRect.bottom)
        y = cursorScreen.y + 18;       // below cursor (default)
    else
        y = cursorScreen.y - sz.cy - 8; // flipped above cursor

    // Clamp x to the right column only — never let the popup extend into
    // the left column and cover the selected match card.
    if (x + sz.cx > pluginRect.right) x = pluginRect.right - sz.cx;
    if (x < rightColLeft)             x = rightColLeft;
    if (y < pluginRect.top)           y = pluginRect.top;

    SetWindowPos (popup, HWND_TOPMOST, x, y, sz.cx, sz.cy,
                  SWP_NOACTIVATE);
}

static void showHoverPopupNow (TigerTandaPlugin* p)
{
    if (!p || !p->hHoverPopup) return;
    if (p->hoverPendingItem < 0
        || p->hoverPendingItem >= (int) p->browseItems.size())
        return;

    p->hoverPopupItem = p->hoverPendingItem;
    const BrowseItem& bi = p->browseItems[p->hoverPopupItem];
    SIZE sz = hoverPopupSize (bi);
    positionHoverPopup (p, p->hHoverPopup, p->hoverPendingPt, sz);
    InvalidateRect (p->hHoverPopup, nullptr, TRUE);
    ShowWindow (p->hHoverPopup, SW_SHOWNOACTIVATE);
}

static void hideHoverPopup (TigerTandaPlugin* p)
{
    if (!p) return;
    if (p->hHoverPopup && IsWindowVisible (p->hHoverPopup))
        ShowWindow (p->hHoverPopup, SW_HIDE);
    p->hoverPopupItem = -1;
    p->hoverPendingItem = -1;
    if (p->hDlg)
        KillTimer (p->hDlg, TIMER_HOVER_POPUP);
}

// Advance the browse list selection by `direction` (+1 or -1), wrapping
// at the ends. Used by the Tab-cycling logic in the listbox subclasses.
// Also repaints the list and refreshes ADD/Prelisten + waveform state.
static void cycleBrowseSelection (TigerTandaPlugin* p, int direction)
{
    if (!p) return;
    int n = (int) p->browseItems.size();
    if (n <= 0) return;

    int cur = p->selectedBrowseIdx;
    int next;
    if (cur < 0)
        next = (direction >= 0) ? 0 : (n - 1);
    else
        next = ((cur + direction) % n + n) % n;

    p->selectedBrowseIdx = next;
    if (p->hBrowseList)
    {
        SendMessageW (p->hBrowseList, LB_SETCURSEL, next, 0);
        InvalidateRect (p->hBrowseList, nullptr, FALSE);
    }

    // ADD / Prelisten are never EnableWindow(FALSE)'d: disabled buttons do
    // not receive mouse events, so their tooltips never show and right-click
    // on ADD wouldn't reach our subclass. They're always enabled; the dim
    // visual state is controlled purely by WM_DRAWITEM via `hasSel`, and
    // the WM_COMMAND handlers no-op when `selectedBrowseIdx` is invalid.
    if (p->hBtnAddEnd)    InvalidateRect (p->hBtnAddEnd,    nullptr, FALSE);
    if (p->hBtnPrelisten) InvalidateRect (p->hBtnPrelisten, nullptr, FALSE);

    const BrowseItem& bi = p->browseItems[next];
    if (!bi.filePath.empty())
    {
        rebuildPrelistenWaveBins (p->prelistenWaveBins, bi.filePath);
        p->prelistenWavePath = bi.filePath;
        p->prelistenPos = 0.0;
        if (p->hDlg)
            InvalidateRect (p->hDlg, &p->prelistenWaveRect, FALSE);
    }
}

// Subclass proc for the matches listbox — distinguishes mouse-driven from
// keyboard-driven selection changes so the LBN_SELCHANGE handler can decide
// between firing the smart search immediately (click) or debouncing by
// 250ms (arrow key). Also:
//   • VK_DOWN with no current selection → select row 0 (Windows LB defaults
//     don't auto-seed selection the first time the arrow is pressed, which
//     feels broken to keyboard users).
//   • VK_TAB / Shift+Tab → cycle through the browse result rows without
//     moving keyboard focus. Lets the user press a match, then Tab through
//     the three library hits with ADD ready to fire.
static LRESULT CALLBACK resultsListSubclassProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                 UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* p = reinterpret_cast<TigerTandaPlugin*> (dwRefData);
    if (p)
    {
        switch (msg)
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            p->resultsLastInputWasMouse = true;
            break;
        case WM_KEYDOWN:
            p->resultsLastInputWasMouse = false;
            if (wp == VK_ESCAPE)
            {
                SendMessageW (p->hDlg, WM_CLOSE, 0, 0);
                return 0;
            }
            if (wp == VK_TAB)
            {
                bool shift = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
                cycleBrowseSelection (p, shift ? -1 : +1);
                return 0;  // consume — don't let LB treat Tab as a char
            }
            if (wp == VK_DOWN)
            {
                int cur = (int) SendMessageW (hwnd, LB_GETCURSEL, 0, 0);
                if (cur < 0 && !p->results.empty())
                {
                    SendMessageW (hwnd, LB_SETCURSEL, 0, 0);
                    // Fire the LBN_SELCHANGE path so the selected-match
                    // smart search logic runs (mirrors what a click does).
                    SendMessageW (GetParent (hwnd), WM_COMMAND,
                                  MAKEWPARAM (GetDlgCtrlID (hwnd), LBN_SELCHANGE),
                                  (LPARAM) hwnd);
                    return 0;
                }
            }
            if (wp == VK_RETURN)
            {
                // Enter = "confirm current match and search library now"
                // (same behavior as a mouse click, not the debounced keyboard
                // path). Lets keyboard users stop any in-flight debounce and
                // jump straight to the library search.
                int cur = (int) SendMessageW (hwnd, LB_GETCURSEL, 0, 0);
                if (cur >= 0 && cur < (int) p->results.size()
                    && !p->smartSearchPending)
                {
                    p->lastSmartSearchTitle.clear();
                    p->lastSmartSearchArtist.clear();
                    p->triggerBrowserSearch (p->results[cur]);
                }
                return 0;
            }
            break;
        case WM_GETDLGCODE:
            // Ask Windows to route Tab through WM_KEYDOWN to us instead of
            // treating it as a dialog-navigation key (we aren't a dialog,
            // but some listbox paths still consume Tab otherwise).
            return DLGC_WANTTAB | DLGC_WANTARROWS | DLGC_WANTCHARS
                   | DefSubclassProc (hwnd, msg, wp, lp);
        case WM_NCDESTROY:
            RemoveWindowSubclass (hwnd, resultsListSubclassProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc (hwnd, msg, wp, lp);
}

// Subclass proc for the browse listbox — tracks mouse movement, queues the
// dwell timer, hides on mouse leave. Only attached to the browse list.
static LRESULT CALLBACK browseHoverSubclassProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                 UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* p = reinterpret_cast<TigerTandaPlugin*> (dwRefData);
    if (!p) return DefSubclassProc (hwnd, msg, wp, lp);

    switch (msg)
    {
    case WM_MOUSEMOVE:
    {
        POINT pt = { GET_X_LPARAM (lp), GET_Y_LPARAM (lp) };
        DWORD itemInfo = (DWORD) SendMessageW (hwnd, LB_ITEMFROMPOINT, 0, MAKELPARAM (pt.x, pt.y));
        bool outside = HIWORD (itemInfo) != 0;
        int item = outside ? -1 : (int) LOWORD (itemInfo);

        if (item < 0 || item >= (int) p->browseItems.size())
        {
            hideHoverPopup (p);
        }
        else if (item != p->hoverPendingItem && item != p->hoverPopupItem)
        {
            // New row: reset dwell timer, hide any currently showing popup
            if (p->hHoverPopup && IsWindowVisible (p->hHoverPopup))
                ShowWindow (p->hHoverPopup, SW_HIDE);
            p->hoverPopupItem = -1;
            p->hoverPendingItem = item;
            POINT screen = pt;
            ClientToScreen (hwnd, &screen);
            p->hoverPendingPt = screen;
            if (p->hDlg)
                SetTimer (p->hDlg, TIMER_HOVER_POPUP, HOVER_POPUP_DWELL_MS, nullptr);
        }
        else if (item == p->hoverPendingItem && item != p->hoverPopupItem)
        {
            // Same pending row, cursor moving within it — update captured
            // position so the popup shows where the cursor ends up settling.
            POINT screen = pt;
            ClientToScreen (hwnd, &screen);
            p->hoverPendingPt = screen;
        }

        TRACKMOUSEEVENT tme {};
        tme.cbSize = sizeof (tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent (&tme);
        break;
    }

    case WM_MOUSELEAVE:
        hideHoverPopup (p);
        break;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE)
        {
            SendMessageW (p->hDlg, WM_CLOSE, 0, 0);
            return 0;
        }
        if (wp == VK_TAB)
        {
            bool shift = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
            cycleBrowseSelection (p, shift ? -1 : +1);
            return 0;
        }
        if (wp == VK_RETURN)
        {
            // Enter on the browse list = ADD (playlist_add). Mirrors what
            // the ADD button does via left click.
            if (p->selectedBrowseIdx >= 0
                && p->selectedBrowseIdx < (int) p->browseItems.size())
            {
                p->addSelectedBrowseToAutomix();
            }
            return 0;
        }
        break;

    case WM_GETDLGCODE:
        return DLGC_WANTTAB | DLGC_WANTARROWS | DLGC_WANTCHARS
               | DefSubclassProc (hwnd, msg, wp, lp);

    case WM_NCDESTROY:
        RemoveWindowSubclass (hwnd, browseHoverSubclassProc, uIdSubclass);
        break;
    }

    return DefSubclassProc (hwnd, msg, wp, lp);
}

// Subclass for the ADD button — left click falls through to the normal
// WM_COMMAND handler (playlist_add); right click invokes sidelist_add via
// addSelectedBrowseToAutomix("sidelist_add"). Also disables the normal
// "press-state" behavior on right click so the button doesn't stick.
static LRESULT CALLBACK addButtonSubclassProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                               UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* p = reinterpret_cast<TigerTandaPlugin*> (dwRefData);
    if (p)
    {
        switch (msg)
        {
        case WM_RBUTTONUP:
            // Only fire when there's a valid browse selection — same gate
            // as the left-click path.
            if (p->selectedBrowseIdx >= 0
                && p->selectedBrowseIdx < (int) p->browseItems.size())
            {
                p->addSelectedBrowseToAutomix ("sidelist_add");
            }
            return 0;
        case WM_RBUTTONDOWN:
            // Consume so Windows doesn't try to do anything with it
            return 0;
        case WM_NCDESTROY:
            RemoveWindowSubclass (hwnd, addButtonSubclassProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc (hwnd, msg, wp, lp);
}

// Subclass for the candidates listbox: Tab cycles the browse result rows
// (just like the matches + browse listboxes) so the keyboard flow stays
// consistent — user lands on a candidate, Tab-walks the top library hits.
static LRESULT CALLBACK candidatesListSubclassProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* p = reinterpret_cast<TigerTandaPlugin*> (dwRefData);
    if (p)
    {
        switch (msg)
        {
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE)
            {
                SendMessageW (p->hDlg, WM_CLOSE, 0, 0);
                return 0;
            }
            if (wp == VK_TAB)
            {
                bool shift = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
                cycleBrowseSelection (p, shift ? -1 : +1);
                return 0;
            }
            if (wp == VK_DOWN)
            {
                int cur = (int) SendMessageW (hwnd, LB_GETCURSEL, 0, 0);
                int cnt = (int) SendMessageW (hwnd, LB_GETCOUNT,  0, 0);
                if ((cur < 0 || cur >= cnt - 1) && !p->results.empty())
                {
                    // At bottom of candidates (or empty): jump to results list
                    SetFocus (p->hResultsList);
                    if (p->selectedResultIdx < 0)
                    {
                        SendMessageW (p->hResultsList, LB_SETCURSEL, 0, 0);
                        SendMessageW (GetParent (hwnd), WM_COMMAND,
                                      MAKEWPARAM (IDC_RESULTS_LIST, LBN_SELCHANGE),
                                      (LPARAM) p->hResultsList);
                    }
                    return 0;
                }
            }
            break;
        case WM_GETDLGCODE:
            return DLGC_WANTTAB | DLGC_WANTARROWS | DLGC_WANTCHARS
                   | DefSubclassProc (hwnd, msg, wp, lp);
        case WM_NCDESTROY:
            RemoveWindowSubclass (hwnd, candidatesListSubclassProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc (hwnd, msg, wp, lp);
}

// Subclass for the title / artist / year edit controls — forwards Escape
// to the dialog so the key closes the window even when focus is in a
// search field. Everything else (Tab, arrows, typing) goes through the
// default edit-control handling.
static LRESULT CALLBACK editEscapeSubclassProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* p = reinterpret_cast<TigerTandaPlugin*> (dwRefData);
    if (p && msg == WM_KEYDOWN && wp == VK_ESCAPE)
    {
        SendMessageW (p->hDlg, WM_CLOSE, 0, 0);
        return 0;
    }
    if (msg == WM_GETDLGCODE && p)
    {
        // Let the edit control handle chars/tab itself, but also ensure
        // it forwards VK_ESCAPE to us via WM_KEYDOWN.
        return DLGC_WANTCHARS | DLGC_WANTARROWS
               | DefSubclassProc (hwnd, msg, wp, lp);
    }
    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass (hwnd, editEscapeSubclassProc, uIdSubclass);
    return DefSubclassProc (hwnd, msg, wp, lp);
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
                : disabled ? TCol::buttonDisabled
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
    bool pressed  = (di->itemState & ODS_SELECTED) != 0;
    bool disabled = (di->itemState & ODS_DISABLED) != 0;
    bool hovered  = (di->itemState & ODS_HOTLIGHT) != 0 || extraHover;

    COLORREF fillBg = bgColor;
    if (hovered && !isActive && !pressed && !disabled)
    {
        BYTE hr = (BYTE)(GetRValue (bgColor) + 18 > 255 ? 255 : GetRValue (bgColor) + 18);
        BYTE hg = (BYTE)(GetGValue (bgColor) + 18 > 255 ? 255 : GetGValue (bgColor) + 18);
        BYTE hb = (BYTE)(GetBValue (bgColor) + 18 > 255 ? 255 : GetBValue (bgColor) + 18);
        fillBg = RGB (hr, hg, hb);
    }
    fillRect (hdc, r, fillBg);

    COLORREF fg;
    if (disabled)
        fg = TCol::textDim;
    else
        fg = (isActive || pressed) ? TCol::accentBrt : hovered ? TCol::textNormal : TCol::textDim;
    HFONT font = (isActive || pressed) ? fontBold : fontNormal;
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
// Row 1: Title (bold, bright — same size as body, uniform font size)
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

    int px = r.left + DETAIL_PAD_X;
    int py = r.top + DETAIL_PAD_Y;
    const int rightEdge = r.right - DETAIL_PAD_X;
    const int titleH = 18;
    const int lineH  = 18;
    // DT_NOCLIP is important here: drawText (→ DrawTextW) otherwise clips to
    // the passed RECT, which chops descenders (g, y, p) off rows sized tight
    // to the font height. With DT_NOCLIP the glyph paints in full.
    const UINT rowFlags = DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP;

    // Row 1: Track title (bold, bright — same size as body)
    RECT rTitle { px, py, rightEdge, py + titleH };
    drawText (hdc, rTitle, rec.title, TCol::textBright, fontTitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP);
    py += titleH + DETAIL_ROW_GAP;

    // Row 2: Bandleader · Singer
    std::wstring line2 = rec.bandleader;
    if (!rec.singer.empty()) { if (!line2.empty()) line2 += L"  \u00B7  "; line2 += rec.singer; }
    RECT r2 { px, py, rightEdge, py + lineH };
    drawText (hdc, r2, line2, TCol::textNormal, fontBody, rowFlags);
    py += lineH + DETAIL_ROW_GAP;

    // Row 3: Date · Genre
    std::wstring dateStr = formatDateYMD (rec.date);
    std::wstring line3;
    if (!dateStr.empty())   line3 += dateStr;
    if (!rec.genre.empty()) { if (!line3.empty()) line3 += L"  \u00B7  "; line3 += rec.genre; }
    RECT r3 { px, py, rightEdge, py + lineH };
    drawText (hdc, r3, line3, TCol::textDim, fontBody, rowFlags);
    py += lineH + DETAIL_ROW_GAP;

    // Row 4: Label
    if (!rec.label.empty())
    {
        std::wstring line4 = L"Label: " + rec.label;
        RECT r4 { px, py, rightEdge, py + lineH };
        drawText (hdc, r4, line4, TCol::textDim, fontBody, rowFlags);
    }
    py += lineH + DETAIL_ROW_GAP;

    // Row 5: Group
    if (!rec.grouping.empty())
    {
        std::wstring line5 = L"Group: " + rec.grouping;
        RECT r5 { px, py, rightEdge, py + lineH };
        drawText (hdc, r5, line5, TCol::textDim, fontBody, rowFlags);
    }
}

// Draw prelisten waveform into a RECT (double-buffered)
static void drawPrelistenWave (HDC hdc, RECT wr, const std::vector<int>& bins, double posPercent)
{
    int ww = wr.right - wr.left;
    int wh = wr.bottom - wr.top;
    // Guard against zero/negative dimensions: CreateCompatibleBitmap with
    // a zero extent returns NULL and we'd then leak the DC + draw nothing.
    if (ww <= 0 || wh <= 0) return;
    HDC memDC = CreateCompatibleDC (hdc);
    HBITMAP memBmp = CreateCompatibleBitmap (hdc, ww, wh);
    HBITMAP oldBmp = (HBITMAP) SelectObject (memDC, memBmp);

    RECT localWr = { 0, 0, ww, wh };
    fillRect (memDC, localWr, TCol::waveformBg);

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
            fillRect (memDC, ph, TCol::waveformPeak);
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
    const int closeBtnW = 22;
    const int closeRightMargin = 26 - closeBtnW;  // existing right margin
    MoveWindow (p->hBtnClose, DLG_W - closeBtnW - closeRightMargin, topY, closeBtnW, TAB_BTN_H, FALSE);
    ShowWindow (p->hBtnClose, SW_SHOW);
    // Derive toggle width from available space rather than hard-coding 140
    // so the Tanda/Settings toggle can't overlap the close button when
    // DLG_W changes. Reserve the area right of the brand text, with a
    // reasonable minimum.
    const int toggleMinW = 120;
    const int toggleMaxW = DLG_W - (closeBtnW + closeRightMargin) - PAD - (DLG_W / 2 + PAD);
    int toggleW = (toggleMaxW < toggleMinW) ? toggleMinW : toggleMaxW;
    if (toggleW > 180) toggleW = 180;  // cap so it doesn't get absurdly wide
    MoveWindow (p->hBtnTabSettings,
                DLG_W - closeBtnW - closeRightMargin - PAD - toggleW, topY,
                toggleW, TAB_BTN_H, FALSE);
    ShowWindow (p->hBtnTabSettings, SW_SHOW);

    // Small gap between left and right panels (less than the outer PAD)
    const int COL_GAP = 4;

    // Main view (activeTab == 0)
    if (showMain)
    {
        const int lx = PAD;
        const int lw = leftW - PAD - COL_GAP / 2;
        // Reserve scrollbar width on right so inputs/candidates align with
        // scrolled results list items. With LBS_DISABLENOSCROLL the scrollbar
        // is always visible on the match/candidate lists, so row usable
        // width always equals the input row's usable width.
        const int sbW = GetSystemMetrics (SM_CXVSCROLL);
        const int usableW = lw - sbW;

        // Metadata load failure banner — when shown, shifts the main left
        // column (column headers, search row, candidates, matches header,
        // matches list) and right column (detail box and everything below)
        // down by META_BANNER_H.
        const int bannerShift = p->metadataLoadFailed ? META_BANNER_H : 0;
        if (p->metadataLoadFailed)
            p->metaBannerRect = { PAD, TOP_H + TOP_GAP,
                                  DLG_W - PAD, TOP_H + TOP_GAP + META_BANNER_H };
        else
            p->metaBannerRect = { 0, 0, 0, 0 };

        // Cache painted column header Y so WM_PAINT doesn't recompute.
        p->columnHeaderY = TOP_H + TOP_GAP + bannerShift;

        // Search inputs — below column headers (headers are painted, 14px)
        int ly = p->columnHeaderY + 14;
        const int gap = 4;
        const int titleW = (usableW - YEAR_COL_W - gap * 2) * 55 / 100;
        const int artistW = usableW - titleW - YEAR_COL_W - gap * 2;
        MoveWindow (p->hEditTitle,  lx,                            ly, titleW,     EDIT_H, FALSE);
        MoveWindow (p->hEditArtist, lx + titleW + gap,             ly, artistW,    EDIT_H, FALSE);
        MoveWindow (p->hEditYear,   lx + usableW - YEAR_COL_W,     ly, YEAR_COL_W, EDIT_H, FALSE);
        // Lock button: fits in scrollbar margin (sbW ≈ 17px)
        MoveWindow (p->hBtnLock, lx + usableW + 1, ly, LOCK_BTN_W, EDIT_H, FALSE);
        ly += EDIT_H + TRACK_SEARCH_GAP;

        // Candidates list — exactly 3 rows. LBS_DISABLENOSCROLL keeps the
        // scrollbar always visible so column X positions inside the rows
        // match the search row above exactly. Width is lw (not usableW)
        // because the listbox itself owns the scrollbar column.
        int candH = CAND_ITEM_H * 3 + 2;
        MoveWindow (p->hCandList, lx, ly, lw, candH, FALSE);
        ly += candH + 6;  // small gap before matches header

        // "MATCHES (N)" header painted, 14px. Hoist Y into a plugin member
        // so WM_PAINT doesn't have to recompute it from raw constants.
        const int BOTTOM_MARGIN = 4;
        p->matchHeaderY = ly;
        int matchListTop = ly + 14;
        int matchListBot = DLG_H - BOTTOM_MARGIN;
        MoveWindow (p->hResultsList, lx, matchListTop, lw, matchListBot - matchListTop, FALSE);

        // Right column
        const int rx = leftW + COL_GAP / 2;
        const int rw = rightW - PAD - COL_GAP / 2;

        // Detail box starts flush against the top bar (plus banner shift)
        int ry = TOP_H + bannerShift;

        // Detail box position (painted in WM_PAINT, not a control)
        int detailBot = ry + DETAIL_BOX_H;

        // Right column: [detail box] → [browse header strip = painted
        // label only] → [browse list] → [prelisten/ADD row].
        // With the FIND IN VDJ button removed, the header strip is
        // simply a slim painted label row.
        int browseTop = detailBot + BROWSE_HEADER_H;
        p->browseResultsHeaderY = detailBot + 2;  // 2px inset for painted label

        // Browse list is exactly 4 items tall — sized to avoid scrollbar.
        int browseH = BROWSE_ITEM_H * 4 + 2;
        // Anchor prelisten row directly below the browse list with a small
        // visible gap, so the right column feels evenly spaced regardless of
        // the exact DLG_H budget.
        int preRowY = browseTop + browseH + PRELISTEN_TOP_GAP;
        // Never collide with the bottom margin — clamp if needed.
        int preRowMax = DLG_H - BOTTOM_MARGIN - BTN_H;
        if (preRowY > preRowMax) preRowY = preRowMax;
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
    showCtrl (p->hBtnLock,       showMain);
    showCtrl (p->hCandList,      showMain);
    showCtrl (p->hResultsList,   showMain);
    showCtrl (p->hBtnPrelisten,  showMain);
    showCtrl (p->hBtnAddEnd,     showMain);
    // Browse list is shown only when it has items; otherwise the main
    // window paints a placeholder in its place.
    if (p->hBrowseList)
        ShowWindow (p->hBrowseList,
                    (showMain && !p->browseItems.empty()) ? SW_SHOW : SW_HIDE);

    // Settings view (activeTab == 1) — two-column layout
    //
    //   Left column (50%): SEARCH FILTERS (ARTISTS / YEAR RANGE / OTHER
    //                      sub-groups) + Tiger Tag logo
    //   Right column (50%): HOW IT WORKS sections shown all at once
    //                       (Option B: no sub-tabs, one scroll-free view)
    //
    // Layout constants are computed from DLG_W so the tab still fits if
    // the dialog ever resizes. Sub-group header Ys are cached on the plugin
    // struct so WM_PAINT can align painted headers with the control rows
    // without duplicating the layout math.
    if (showS)
    {
        const int COL_GAP   = 10;
        const int settingsL = DLG_W * 50 / 100;        // left column width (50/50)
        const int lx        = PAD;
        const int lw        = settingsL - PAD;          // usable left width
        const int rx        = settingsL + COL_GAP / 2;
        const int rw        = DLG_W - PAD - rx;         // usable right width
        (void) rw;

        const int btnH = BTN_H - 4;
        const int subHdrH = 14;   // matches Tanda-style section header font
        const int subHdrPad = 3;  // gap below a sub-group header
        const int rowGap = 1;     // tight gap between checkbox rows
        const int subGroupGap = 6;  // vertical gap between sub-groups

        // ── Left column: FILTERS main header + sub-groups ──────────────
        // Everything on the left is a 2-column grid. DEFAULTS gets
        // [ARTIST][SINGER] on one row and [YEAR + spinner][GENRE] on the
        // next. OTHER gets [LABEL][GROUPING] / [ORCHESTRA][TRACK]. The
        // bolded "FILTERS" header sits at the top, mirroring "HOW IT WORKS"
        // on the right.
        int sy = TOP_H + PAD;

        const int colGap2 = 4;
        const int colW   = (lw - colGap2) / 2;   // 2-column grid
        const int lCol   = lx;
        const int rCol   = lx + colW + colGap2;

        // Main header: FILTERS (drawn bold in WM_PAINT; takes ~18px).
        const int mainHdrH = 18;
        const int mainHdrPad = 6;   // gap below main header before first subgroup
        p->settingsMainHeaderY = sy;
        sy += mainHdrH + mainHdrPad;

        // Sub-group 1: DEFAULTS — [ARTIST][SINGER] then [YEAR + spinner][GENRE]
        p->settingsArtistsHeaderY = sy;
        sy += subHdrH + subHdrPad;
        MoveWindow (p->hChkArtist, lCol, sy, colW, btnH, FALSE);
        MoveWindow (p->hChkSinger, rCol, sy, colW, btnH, FALSE);
        sy += btnH + rowGap;

        // Row 2 of DEFAULTS: [YEAR + spinner][GENRE]
        //
        // Left half of this row holds both the YEAR checkbox and its
        // ±N spinner as one control group. We give the YEAR checkbox a
        // fixed width (wide enough for the glyph + "YEAR" label) and
        // squeeze the spinner into the remaining left-column space.
        const int spinBtnW   = 20;
        const int spinLabelW = 40;
        const int spinGroupW = spinBtnW * 2 + spinLabelW + 2;
        // YEAR checkbox uses the remainder of the left column minus the
        // spinner and a small gap.
        const int yearSpinGap = 4;
        int yearChkW = colW - spinGroupW - yearSpinGap;
        if (yearChkW < 48) yearChkW = 48;  // floor for glyph + "YEAR"

        int yearChkX = lCol;
        int spinX    = yearChkX + yearChkW + yearSpinGap;

        MoveWindow (p->hBtnYearToggle, yearChkX, sy, yearChkW, btnH,     FALSE);
        MoveWindow (p->hBtnYearMinus,  spinX,                                   sy + 1, spinBtnW,   btnH - 2, FALSE);
        MoveWindow (p->hBtnYearRange,  spinX + spinBtnW + 1,                    sy + 1, spinLabelW, btnH - 2, FALSE);
        MoveWindow (p->hBtnYearPlus,   spinX + spinBtnW + 1 + spinLabelW + 1,   sy + 1, spinBtnW,   btnH - 2, FALSE);
        MoveWindow (p->hChkGenre,      rCol,     sy, colW,     btnH,     FALSE);

        // The settingsYearHeaderY field is unused in the new layout (year
        // row has no sub-header) but we still reset it to keep state clean.
        p->settingsYearHeaderY = 0;

        sy += btnH + rowGap;
        sy += subGroupGap;

        // Sub-group 2: OTHER — 2 rows × 2 cols.
        //   Row 1: [LABEL][GROUPING]
        //   Row 2: [ORCHESTRA][TRACK]
        p->settingsOtherHeaderY = sy;
        sy += subHdrH + subHdrPad;
        MoveWindow (p->hChkLabel,     lCol, sy, colW, btnH, FALSE);
        MoveWindow (p->hChkGrouping,  rCol, sy, colW, btnH, FALSE);
        sy += btnH + rowGap;
        MoveWindow (p->hChkOrchestra, lCol, sy, colW, btnH, FALSE);
        MoveWindow (p->hChkTrack,     rCol, sy, colW, btnH, FALSE);
        sy += btnH + rowGap;

        // Logo fills the remaining vertical space between the last filter
        // row and the bottom of the Settings tab. Brand row is gone from
        // this tab (see Fix 1) so no BRAND_H subtraction; the increased
        // logoTopPad keeps clear separation from the OTHER row above.
        const int logoTopPad = 16;
        const int logoBotPad = 4;
        int logoTop = sy + logoTopPad;
        int logoBot = DLG_H - PAD - logoBotPad;
        int logoH = logoBot - logoTop;
        if (logoH < 0) logoH = 0;
        p->settingsLogoY = logoTop;
        p->settingsLogoH = logoH;

        // Hide legacy How-tab buttons (no longer used in Option B layout)
        for (int i = 0; i < 5; ++i)
            if (p->hBtnHowTabs[i]) ShowWindow (p->hBtnHowTabs[i], SW_HIDE);
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
    showCtrl (p->hBtnYearMinus,   showS);
    showCtrl (p->hBtnYearPlus,    showS);
    // How-tab buttons are unused in the new Settings layout — always hidden
    for (int i = 0; i < 5; ++i)
        if (p->hBtnHowTabs[i]) ShowWindow (p->hBtnHowTabs[i], SW_HIDE);

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
    // ── Erase background ─────────────────────────────────────────────────────
    // WM_PAINT fully fills the client area (TCol::bg + panels), so skip the
    // default erase to avoid flicker and eliminate the need for a window
    // class background brush.
    case WM_ERASEBKGND:
        return 1;

    // ── Close ────────────────────────────────────────────────────────────────
    case WM_CLOSE:
        if (p)
        {
            p->dialogRequestedOpen  = false;
            p->suppressNextHideSync = true;
        }
        ShowWindow (hwnd, SW_HIDE);
        return 0;

    // ── Escape at the window level (falls through when a child consumed it) ─
    // Children that want to block this can return from their own WM_KEYDOWN.
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            SendMessageW (hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;

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
                p->hBtnAddEnd, p->hBtnYearToggle, p->hBtnYearRange,
                p->hBtnYearMinus, p->hBtnYearPlus,
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

        // Cache our own HWND on the plugin struct early so other helpers
        // (hover popup, etc.) can access it before CreateWindowExW returns.
        p->hDlg = hwnd;

        // Walk up to the VDJ main top-level window and cache it for
        // isVdjHostForeground(). Must happen after the plugin HWND is
        // parented into VDJ so GA_ROOT can find the real host.
        setVdjRootHwnd (hwnd);

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

        // Lock button (right of year edit — freezes search fields)
        p->hBtnLock = mkBtn (IDC_BTN_LOCK, L"\xE72E");

        // Filter buttons (ALL CAPS)
        p->hChkArtist    = mkBtn (IDC_CHK_SAME_ARTIST,    L"ARTIST");
        p->hChkSinger    = mkBtn (IDC_CHK_SAME_SINGER,    L"SINGER");
        p->hChkGrouping  = mkBtn (IDC_CHK_SAME_GROUPING,  L"GROUPING");
        p->hChkGenre     = mkBtn (IDC_CHK_SAME_GENRE,     L"GENRE");
        p->hChkOrchestra = mkBtn (IDC_CHK_SAME_ORCHESTRA, L"ORCHESTRA");
        p->hChkLabel     = mkBtn (IDC_CHK_SAME_LABEL,     L"LABEL");
        p->hChkTrack     = mkBtn (IDC_CHK_SAME_TRACK,     L"TRACK");

        // Year range controls — toggle + spinner ([- ] [label] [+])
        p->hBtnYearToggle = mkBtn (IDC_BTN_YEAR_TOGGLE, L"YEAR");
        {
            static const wchar_t* kYrLabels[] = { L"\u00B10", L"\u00B11", L"\u00B12", L"\u00B13", L"\u00B15", L"\u00B110" };
            static const int      kYrVals[]   = { 0, 1, 2, 3, 5, 10 };
            int initIdx = 0;
            for (int i = 0; i < 6; ++i)
                if (kYrVals[i] == p->yearRange) { initIdx = i; break; }
            p->hBtnYearMinus = mkBtn (IDC_BTN_YEAR_MINUS, L"\u2212");  // −
            p->hBtnYearRange = mkBtn (IDC_BTN_YEAR_RANGE, kYrLabels[initIdx]);
            p->hBtnYearPlus  = mkBtn (IDC_BTN_YEAR_PLUS,  L"+");
            EnableWindow (p->hBtnYearMinus, initIdx > 0);
            EnableWindow (p->hBtnYearPlus,  initIdx < 5);
        }

        // "How it works" sub-tab buttons (Settings tab)
        static const wchar_t* kHowNames[] = { L"Overview", L"Track", L"Matches", L"Browser", L"Filters" };
        for (int i = 0; i < 5; ++i)
            p->hBtnHowTabs[i] = mkBtn (IDC_BTN_HOW_TAB_0 + i, kHowNames[i]);

        // Candidates list — fixed 3 rows, sized to fit exactly. No
        // scrollbar: the row count is pinned so the scrollbar track would
        // just be dead space. The candidate row draw code still uses
        // `r.right - sbW` for its column math; with the scrollbar gone,
        // r.right == lx + lw (full listbox width) and r.right - sbW still
        // lands at lx + usableW, matching the search row column positions.
        p->hCandList = CreateWindowW (L"LISTBOX", nullptr,
                                      WS_CHILD | WS_VISIBLE
                                      | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS
                                      | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                      0, 0, 10, 10, hwnd,
                                      (HMENU) IDC_CANDIDATES_LIST, nullptr, nullptr);

        // Results list — LBS_DISABLENOSCROLL so the scrollbar column is
        // always reserved, keeping row column positions aligned with the
        // search row/candidates list regardless of how many matches there
        // currently are.
        p->hResultsList = CreateWindowW (L"LISTBOX", nullptr,
                                         WS_CHILD | WS_VISIBLE | WS_VSCROLL
                                         | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS
                                         | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT
                                         | LBS_DISABLENOSCROLL,
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

        // ADD / Prelisten are intentionally NOT EnableWindow(FALSE)'d.
        // Disabled controls don't receive WM_MOUSEMOVE, which means:
        //   • TTF_SUBCLASS tooltips would never trigger on them
        //   • Right-click on ADD couldn't reach addButtonSubclassProc
        // Owner-draw dims them via `hasSel`; the WM_COMMAND handlers no-op
        // when the browse selection is invalid.

        // Tooltips for all buttons
        p->hTooltip = CreateWindowExW (0, TOOLTIPS_CLASS, nullptr,
                                       WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       hwnd, nullptr, nullptr, nullptr);
        if (p->hTooltip)
        {
            SendMessageW (p->hTooltip, TTM_SETMAXTIPWIDTH, 0, 260);
            // 500ms initial delay — long enough that tooltips don't flash
            // during fast mouse movement, short enough that a deliberate
            // hover actually produces one.
            SendMessageW (p->hTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 500);

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
            addTip (p->hBtnPrelisten,    L"Preview selected library result (play/pause)");
            addTip (p->hBtnAddEnd,       L"Click: add to automix bottom\nRight-click: add to sidelist");
            addTip (p->hChkArtist,       L"Filter: same bandleader / artist");
            addTip (p->hChkSinger,       L"Filter: same singer");
            addTip (p->hChkGrouping,     L"Filter: same recording period / group");
            addTip (p->hChkGenre,        L"Filter: same genre");
            addTip (p->hChkOrchestra,    L"Filter: same orchestra");
            addTip (p->hChkLabel,        L"Filter: same record label");
            addTip (p->hChkTrack,        L"Filter: same track title");
            addTip (p->hBtnYearToggle,   L"Toggle year-range filter on/off");
            addTip (p->hBtnYearMinus,    L"Decrease year range");
            addTip (p->hBtnYearRange,    L"Year range (\u00B1years from confirmed candidate)");
            addTip (p->hBtnYearPlus,     L"Increase year range");
            addTip (p->hBtnLock,         L"Lock: freeze search fields while browsing");
        }

        // Themed hover popup for browse results (album + stars after 1s
        // dwell). Custom window so we can match the plugin theme colors
        // and clamp the position to the plugin window bounds.
        ensureHoverPopupClass (p->hInstance);
        p->hHoverPopup = CreateWindowExW (WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                          HOVER_POPUP_CLASS, nullptr,
                                          WS_POPUP,
                                          0, 0, 240, 60,
                                          hwnd, nullptr, p->hInstance, p);
        if (p->hBrowseList)
            SetWindowSubclass (p->hBrowseList, browseHoverSubclassProc, 1, (DWORD_PTR) p);
        if (p->hResultsList)
            SetWindowSubclass (p->hResultsList, resultsListSubclassProc, 1, (DWORD_PTR) p);
        if (p->hCandList)
            SetWindowSubclass (p->hCandList, candidatesListSubclassProc, 1, (DWORD_PTR) p);
        if (p->hBtnAddEnd)
            SetWindowSubclass (p->hBtnAddEnd, addButtonSubclassProc, 1, (DWORD_PTR) p);
        if (p->hEditTitle)
            SetWindowSubclass (p->hEditTitle,  editEscapeSubclassProc, 1, (DWORD_PTR) p);
        if (p->hEditArtist)
            SetWindowSubclass (p->hEditArtist, editEscapeSubclassProc, 1, (DWORD_PTR) p);
        if (p->hEditYear)
            SetWindowSubclass (p->hEditYear,   editEscapeSubclassProc, 1, (DWORD_PTR) p);

        // Sync year toggle text — shows "YEAR \u00B1N" when enabled, "YEAR OFF"
        // when disabled. The actual string is also recomputed on every draw
        // in WM_DRAWITEM so any future yearRange change repaints correctly.
        {
            wchar_t tb[32] = {};
            if (p->filterUseYearRange)
                wsprintfW (tb, L"YEAR \u00B1%d", p->yearRange);
            else
                wcscpy_s (tb, L"YEAR OFF");
            SetWindowTextW (p->hBtnYearToggle, tb);
        }

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
        HDC winDC = BeginPaint (hwnd, &ps);

        RECT clientR;
        GetClientRect (hwnd, &clientR);

        // Double-buffer: paint into an off-screen memDC first, then BitBlt
        // the finished frame to the window DC in one shot. Eliminates the
        // intermediate white flash from the GDI+ logo DrawImage on Settings
        // and reduces flicker everywhere else the parent repaints behind
        // WS_CLIPCHILDREN'd owner-draw children.
        HDC     memDC  = CreateCompatibleDC (winDC);
        HBITMAP memBmp = CreateCompatibleBitmap (winDC, clientR.right, clientR.bottom);
        HBITMAP oldBmp = (HBITMAP) SelectObject (memDC, memBmp);
        HDC hdc = memDC;

        fillRect (hdc, clientR, TCol::bg);

        // Top bar
        RECT topR { 0, 0, DLG_W, TOP_H };
        fillRect (hdc, topR, TCol::panel);

        if (!p)
        {
            BitBlt (winDC, 0, 0, clientR.right, clientR.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject (memDC, oldBmp);
            DeleteObject (memBmp);
            DeleteDC (memDC);
            EndPaint (hwnd, &ps);
            return 0;
        }

        // "Tiger Tanda" brand in top bar
        RECT brandR { PAD, 0, DLG_W / 2, TOP_H };
        drawText (hdc, brandR, L"Tiger Tanda", TCol::accent, p->fontTitle,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT bodyR { 0, TOP_H, DLG_W, DLG_H };
        fillRect (hdc, bodyR, TCol::bg);

        // Main view
        if (p->activeTab == 0)
        {
            const int COL_GAP = 4;
            const int leftW = DLG_W * LEFT_COL_PCT / 100;
            const int lx = PAD;
            const int lw = leftW - PAD - COL_GAP / 2;
            const int rx = leftW + COL_GAP / 2;
            const int rw = DLG_W - leftW - PAD - COL_GAP / 2;

            // Metadata load failure banner — paint first so column headers
            // paint on top if overlapping (shouldn't happen since applyLayout
            // already shifted everything down by META_BANNER_H).
            if (p->metadataLoadFailed
                && p->metaBannerRect.right > p->metaBannerRect.left)
            {
                fillRect (hdc, p->metaBannerRect, TCol::filterActive);
                RECT bTxt { p->metaBannerRect.left + 10, p->metaBannerRect.top,
                            p->metaBannerRect.right - 10, p->metaBannerRect.bottom };
                drawText (hdc, bTxt,
                          L"\u26A0  metadata.csv not found \u2014 check Settings",
                          TCol::textBright, p->fontBold,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            // Left column headers — aligned with usable width (reserving scrollbar space)
            const int sbW = GetSystemMetrics (SM_CXVSCROLL);
            const int usableW = lw - sbW;
            // columnHeaderY is computed once in applyLayout; subtract 2 to
            // nudge the painted label text up by 2px (matches historical
            // offset from the search row).
            int headerY = p->columnHeaderY - 2;
            const int gap = 4;
            const int titleW = (usableW - YEAR_COL_W - gap * 2) * 55 / 100;
            const int artistW = usableW - titleW - YEAR_COL_W - gap * 2;
            RECT htR { lx, headerY, lx + titleW, headerY + 12 };
            RECT haR { lx + titleW + gap, headerY, lx + titleW + gap + artistW, headerY + 12 };
            RECT hyR { lx + usableW - YEAR_COL_W, headerY, lx + usableW, headerY + 12 };
            drawText (hdc, htR, L"TITLE",  TCol::textDim, p->fontSmallBold, DT_LEFT | DT_TOP | DT_SINGLELINE);
            drawText (hdc, haR, L"ARTIST", TCol::textDim, p->fontSmallBold, DT_LEFT | DT_TOP | DT_SINGLELINE);
            drawText (hdc, hyR, L"YEAR",   TCol::textDim, p->fontSmallBold, DT_CENTER | DT_TOP | DT_SINGLELINE);

            // "MATCHES (N)" header — use the Y cached by applyLayout so the
            // math stays in exactly one place.
            std::wstring matchLabel = L"MATCHES (" + std::to_wstring (p->results.size()) + L")";
            RECT mhR { lx, p->matchHeaderY, lx + lw, p->matchHeaderY + 12 };
            drawText (hdc, mhR, matchLabel, TCol::textDim, p->fontSmallBold, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // Detail box — no header label above it, the box's track title
            // row is the header. Flush against the top bar (plus banner).
            int detailY = TOP_H + (p->metadataLoadFailed ? META_BANNER_H : 0);
            RECT detR { rx, detailY, rx + rw, detailY + DETAIL_BOX_H };
            if (p->selectedResultIdx >= 0 && p->selectedResultIdx < (int) p->results.size())
                drawResultDetailBox (hdc, detR, p->results[p->selectedResultIdx],
                                     p->fontBold, p->fontNormal, p->fontSmall);
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
                drawText (hdc, txtR, L"Select a match to see details and library results", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            // "VDJ BROWSER RESULTS" header — slim painted label spanning
            // the full right-column width. (FIND IN VDJ button removed.)
            RECT bhR { rx, p->browseResultsHeaderY,
                       rx + rw, p->browseResultsHeaderY + 16 };
            drawText (hdc, bhR, L"VDJ BROWSER RESULTS", TCol::textDim, p->fontSmallBold,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // When browseItems is empty the listbox is hidden via
            // syncBrowseListVisibility, leaving the main window background
            // in its place. Under the new workflow, clicking or arrow-keying
            // a match fires the search immediately (or within 250ms), so
            // the "no selection / selected but not yet searched" states are
            // only briefly visible — the detail box carries the instruction
            // text in those cases. Only render the placeholder card while a
            // search is actively pending or when a search returned no hits.
            if (p->browseItems.empty() && p->hBrowseList
                && (p->smartSearchPending || p->smartSearchNoResults))
            {
                RECT blR;
                GetWindowRect (p->hBrowseList, &blR);
                POINT tl = { blR.left, blR.top };
                POINT br = { blR.right, blR.bottom };
                ScreenToClient (hwnd, &tl);
                ScreenToClient (hwnd, &br);
                RECT placeholder { tl.x, tl.y, br.x, br.y };

                // Card background + border
                fillRect (hdc, placeholder, TCol::card);
                HPEN pp = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldPp = (HPEN) SelectObject (hdc, pp);
                HBRUSH oldB = (HBRUSH) SelectObject (hdc, GetStockObject (NULL_BRUSH));
                Rectangle (hdc, placeholder.left, placeholder.top,
                           placeholder.right, placeholder.bottom);
                SelectObject (hdc, oldPp);
                SelectObject (hdc, oldB);
                DeleteObject (pp);

                const wchar_t* primary = nullptr;
                const wchar_t* secondary = nullptr;
                if (p->smartSearchPending)
                {
                    primary   = L"Searching VDJ library\u2026";
                    secondary = L"(this may take a moment)";
                }
                else  // smartSearchNoResults
                {
                    primary   = L"No matches found in VDJ library";
                    secondary = L"Try a different match or check library folder";
                }

                // Layout: two centered lines, primary above (bold) and
                // secondary below (dim small).
                const int pH    = placeholder.bottom - placeholder.top;
                const int lineG = 4;
                const int primH = 18;
                const int secH  = 14;
                int blockH = primH + lineG + secH;
                int yTop   = placeholder.top + (pH - blockH) / 2;

                RECT r1 { placeholder.left + 8, yTop,
                          placeholder.right - 8, yTop + primH };
                drawText (hdc, r1, primary, TCol::textBright, p->fontBold,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                RECT r2 { placeholder.left + 8, yTop + primH + lineG,
                          placeholder.right - 8, yTop + primH + lineG + secH };
                drawText (hdc, r2, secondary, TCol::textDim, p->fontSmall,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Prelisten waveform
            RECT wr = p->prelistenWaveRect;
            if (wr.right > wr.left)
                drawPrelistenWave (hdc, wr, p->prelistenWaveBins, p->prelistenPos);
        }

        // Settings view — Option B two-column layout:
        //   Left  (50%): SEARCH FILTERS (ARTISTS / YEAR RANGE / OTHER) + logo
        //   Right (50%): HOW IT WORKS sections all visible at once
        //
        // The Tiger Tanda brand stays in the top bar. Sub-group header Ys
        // are pulled from the plugin struct (computed in applyLayout) so
        // the painted headers line up with the control rows.
        if (p->activeTab == 1)
        {
            const int COL_GAP   = 10;
            const int settingsL = DLG_W * 50 / 100;
            const int lx        = PAD;
            const int lw        = settingsL - PAD;
            const int rx        = settingsL + COL_GAP / 2;
            const int rw        = DLG_W - PAD - rx;

            // Helper: section header — matches the Tanda tab style
            // (TCol::textDim + fontSmall, uppercase, no horizontal rule).
            // Returns the Y below the label (14px label height + 4px gap).
            auto drawSectionHeader = [&] (int x, int y, int w, const wchar_t* label) -> int
            {
                RECT hR { x, y, x + w, y + 14 };
                drawText (hdc, hR, label, TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_TOP | DT_SINGLELINE);
                return y + 14 + 4;
            };

            // Main header (bold, bright) — used for FILTERS on the left and
            // HOW IT WORKS on the right. Slightly taller line, accent color
            // so it reads as a top-level divider.
            auto drawMainHeader = [&] (int x, int y, int w, const wchar_t* label) -> int
            {
                RECT hR { x, y, x + w, y + 16 };
                drawText (hdc, hR, label, TCol::textBright, p->fontBold,
                          DT_LEFT | DT_TOP | DT_SINGLELINE);
                return y + 16 + 4;
            };

            // ── Left column: FILTERS main header + sub-group headers ─────
            // Sub-group Ys come from applyLayout so painted labels line up
            // exactly with their control rows.
            drawMainHeader   (lx, p->settingsMainHeaderY,    lw, L"FILTERS");
            drawSectionHeader (lx, p->settingsArtistsHeaderY, lw, L"DEFAULTS");
            // Year row sits next to GENRE inside DEFAULTS, no own sub-header.
            drawSectionHeader (lx, p->settingsOtherHeaderY,   lw, L"OTHER");

            // ── Tiger Tag logo (lazy-loaded on first paint) ───────────────
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

            if (p->logoImage && p->settingsLogoH > 0)
            {
                auto* img = reinterpret_cast<Gdiplus::Image*> (p->logoImage);
                UINT imgW = img->GetWidth();
                UINT imgH = img->GetHeight();
                if (imgW > 0 && imgH > 0)
                {
                    const int logoMaxW = lw;
                    int dstH = p->settingsLogoH;
                    int dstW = (int) ((float) imgW / imgH * dstH);
                    if (dstW > logoMaxW) { dstW = logoMaxW; dstH = (int) ((float) imgH / imgW * dstW); }
                    int dstX = lx + (logoMaxW - dstW) / 2;
                    int dstY = p->settingsLogoY + (p->settingsLogoH - dstH) / 2;
                    Gdiplus::Graphics g (hdc);
                    g.SetInterpolationMode (Gdiplus::InterpolationModeHighQualityBicubic);
                    g.DrawImage (img, dstX, dstY, dstW, dstH);
                }
            }

            // ── Right column: HOW IT WORKS (Option B — all sections) ──────
            int ry = TOP_H + PAD;
            ry = drawMainHeader (rx, ry, rw, L"HOW IT WORKS");
            ry += 2;

            // Each section: dim uppercase sub-title matching the unified
            // Tanda-style header, then bullet lines in normal text.
            struct HowSection {
                const wchar_t* title;
                // Up to 6 bullet lines + nullptr terminator. The render loop
                // below stops on the first nullptr, so sections with fewer
                // bullets just leave the tail entries unset (implicitly
                // nullptr per aggregate-init rules).
                const wchar_t* bullets[7];
            };
            static const HowSection kSections[] = {
                { L"TRACK",    { L"\u2022  Browse a track in VDJ. Add or update search with inputs",
                                 L"\u2022  Matches to database. Top 3 presented, with first selected",
                                 nullptr } },
                { L"TANDA MATCHES",  { L"\u2022  Similar tracks presented based on filters.",
                                 L"\u2022  Customize filters in setting tab",
                                 L"\u2022  Press enter or click to search your VDJ library",
                                 nullptr } },
                { L"BROWSER RESULTS",  { L"\u2022  Top 4 browser results presented",
                                 L"\u2022  Hover over for additional info",
                                 L"\u2022  ADD \u2192 automix  \u00B7  Right-click ADD \u2192 sidelist",
                                 nullptr } },
                { L"FILTERS",  { L"\u2022  ARTIST / SINGER filter based on bandleader and singer",
                                 L"\u2022  GENRE matches to Tango, Vals, Milonga",
                                 L"\u2022  YEAR limits matches to \u00B1N years of the pick",
                                 L"\u2022  LABEL \u2192 Record label, GROUPING \u2192 Similar era",
                                 L"\u2022  ORCHESTRA \u2192 orchestra name, TRACK \u2192 track name",
                                 nullptr } },
            };
            const int secGap  = 4;   // space between sections
            const int titleH  = 14;  // section title height
            const int lineH   = 14;  // bullet line height
            for (const auto& sec : kSections)
            {
                // Section sub-title — unified Tanda-style header
                RECT tR { rx, ry, rx + rw, ry + titleH };
                drawText (hdc, tR, sec.title, TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_TOP | DT_SINGLELINE);
                ry += titleH + 1;
                for (const wchar_t* b : sec.bullets)
                {
                    if (!b) break;
                    RECT bR { rx + 6, ry, rx + rw, ry + lineH };
                    drawText (hdc, bR, b, TCol::textNormal, p->fontSmall,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    ry += lineH;
                }
                ry += secGap;
            }

            // Brand is main-view only — Settings tab has no bottom brand row.
        }

        // Flush the off-screen buffer to the window DC in one shot.
        BitBlt (winDC, 0, 0, clientR.right, clientR.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject (memDC, oldBmp);
        DeleteObject (memBmp);
        DeleteDC (memDC);
        EndPaint (hwnd, &ps);
        return 0;
    }

    // ── Timer: browser/deck polling + visibility sync ────────────────────────
    case WM_TIMER:
    {
        if (!p) break;

        // ── Hover popup dwell timer (fires 1 second after a new row hover) ──
        if (wParam == TIMER_HOVER_POPUP)
        {
            KillTimer (hwnd, TIMER_HOVER_POPUP);
            showHoverPopupNow (p);
            return 0;
        }

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
            // Refresh ADD/Prelisten enable state — browseItems may have
            // been populated (or cleared) by runSmartSearch. ADD / Prelisten
            // stay always-enabled (see WM_CREATE comment) — just repaint so
            // their owner-draw dim/bright state tracks the new browse state.
            if (p->hBtnAddEnd)    InvalidateRect (p->hBtnAddEnd,    nullptr, FALSE);
            if (p->hBtnPrelisten) InvalidateRect (p->hBtnPrelisten, nullptr, FALSE);
            return 0;
        }

        // ── Match-select debounce (keyboard arrows through the matches list) ─
        if (wParam == TIMER_MATCH_SELECT)
        {
            KillTimer (hwnd, TIMER_MATCH_SELECT);
            if (p->selectedResultIdx >= 0
                && p->selectedResultIdx < (int) p->results.size()
                && !p->smartSearchPending)
            {
                // Force re-fire even if the target matches the last search —
                // the user explicitly settled on this row after the debounce.
                p->lastSmartSearchTitle.clear();
                p->lastSmartSearchArtist.clear();
                p->triggerBrowserSearch (p->results[p->selectedResultIdx]);
            }
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

        // Skip all VDJ queries while a browser search is in flight — the
        // polling would otherwise read stale search-results state and try
        // to re-identify, fighting the smart-search workflow.
        if (p->smartSearchPending)
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
        if (!pollFolder.empty() && !p->smartSearchPending && !p->searchLocked)
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

        // Result list selection — fires smart search immediately on mouse
        // click, or after a 250ms debounce on keyboard arrow navigation so
        // fast arrow scrolls don't hammer VDJ.
        case IDC_RESULTS_LIST:
            if (notifCode == LBN_SELCHANGE)
            {
                // A fresh selection always cancels any pending debounced
                // search — whether we immediately fire a new one (mouse)
                // or schedule another debounce (keyboard).
                KillTimer (hwnd, TIMER_MATCH_SELECT);

                int sel = (int) SendMessageW (p->hResultsList, LB_GETCURSEL, 0, 0);
                if (sel < 0 || sel >= (int) p->results.size())
                {
                    // Out-of-range / empty list: just clear state and bail.
                    p->selectedResultIdx = -1;
                    if (p->hResultsList) InvalidateRect (p->hResultsList, nullptr, FALSE);
                    if (p->hDlg)         InvalidateRect (p->hDlg,         nullptr, FALSE);
                    break;
                }

                // Same-row guard: Windows can re-fire LBN_SELCHANGE without
                // an actual change (e.g., when focus returns to the listbox).
                // Don't re-fire the search in that case.
                if (sel == p->selectedResultIdx)
                    break;

                p->selectedResultIdx = sel;

                // Bump the smart-search token so any in-flight runSmartSearch
                // will discard its results when it reaches the token check.
                p->smartSearchActiveToken = ++p->smartSearchToken;
                p->smartSearchNoResults = false;

                // Clear stale browse results from the previous match.
                if (!p->browseItems.empty())
                {
                    p->browseItems.clear();
                    p->selectedBrowseIdx = -1;
                    if (p->hBrowseList)
                        SendMessageW (p->hBrowseList, LB_RESETCONTENT, 0, 0);
                }
                // Browse list just got cleared — ADD / Prelisten visually
                // dim via their owner-draw `hasSel` check. They stay always-
                // enabled so tooltips + right-click keep working (see
                // WM_CREATE note).
                if (p->hBtnAddEnd)    InvalidateRect (p->hBtnAddEnd,    nullptr, FALSE);
                if (p->hBtnPrelisten) InvalidateRect (p->hBtnPrelisten, nullptr, FALSE);
                p->syncBrowseListVisibility();

                // Fire the search: mouse click → immediate, keyboard → debounced.
                if (p->resultsLastInputWasMouse)
                {
                    p->resultsLastInputWasMouse = false;
                    if (!p->smartSearchPending)
                    {
                        // Force re-fire — user explicitly clicked the row.
                        p->lastSmartSearchTitle.clear();
                        p->lastSmartSearchArtist.clear();
                        p->triggerBrowserSearch (p->results[sel]);
                    }
                }
                else
                {
                    SetTimer (hwnd, TIMER_MATCH_SELECT, 250, nullptr);
                }

                if (p->hResultsList) InvalidateRect (p->hResultsList, nullptr, FALSE);
                if (p->hDlg)         InvalidateRect (p->hDlg,         nullptr, FALSE);
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

        // Prelisten toggle — ignore click if no browse result selected
        case IDC_BTN_PRELISTEN:
            if (p->selectedBrowseIdx < 0
                || p->selectedBrowseIdx >= (int) p->browseItems.size())
                break;
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
                bool hasValidBrowseSel = (sel >= 0 && sel < (int) p->browseItems.size());
                if (hasValidBrowseSel)
                {
                    p->selectedBrowseIdx = sel;
                    InvalidateRect (p->hBrowseList, nullptr, FALSE);
                    // Buttons stay always-enabled for tooltips + right-click;
                    // repaint so their owner-draw `hasSel` branch picks the
                    // active (non-dim) color.
                    if (p->hBtnAddEnd)    InvalidateRect (p->hBtnAddEnd,    nullptr, FALSE);
                    if (p->hBtnPrelisten) InvalidateRect (p->hBtnPrelisten, nullptr, FALSE);

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

        // Settings: toggle year range on/off (dynamic label + color feedback)
        case IDC_BTN_YEAR_TOGGLE:
        {
            p->filterUseYearRange = !p->filterUseYearRange;
            wchar_t tb[32] = {};
            if (p->filterUseYearRange)
                wsprintfW (tb, L"YEAR \u00B1%d", p->yearRange);
            else
                wcscpy_s (tb, L"YEAR OFF");
            if (p->hBtnYearToggle) SetWindowTextW (p->hBtnYearToggle, tb);
            if (p->hBtnYearToggle) InvalidateRect (p->hBtnYearToggle, nullptr, FALSE);
            if (p->hBtnYearRange)  InvalidateRect (p->hBtnYearRange,  nullptr, FALSE);
            if (p->hBtnYearMinus)  InvalidateRect (p->hBtnYearMinus,  nullptr, FALSE);
            if (p->hBtnYearPlus)   InvalidateRect (p->hBtnYearPlus,   nullptr, FALSE);
            p->saveSettings();
            if (p->confirmedIdx >= 0) p->runTandaSearch();
            break;
        }

        // Settings: year range spinner — minus, label (cycles as fallback), plus
        case IDC_BTN_YEAR_MINUS:
        case IDC_BTN_YEAR_PLUS:
        case IDC_BTN_YEAR_RANGE:
        {
            static const int      kYrVals[]   = { 0, 1, 2, 3, 5, 10 };
            static const wchar_t* kYrLabels[] = { L"\u00B10", L"\u00B11", L"\u00B12", L"\u00B13", L"\u00B15", L"\u00B110" };
            int curIdx = 0;
            for (int i = 0; i < 6; ++i)
                if (kYrVals[i] == p->yearRange) { curIdx = i; break; }
            if (ctrlId == IDC_BTN_YEAR_MINUS)
            {
                if (curIdx > 0) --curIdx;
            }
            else if (ctrlId == IDC_BTN_YEAR_PLUS)
            {
                if (curIdx < 5) ++curIdx;
            }
            else
            {
                // Label click: cycle forward (fallback behaviour)
                curIdx = (curIdx + 1) % 6;
            }
            p->yearRange = kYrVals[curIdx];
            if (p->hBtnYearRange) SetWindowTextW (p->hBtnYearRange, kYrLabels[curIdx]);
            if (p->hBtnYearRange) InvalidateRect (p->hBtnYearRange, nullptr, FALSE);
            // Refresh the YEAR toggle text so the live "\u00B1N" preview follows.
            if (p->filterUseYearRange)
            {
                wchar_t tb[32] = {};
                wsprintfW (tb, L"YEAR \u00B1%d", p->yearRange);
                if (p->hBtnYearToggle) SetWindowTextW (p->hBtnYearToggle, tb);
                if (p->hBtnYearToggle) InvalidateRect (p->hBtnYearToggle, nullptr, FALSE);
            }
            // Enable / disable spinner bounds
            if (p->hBtnYearMinus) EnableWindow (p->hBtnYearMinus, curIdx > 0);
            if (p->hBtnYearPlus)  EnableWindow (p->hBtnYearPlus,  curIdx < 5);
            if (p->hBtnYearMinus) InvalidateRect (p->hBtnYearMinus, nullptr, FALSE);
            if (p->hBtnYearPlus)  InvalidateRect (p->hBtnYearPlus,  nullptr, FALSE);
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

        // Lock toggle — freeze search fields on browser navigation
        case IDC_BTN_LOCK:
        {
            p->searchLocked = !p->searchLocked;
            if (p->hBtnLock)    InvalidateRect (p->hBtnLock,    nullptr, FALSE);
            // Edit text color changes via WM_CTLCOLOREDIT on next repaint
            if (p->hEditTitle)  InvalidateRect (p->hEditTitle,  nullptr, TRUE);
            if (p->hEditArtist) InvalidateRect (p->hEditArtist, nullptr, TRUE);
            if (p->hEditYear)   InvalidateRect (p->hEditYear,   nullptr, TRUE);
            break;
        }

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

            // Prelisten play/pause button — dimmed when no browse result selected
            if (di->CtlID == IDC_BTN_PRELISTEN)
            {
                bool hasSel = (p->selectedBrowseIdx >= 0
                               && p->selectedBrowseIdx < (int) p->browseItems.size());
                COLORREF bg, fg;
                if (!hasSel)
                { bg = TCol::buttonDisabled; fg = TCol::textDim; }
                else if (p->prelistenActive)
                { bg = TCol::accent;     fg = TCol::textBright; }
                else
                { bg = TCol::buttonBg;   fg = TCol::accentBrt; }
                wchar_t txt[4] = {};
                GetWindowTextW (di->hwndItem, txt, 4);
                drawOwnerButton (di, txt, bg, fg, p->fontNormal, p->hoveredBtn == di->hwndItem);
                return TRUE;
            }

            // Filter toggle buttons — flat left-aligned checkbox style:
            // ☑/☐ glyph + label, no background highlight, accent-colored
            // check + bold label when on, dim glyph + normal label when off.
            // IDC_BTN_YEAR_TOGGLE is now drawn with the same checkbox style
            // so the Settings tab reads as a single, consistent filter grid.
            if (di->CtlID == IDC_CHK_SAME_ARTIST    || di->CtlID == IDC_CHK_SAME_SINGER   ||
                di->CtlID == IDC_CHK_SAME_GROUPING  || di->CtlID == IDC_CHK_SAME_GENRE    ||
                di->CtlID == IDC_CHK_SAME_ORCHESTRA  || di->CtlID == IDC_CHK_SAME_LABEL   ||
                di->CtlID == IDC_CHK_SAME_TRACK     || di->CtlID == IDC_BTN_YEAR_TOGGLE)
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
                    case IDC_BTN_YEAR_TOGGLE:    isOn = p->filterUseYearRange;  break;
                }
                wchar_t ftxt[64] = {};
                // YEAR button label is kept stable (just "YEAR") so the
                // checkbox glyph communicates on/off — the adjacent spinner
                // shows the current ±N value.
                if (di->CtlID == IDC_BTN_YEAR_TOGGLE)
                    wcscpy_s (ftxt, L"YEAR");
                else
                    GetWindowTextW (di->hwndItem, ftxt, 64);

                bool hovered = p->hoveredBtn == di->hwndItem
                               || (di->itemState & ODS_HOTLIGHT) != 0;
                HDC fhdc = di->hDC;

                // Flat background — no on/off highlight, just a subtle
                // hover shift.
                fillRect (fhdc, di->rcItem, hovered ? TCol::buttonHover : TCol::panel);

                // Checkbox glyph column (left inset 4px, fixed 18px wide).
                RECT glyphR { di->rcItem.left + 4, di->rcItem.top,
                              di->rcItem.left + 22, di->rcItem.bottom };
                drawText (fhdc, glyphR,
                          isOn ? L"\u2611" : L"\u2610",
                          isOn ? TCol::accent : TCol::textDim,
                          p->fontBold,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                // Label column — follows the glyph with a small gap.
                RECT labelR { glyphR.right + 4, di->rcItem.top,
                              di->rcItem.right - 4, di->rcItem.bottom };
                drawText (fhdc, labelR, ftxt,
                          isOn ? TCol::textBright : TCol::textNormal,
                          isOn ? p->fontBold     : p->fontNormal,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                return TRUE;
            }

            // Year range spinner (− / +) — small square buttons.
            // Neutral (white/normal text) colors now, not accent-orange —
            // the YEAR checkbox's orange check mark is the sole "active"
            // indicator so the buttons don't need to echo the state.
            if (di->CtlID == IDC_BTN_YEAR_MINUS || di->CtlID == IDC_BTN_YEAR_PLUS)
            {
                bool active = p->filterUseYearRange;
                bool disabledState = (di->itemState & ODS_DISABLED) != 0;
                COLORREF sbg = (active && !disabledState) ? TCol::buttonBg   : TCol::buttonDisabled;
                COLORREF sfg = (active && !disabledState) ? TCol::textBright : TCol::textDim;
                wchar_t stxt[4] = {};
                GetWindowTextW (di->hwndItem, stxt, 4);
                drawOwnerButton (di, stxt, sbg, sfg, p->fontBold, p->hoveredBtn == di->hwndItem);
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
            if (di->CtlID == IDC_BTN_LOCK)
            {
                bg = p->searchLocked ? TCol::accent : TCol::buttonBg;
                fg = p->searchLocked ? TCol::textBright : TCol::textDim;
            }
            else if (di->CtlID == IDC_BTN_CLOSE)
            {
                bg = btnHovered ? RGB (200, 45, 45) : RGB (70, 28, 28);
                fg = TCol::textBright;
            }
            else if (di->CtlID == IDC_BTN_ADD_END)
            {
                bool hasSel = (p->selectedBrowseIdx >= 0
                               && p->selectedBrowseIdx < (int) p->browseItems.size());
                if (hasSel)
                { bg = RGB (28, 55, 28); fg = TCol::textBright; }
                else
                { bg = TCol::buttonDisabled; fg = TCol::textDim; }
            }
            // IDC_BTN_YEAR_TOGGLE is handled by the checkbox branch above —
            // never reaches this generic path.
            else if (di->CtlID == IDC_BTN_YEAR_RANGE)
            {
                // Neutral button background; white text when enabled so the
                // value reads clearly without introducing another orange
                // accent. The YEAR checkmark is the only orange indicator.
                bg = TCol::buttonBg;
                fg = p->filterUseYearRange ? TCol::textBright : TCol::textDim;
            }

            // Pass hover state (close button handles its own color above; others use drawOwnerButton's lighten)
            bool passHover = (di->CtlID != IDC_BTN_CLOSE) && btnHovered;
            HFONT btnFont = (di->CtlID == IDC_BTN_YEAR_TOGGLE
                          || di->CtlID == IDC_BTN_YEAR_RANGE)
                            ? p->fontSmall
                            : (di->CtlID == IDC_BTN_LOCK)
                            ? p->fontLockIcon : p->fontNormal;
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
            bool even = (di->itemID % 2 == 0);

            fillRect (hdc, r, confirmed ? TCol::selSubtle : even ? TCol::card : TCol::panel);

            if (confirmed)
            {
                RECT accentR { r.left, r.top, r.left + 3, r.bottom };
                fillRect (hdc, accentR, TCol::accent);
            }

            // Account for the scrollbar column (always present via
            // LBS_DISABLENOSCROLL) so row columns line up pixel-exact with
            // the search row / column headers above. The row's usable area
            // equals the edit row's usableW (lw - sbW), with the same title
            // / artist / year partition. Text is inset 6px on each side
            // inside each column for a subtle left margin.
            const int sbW = GetSystemMetrics (SM_CXVSCROLL);
            const int rightEdge = r.right - sbW;
            const int rw = rightEdge - r.left;
            const int gap = 4;
            const int titleColW  = (rw - YEAR_COL_W - gap * 2) * 55 / 100;
            const int artistColW =  rw - titleColW - YEAR_COL_W - gap * 2;
            const int titleX    = r.left;
            const int artistX   = titleX + titleColW + gap;
            const int yearX     = rightEdge - YEAR_COL_W;
            const int textInset = 6;

            RECT titleR  { titleX  + textInset, r.top, titleX  + titleColW  - textInset, r.bottom };
            RECT artistR { artistX + textInset, r.top, artistX + artistColW - textInset, r.bottom };
            RECT yearR   { yearX,               r.top, yearX + YEAR_COL_W,               r.bottom };

            // Title always white, same 13pt as the input boxes. When the row
            // is confirmed (selected), all three columns use the bold
            // variant for emphasis. Artist and year stay dim gray normally.
            HFONT rowFont = confirmed ? p->fontBold : p->fontNormal;
            drawText (hdc, titleR,  rec.title,         TCol::textBright, rowFont,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, artistR, formatArtist (rec), confirmed ? TCol::textBright : TCol::textDim, rowFont,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, yearR,   rec.year,           confirmed ? TCol::textBright : TCol::textDim, rowFont,
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

            fillRect (hdc, r, sel ? TCol::selSubtle : even ? TCol::card : TCol::panel);

            if (sel)
            {
                RECT accentR { r.left, r.top, r.left + 3, r.bottom };
                fillRect (hdc, accentR, TCol::accent);
            }

            // Column math mirrors the candidates list. Critical detail:
            // LBS_DISABLENOSCROLL reserves the scrollbar column in the
            // listbox client area, so r.right here is already equal to
            // (listbox width - sbW). Do NOT subtract sbW again — that would
            // shift the year column sbW pixels too far left and break
            // alignment with the search row / candidates list above.
            const int rightEdge = r.right;
            const int rw        = rightEdge - r.left;
            const int gap       = 4;
            const int titleColW  = (rw - YEAR_COL_W - gap * 2) * 55 / 100;
            const int artistColW =  rw - titleColW - YEAR_COL_W - gap * 2;
            const int titleX     = r.left;
            const int artistX    = titleX + titleColW + gap;
            const int yearX      = rightEdge - YEAR_COL_W;
            const int textInset  = 6;

            RECT titleR  { titleX  + textInset, r.top, titleX  + titleColW  - textInset, r.bottom };
            RECT artistR { artistX + textInset, r.top, artistX + artistColW - textInset, r.bottom };
            RECT yearR   { yearX,               r.top, yearX + YEAR_COL_W,               r.bottom };

            // Title is always white and 13pt. Normally unbold; bold when the
            // row is selected. Artist/year: dim normal by default; white bold
            // when selected — so the whole selected row pops together.
            HFONT rowFont = sel ? p->fontBold : p->fontNormal;
            drawText (hdc, titleR,  rec.title,         TCol::textBright, rowFont,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, artistR, formatArtist (rec), sel ? TCol::textBright : TCol::textDim, rowFont,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            std::wstring yearStr = rec.year.empty()
                ? (rec.date.size() >= 4 ? rec.date.substr (0, 4) : rec.date)
                : rec.year;
            drawText (hdc, yearR,   yearStr,            sel ? TCol::textBright : TCol::textDim, rowFont,
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

            fillRect (hdc, r, sel ? TCol::selSubtle : even ? TCol::card : TCol::panel);

            if (sel)
            {
                RECT accentR { r.left, r.top, r.left + 3, r.bottom };
                fillRect (hdc, accentR, TCol::accent);
            }

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
                fillRect (hdc, ar, TCol::waveformBg);
                HPEN pn  = CreatePen (PS_SOLID, 1, TCol::cardBorder);
                HPEN oldP = (HPEN) SelectObject (hdc, pn);
                HBRUSH oldB = (HBRUSH) SelectObject (hdc, GetStockObject (NULL_BRUSH));
                Rectangle (hdc, ar.left, ar.top, ar.right, ar.bottom);
                SelectObject (hdc, oldP);
                SelectObject (hdc, oldB);
                DeleteObject (pn);
            }

            // Text area to the left of the art. Use symmetric 6/6 padding
            // like the candidate/result lists, and use the shared YEAR_COL_W
            // so the year column width matches the other lists. The album
            // art thumbnail is on the right, so `textRight` stays clear of it.
            const int tx        = r.left + 6;
            const int textRight = artX - 6;
            const int titleW    = textRight - tx - YEAR_COL_W - 4;

            // Row 1 (top half): Title (left) + Year (right)
            int row1Top = r.top + 4;
            int row1Bot = r.top + itemH / 2;
            RECT titleR { tx,                       row1Top, tx + titleW, row1Bot };
            RECT yearR  { textRight - YEAR_COL_W,   row1Top, textRight,   row1Bot };

            drawText (hdc, titleR, bi.title, TCol::textBright, p->fontBold,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, yearR,  bi.year,  sel ? TCol::textBright : TCol::textDim, p->fontSmall,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            // Row 2 (bottom half): Artist, full width of text area
            RECT artistR { tx, row1Bot, textRight, r.bottom - 4 };
            drawText (hdc, artistR, bi.artist, sel ? TCol::textBright : TCol::textDim, p->fontSmall,
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
            SetTextColor (hdc, p->searchLocked ? TCol::textDim : TCol::textNormal);
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

        // Metadata banner click → jump to Settings tab so the user can fix
        // the metadata folder path.
        if (p->metadataLoadFailed && p->activeTab == 0
            && p->metaBannerRect.right > p->metaBannerRect.left
            && PtInRect (&p->metaBannerRect, pt))
        {
            p->activeTab = 1;
            p->saveSettings();
            applyLayout (p, hwnd);
            return 0;
        }

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
        KillTimer (hwnd, TIMER_HOVER_POPUP);
        if (p && p->hTooltip)    { DestroyWindow (p->hTooltip);    p->hTooltip = nullptr; }
        if (p && p->hHoverPopup) { DestroyWindow (p->hHoverPopup); p->hHoverPopup = nullptr; }
        // Unregister the hover popup window class (registered lazily in
        // ensureHoverPopupClass). The main WND_CLASS is unregistered by
        // TigerTandaPlugin::Release(). Guarded so we only call it once
        // per process lifetime — a second call would silently fail.
        {
            static bool hoverPopupClassUnregistered = false;
            if (!hoverPopupClassUnregistered)
            {
                UnregisterClassW (L"TigerTandaHoverPopup", GetModuleHandleW (nullptr));
                hoverPopupClassUnregistered = true;
            }
        }
        return 0;
    }

    return DefWindowProcW (hwnd, msg, wParam, lParam);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI wrapper implementations (Windows)
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::uiResetCandidatesList()
{
    if (hCandList)
        SendMessageW (hCandList, LB_RESETCONTENT, 0, 0);
}

void TigerTandaPlugin::uiAddCandidateRow()
{
    if (hCandList)
        SendMessageW (hCandList, LB_ADDSTRING, 0, (LPARAM) L"");
}

void TigerTandaPlugin::uiInvalidateCandidates()
{
    if (hCandList)
        InvalidateRect (hCandList, nullptr, FALSE);
}

void TigerTandaPlugin::uiResetResultsList()
{
    if (hResultsList)
        SendMessageW (hResultsList, LB_RESETCONTENT, 0, 0);
}

void TigerTandaPlugin::uiAddResultRow()
{
    if (hResultsList)
        SendMessageW (hResultsList, LB_ADDSTRING, 0, (LPARAM) L"");
}

void TigerTandaPlugin::uiResetBrowseList()
{
    if (hBrowseList)
        SendMessageW (hBrowseList, LB_RESETCONTENT, 0, 0);
}

void TigerTandaPlugin::uiAddBrowseRow()
{
    if (hBrowseList)
        SendMessageW (hBrowseList, LB_ADDSTRING, 0, (LPARAM) L"");
}

void TigerTandaPlugin::uiInvalidateDialog()
{
    if (hDlg)
        InvalidateRect (hDlg, nullptr, FALSE);
}

void TigerTandaPlugin::uiSetTimer (int timerId, int ms)
{
    if (hDlg)
        SetTimer (hDlg, (UINT_PTR) timerId, (UINT) ms, nullptr);
}

void TigerTandaPlugin::uiKillTimer (int timerId)
{
    if (hDlg)
        KillTimer (hDlg, (UINT_PTR) timerId);
}

void TigerTandaPlugin::uiSetEditText (int ctrlId, const std::wstring& text)
{
    HWND h = nullptr;
    switch (ctrlId)
    {
        case IDC_EDIT_TITLE:  h = hEditTitle;  break;
        case IDC_EDIT_ARTIST: h = hEditArtist; break;
        case IDC_EDIT_YEAR:   h = hEditYear;   break;
    }
    if (h) SetWindowTextW (h, text.c_str());
}
