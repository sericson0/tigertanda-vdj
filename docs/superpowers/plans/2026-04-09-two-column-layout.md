# Two-Column Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 4-tab UI with a single two-column view (search+matches left, detail+browse right) plus Settings.

**Architecture:** Widen window to ~700x380, remove Track/Matches/Browse tab buttons, merge all content into one main view (activeTab=0) with Settings as activeTab=1. Add year input, live search via EN_CHANGE debounce, and auto VDJ search on match selection.

**Tech Stack:** Win32 GDI, C++, VDJ plugin SDK

---

### Task 1: Update header — layout constants, control IDs, HWNDs

**Files:**
- Modify: `Source/TigerTanda.h`

- [ ] **Step 1: Update layout constants**

```cpp
inline constexpr int DLG_H          = 380;
inline constexpr int DLG_W          = 700;
inline constexpr int TOP_H          = 36;    // slightly shorter top bar
inline constexpr int PAD            = 8;
inline constexpr int BTN_H          = 24;
inline constexpr int EDIT_H         = 24;
inline constexpr int BRAND_H        = 26;
inline constexpr int CAND_ITEM_H    = 24;    // single-row, aligned with inputs (was 46)
inline constexpr int TAB_BTN_H      = 20;
inline constexpr int RESULT_ITEM_H  = 22;
inline constexpr int BROWSE_ITEM_H  = 24;
inline constexpr int DETAIL_BOX_H   = 82;
inline constexpr int PRE_WAVE_H     = 20;
inline constexpr int TRACK_SEARCH_GAP = 4;   // reduced gap (was 14)
inline constexpr int LEFT_COL_PCT   = 60;    // left column percentage
```

- [ ] **Step 2: Add new control ID and timer**

Add to `CtrlId` enum:
```cpp
    IDC_EDIT_YEAR          = 2104,
```

Add debounce timer:
```cpp
inline constexpr UINT_PTR TIMER_SEARCH_DEBOUNCE = 4;
```

- [ ] **Step 3: Remove obsolete control IDs**

Remove from `CtrlId` enum:
```cpp
    IDC_BTN_SEARCH         = 2103,    // REMOVE
    IDC_BTN_TAB_TRACK      = 2802,    // REMOVE
    IDC_BTN_TAB_MATCHES    = 2803,    // REMOVE
    IDC_BTN_TAB_BROWSE     = 2804,    // REMOVE
    IDC_BTN_SEARCH_VDJ     = 2703,    // REMOVE
```

- [ ] **Step 4: Update HWND members**

Add:
```cpp
    HWND hEditYear         = nullptr;
```

Remove:
```cpp
    HWND hBtnSearch        = nullptr;     // REMOVE
    HWND hBtnTabTrack      = nullptr;     // REMOVE
    HWND hBtnTabMatches    = nullptr;     // REMOVE
    HWND hBtnTabBrowse     = nullptr;     // REMOVE
    HWND hBtnSearchVdj     = nullptr;     // REMOVE
```

- [ ] **Step 5: Update activeTab default and comment**

Change:
```cpp
    int  activeTab    = 0;    // 0=Main, 1=Settings
```

- [ ] **Step 6: Commit**

```bash
git add Source/TigerTanda.h
git commit -m "refactor: update header for two-column layout — new constants, IDs, remove old tabs"
```

---

### Task 2: Update TigerTanda.cpp — window size, settings, cleanup

**Files:**
- Modify: `Source/TigerTanda.cpp`

- [ ] **Step 1: Update activeTab range in loadSettings**

In `loadSettings()`, change the activeTab validation:
```cpp
            else if (key == "activeTab")
            {
                activeTab = std::stoi (val);
                if (activeTab < 0 || activeTab > 1) activeTab = 0;
            }
```

- [ ] **Step 2: Commit**

```bash
git add Source/TigerTanda.cpp
git commit -m "fix: update activeTab range for 2-view layout (0=Main, 1=Settings)"
```

---

### Task 3: Update TigerTandaMatching.cpp — resetAll clears year edit

**Files:**
- Modify: `Source/TigerTandaMatching.cpp`

- [ ] **Step 1: Add year edit clear to resetAll**

In `resetAll()`, add after the artist edit clear:
```cpp
    if (hEditYear)    SetWindowTextW (hEditYear,   L"");
```

- [ ] **Step 2: Commit**

```bash
git add Source/TigerTandaMatching.cpp
git commit -m "fix: clear year edit on resetAll"
```

---

### Task 4: Rewrite WM_CREATE — new controls, remove old ones

**Files:**
- Modify: `Source/TigerTandaUI.cpp` (WM_CREATE section, ~lines 530–673)

- [ ] **Step 1: Replace top bar controls**

Remove creation of:
- `hBtnTabTrack` (IDC_BTN_TAB_TRACK)
- `hBtnTabMatches` (IDC_BTN_TAB_MATCHES)
- `hBtnTabBrowse` (IDC_BTN_TAB_BROWSE)

Keep:
- `hBtnClose` (IDC_BTN_CLOSE)
- `hBtnTabSettings` (IDC_BTN_TAB_SETTINGS) — now toggles between Main/Settings

- [ ] **Step 2: Replace search row controls**

Remove creation of `hBtnSearch` (IDC_BTN_SEARCH).

Add year edit after artist edit:
```cpp
        p->hEditYear = CreateWindowExW (WS_EX_CLIENTEDGE, L"EDIT", L"",
                                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER,
                                        0, 0, 10, 10, hwnd,
                                        (HMENU) IDC_EDIT_YEAR, nullptr, nullptr);
        SendMessageW (p->hEditYear, EM_SETCUEBANNER, TRUE, (LPARAM) L"Year");
```

- [ ] **Step 3: Remove FIND TRACK button creation**

Remove creation of `hBtnSearchVdj` (IDC_BTN_SEARCH_VDJ).

- [ ] **Step 4: Update tooltip registrations**

Remove tooltips for removed buttons (`hBtnSearch`, `hBtnSearchVdj`, `hBtnTabTrack` etc).

Add tooltip for year edit if desired (optional, can skip).

- [ ] **Step 5: Update WM_SETCURSOR tracked buttons array**

Remove `p->hBtnSearch`, `p->hBtnSearchVdj`, `p->hBtnTabTrack`, `p->hBtnTabMatches`, `p->hBtnTabBrowse` from the `tracked[]` array in WM_SETCURSOR.

- [ ] **Step 6: Commit**

```bash
git add Source/TigerTandaUI.cpp
git commit -m "refactor: WM_CREATE — add year edit, remove old tab/search/find buttons"
```

---

### Task 5: Rewrite applyLayout for two-column layout

**Files:**
- Modify: `Source/TigerTandaUI.cpp` (applyLayout function, ~lines 277–431)

- [ ] **Step 1: Replace the entire applyLayout function**

```cpp
static void applyLayout (TigerTandaPlugin* p, HWND hwnd)
{
    if (!p || !hwnd) return;

    SetWindowPos (hwnd, nullptr, 0, 0, DLG_W, DLG_H,
                  SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    const int bodyY = TOP_H;
    const int bodyH = DLG_H - TOP_H;
    const int leftW = DLG_W * LEFT_COL_PCT / 100;
    const int rightW = DLG_W - leftW;

    const bool showMain = (p->activeTab == 0);
    const bool showS    = (p->activeTab == 1);

    auto showCtrl = [](HWND h, bool vis)
    {
        if (h) ShowWindow (h, vis ? SW_SHOW : SW_HIDE);
    };

    // ── Top bar ───────────────────────────────────────────────────────────────
    // Settings gear — right of center, left of close
    const int topY = (TOP_H - TAB_BTN_H) / 2;
    MoveWindow (p->hBtnClose, DLG_W - 26, topY, 22, TAB_BTN_H, FALSE);
    ShowWindow (p->hBtnClose, SW_SHOW);
    MoveWindow (p->hBtnTabSettings, DLG_W - 50, topY, 22, TAB_BTN_H, FALSE);
    ShowWindow (p->hBtnTabSettings, SW_SHOW);

    // ── Main view ─────────────────────────────────────────────────────────────
    if (showMain)
    {
        const int lx = PAD;
        const int lw = leftW - PAD * 2;

        // Column headers are painted in WM_PAINT, not controls

        // Search inputs row
        int ly = bodyY + PAD + 14;  // 14px for painted column headers
        const int gap = 4;
        const int yearW = 52;
        const int titleW = (lw - yearW - gap * 2) * 55 / 100;
        const int artistW = lw - titleW - yearW - gap * 2;
        MoveWindow (p->hEditTitle,  lx,                       ly, titleW,  EDIT_H, FALSE);
        MoveWindow (p->hEditArtist, lx + titleW + gap,        ly, artistW, EDIT_H, FALSE);
        MoveWindow (p->hEditYear,   lx + lw - yearW,          ly, yearW,   EDIT_H, FALSE);
        ly += EDIT_H + TRACK_SEARCH_GAP;

        // Candidates list (2 items max, small height)
        int candH = CAND_ITEM_H * 2 + 2;
        MoveWindow (p->hCandList, lx, ly, lw, candH, FALSE);
        ly += candH + 8;  // 8px for separator (painted)

        // Results list — fills remaining left column
        int resultsBot = DLG_H - PAD;
        MoveWindow (p->hResultsList, lx, ly + 14, lw, resultsBot - ly - 14, FALSE);  // 14px for header

        // Right column
        const int rx = leftW + PAD;
        const int rw = rightW - PAD * 2;
        int ry = bodyY + PAD + 14;  // 14px for "SELECTED TRACK" header

        // Detail box
        MoveWindow (p->hResultsList, lx, ly + 14, lw, resultsBot - ly - 14, FALSE);
        // (detail box is painted, not a control)

        // Browse list — below detail box
        int detailBot = ry + DETAIL_BOX_H;
        int browseHeaderH = 14 + 4;  // "VDJ BROWSER RESULTS" header + gap
        int browseTop = detailBot + browseHeaderH;
        int preRowY = DLG_H - PAD - BTN_H;
        int browseH = preRowY - 6 - browseTop;
        MoveWindow (p->hBrowseList, rx, browseTop, rw, browseH, FALSE);

        // Prelisten + ADD row
        const int preBtnW = 28;
        const int addBtnW = 72;
        MoveWindow (p->hBtnPrelisten, rx, preRowY, preBtnW, BTN_H, FALSE);
        MoveWindow (p->hBtnAddEnd, rx + rw - addBtnW, preRowY, addBtnW, BTN_H, FALSE);

        // Store waveform rect
        p->prelistenWaveRect = { rx + preBtnW + 4, preRowY,
                                 rx + rw - addBtnW - 4, preRowY + BTN_H };
    }
    showCtrl (p->hEditTitle,     showMain);
    showCtrl (p->hEditArtist,    showMain);
    showCtrl (p->hEditYear,      showMain);
    showCtrl (p->hCandList,      showMain);
    showCtrl (p->hResultsList,   showMain);
    showCtrl (p->hBrowseList,    showMain);
    showCtrl (p->hBtnPrelisten,  showMain);
    showCtrl (p->hBtnAddEnd,     showMain);

    // ── Settings view ─────────────────────────────────────────────────────────
    if (showS)
    {
        // Settings uses full width (same as before but using full DLG_W)
        const int lx = PAD;
        const int lw = DLG_W - PAD * 2;
        const int btnH = BTN_H - 4;
        const int gap  = 4;
        const int colW = (lw - gap * 2) / 3;

        int sy = bodyY + PAD - 5;

        // "How it works" sub-tabs row
        const int howTabW = lw / 5;
        for (int i = 0; i < 5; ++i)
            MoveWindow (p->hBtnHowTabs[i], lx + i * howTabW, sy, howTabW, 18, FALSE);
        sy += 18 + 4;

        sy += 60 + 13;  // skip painted content + gap

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

        // Filter Row 3: [YEAR][±N] ... [TRACK]
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
```

- [ ] **Step 2: Remove repaintTabs function**

Delete the `repaintTabs` helper — no longer needed (no tab strip to repaint).

- [ ] **Step 3: Commit**

```bash
git add Source/TigerTandaUI.cpp
git commit -m "refactor: rewrite applyLayout for two-column layout"
```

---

### Task 6: Rewrite WM_PAINT for new layout

**Files:**
- Modify: `Source/TigerTandaUI.cpp` (WM_PAINT section, ~lines 676–886)

- [ ] **Step 1: Update top bar painting**

Replace top bar fill + tab drawing with:
```cpp
        // Top bar background
        RECT topR { 0, 0, DLG_W, TOP_H };
        fillRect (hdc, topR, TCol::panel);

        // "Tiger Tanda" brand in top bar (left-aligned)
        if (p)
        {
            RECT brandR { PAD, 0, DLG_W / 2, TOP_H };
            drawText (hdc, brandR, L"Tiger Tanda", TCol::accent, p->fontTitle,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
```

- [ ] **Step 2: Paint main view sections**

For `activeTab == 0`, paint:
```cpp
        if (p && p->activeTab == 0)
        {
            const int leftW = DLG_W * LEFT_COL_PCT / 100;
            const int lx = PAD;
            const int lw = leftW - PAD * 2;
            const int rx = leftW + PAD;
            const int rw = DLG_W - leftW - PAD * 2;

            // Left column headers: TITLE / ARTIST / YEAR
            int headerY = TOP_H + PAD;
            const int gap = 4;
            const int yearW = 52;
            const int titleW = (lw - yearW - gap * 2) * 55 / 100;
            const int artistW = lw - titleW - yearW - gap * 2;
            RECT htR { lx, headerY, lx + titleW, headerY + 12 };
            RECT haR { lx + titleW + gap, headerY, lx + titleW + gap + artistW, headerY + 12 };
            RECT hyR { lx + lw - yearW, headerY, lx + lw, headerY + 12 };
            drawText (hdc, htR, L"TITLE",  TCol::textDim, p->fontSmall, DT_LEFT | DT_TOP | DT_SINGLELINE);
            drawText (hdc, haR, L"ARTIST", TCol::textDim, p->fontSmall, DT_LEFT | DT_TOP | DT_SINGLELINE);
            drawText (hdc, hyR, L"YEAR",   TCol::textDim, p->fontSmall, DT_CENTER | DT_TOP | DT_SINGLELINE);

            // Separator between candidates and matches
            int candBot = TOP_H + PAD + 14 + EDIT_H + TRACK_SEARCH_GAP + CAND_ITEM_H * 2 + 2;
            int sepY = candBot + 3;
            HPEN sep = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, sep);
            MoveToEx (hdc, lx, sepY, nullptr);
            LineTo   (hdc, lx + lw, sepY);
            SelectObject (hdc, old);
            DeleteObject (sep);

            // "MATCHES (N)" header
            int matchHeaderY = sepY + 4;
            std::wstring matchLabel = L"MATCHES (" + std::to_wstring (p->results.size()) + L")";
            RECT mhR { lx, matchHeaderY, lx + lw, matchHeaderY + 12 };
            drawText (hdc, mhR, matchLabel, TCol::textDim, p->fontSmall, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // Right column: "SELECTED TRACK" header
            int rHeaderY = TOP_H + PAD;
            RECT stR { rx, rHeaderY, rx + rw, rHeaderY + 12 };
            drawText (hdc, stR, L"SELECTED TRACK", TCol::textDim, p->fontSmall, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // Detail box
            int detailY = rHeaderY + 14;
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
                drawText (hdc, txtR, L"Select a match to see details", TCol::textDim, p->fontSmall,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            // "VDJ BROWSER RESULTS" header
            int browseHeaderY = detailY + DETAIL_BOX_H + 4;
            RECT bhR { rx, browseHeaderY, rx + rw, browseHeaderY + 12 };
            drawText (hdc, bhR, L"VDJ BROWSER RESULTS", TCol::textDim, p->fontSmall, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // Prelisten waveform
            RECT wr = p->prelistenWaveRect;
            if (wr.right > wr.left)
                drawPrelistenWave (hdc, wr, p->prelistenWaveBins, p->prelistenPos);
        }
```

- [ ] **Step 3: Keep Settings tab paint unchanged**

The existing Settings paint logic (activeTab == 3) becomes activeTab == 1. Update the condition check from `p->activeTab == 3` to `p->activeTab == 1`. Remove the brand text painting that was conditional on `activeTab != 3` — brand is now in the top bar always.

- [ ] **Step 4: Commit**

```bash
git add Source/TigerTandaUI.cpp
git commit -m "refactor: WM_PAINT for two-column layout with section headers"
```

---

### Task 7: Rewrite WM_COMMAND — live search, auto-browse, simplified tabs

**Files:**
- Modify: `Source/TigerTandaUI.cpp` (WM_COMMAND section, ~lines 1039–1253)

- [ ] **Step 1: Replace tab button handlers**

Remove handlers for `IDC_BTN_TAB_TRACK`, `IDC_BTN_TAB_MATCHES`, `IDC_BTN_TAB_BROWSE`.

Change `IDC_BTN_TAB_SETTINGS` to toggle between main and settings:
```cpp
        case IDC_BTN_TAB_SETTINGS:
            p->activeTab = (p->activeTab == 1) ? 0 : 1;
            p->saveSettings();
            applyLayout (p, hwnd);
            break;
```

- [ ] **Step 2: Add EN_CHANGE live search with debounce**

Remove the `IDC_BTN_SEARCH` handler.

Add handlers for the edit controls:
```cpp
        case IDC_EDIT_TITLE:
        case IDC_EDIT_ARTIST:
        case IDC_EDIT_YEAR:
            if (notifCode == EN_CHANGE)
                SetTimer (hwnd, TIMER_SEARCH_DEBOUNCE, 300, nullptr);
            break;
```

- [ ] **Step 3: Add debounce timer handler in WM_TIMER**

Add before the existing `TIMER_SMART_SEARCH` handler:
```cpp
        if (wParam == TIMER_SEARCH_DEBOUNCE)
        {
            KillTimer (hwnd, TIMER_SEARCH_DEBOUNCE);
            wchar_t title[512] = {}, artist[512] = {};
            GetWindowTextW (p->hEditTitle,  title,  512);
            GetWindowTextW (p->hEditArtist, artist, 512);
            p->runIdentification (title, artist);
            return 0;
        }
```

- [ ] **Step 4: Update candidate selection — no tab switch**

Change `IDC_CANDIDATES_LIST` handler to remove the auto-switch to Matches tab (they're now in the same view):
```cpp
        case IDC_CANDIDATES_LIST:
            if (notifCode == LBN_SELCHANGE || notifCode == LBN_DBLCLK)
            {
                int sel = (int) SendMessageW (p->hCandList, LB_GETCURSEL, 0, 0);
                if (sel >= 0)
                    p->confirmCandidate (sel);
            }
            break;
```

- [ ] **Step 5: Auto-search VDJ on match selection**

Replace `IDC_RESULTS_LIST` handler to trigger smart search on selection:
```cpp
        case IDC_RESULTS_LIST:
            if (notifCode == LBN_SELCHANGE)
            {
                int sel = (int) SendMessageW (p->hResultsList, LB_GETCURSEL, 0, 0);
                p->selectedResultIdx = sel;
                InvalidateRect (hwnd, nullptr, FALSE);

                // Auto-search VDJ browser (replaces FIND TRACK button)
                if (sel >= 0 && sel < (int) p->results.size())
                {
                    const TgRecord& rec = p->results[sel];
                    std::wstring query = normalizeForSearch (rec.title);
                    if (!rec.bandleader.empty()) query += L" " + normalizeForSearch (rec.bandleader);
                    std::string cmd = "search \"" + toUtf8 (query) + "\"";

                    p->vdjSend ("browser_window 'songs'");
                    p->vdjSend (cmd);

                    p->searchTargetTitle  = rec.title;
                    p->searchTargetArtist = rec.bandleader;
                    p->searchTargetYear   = rec.year;
                    p->smartSearchPending = true;

                    SetTimer (hwnd, TIMER_SMART_SEARCH, 500, nullptr);
                }
            }
            break;
```

- [ ] **Step 6: Remove IDC_BTN_SEARCH_VDJ handler**

Delete the entire `case IDC_BTN_SEARCH_VDJ:` block.

- [ ] **Step 7: Commit**

```bash
git add Source/TigerTandaUI.cpp
git commit -m "feat: live search, auto-browse on match select, simplified tab toggle"
```

---

### Task 8: Update WM_DRAWITEM — new candidate layout, remove old tab drawing

**Files:**
- Modify: `Source/TigerTandaUI.cpp` (WM_DRAWITEM section, ~lines 1257–1530)

- [ ] **Step 1: Remove tab button draw handlers**

Remove the `IDC_BTN_TAB_TRACK`, `IDC_BTN_TAB_MATCHES`, `IDC_BTN_TAB_BROWSE` drawTextToggle calls. Keep `IDC_BTN_TAB_SETTINGS` but change it to draw a simple gear icon button instead of a text toggle:
```cpp
            if (di->CtlID == IDC_BTN_TAB_SETTINGS)
            {
                COLORREF bg = (p->activeTab == 1) ? TCol::buttonHover : TCol::panel;
                drawOwnerButton (di, L"\u26ED", bg, TCol::textNormal, p->fontNormal, p->hoveredBtn == di->hwndItem);
                return TRUE;
            }
```

- [ ] **Step 2: Remove search button draw handler**

Delete the `IDC_BTN_SEARCH` magnifying glass draw handler.

- [ ] **Step 3: Rewrite candidate list drawing — 3-column layout**

Replace the candidates list draw handler:
```cpp
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

            // Left accent border for confirmed candidate
            if (confirmed)
            {
                RECT accentR { r.left, r.top, r.left + 3, r.bottom };
                fillRect (hdc, accentR, TCol::accent);
            }

            // 3-column layout aligned with inputs: title | artist | year
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

            drawText (hdc, titleR,  rec.title,      titleCol, confirmed ? p->fontBold : p->fontNormal,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, artistR, rec.bandleader,  otherCol, p->fontSmall,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, yearR,   rec.year,        otherCol, p->fontSmall,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, pen);
            MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
            LineTo   (hdc, r.right, r.bottom - 1);
            SelectObject (hdc, old);
            DeleteObject (pen);
            return TRUE;
        }
```

- [ ] **Step 4: Update results list drawing — add artist column**

Replace the results list draw handler to show title | artist | year:
```cpp
        if (di->CtlID == IDC_RESULTS_LIST && di->itemID != (UINT) -1)
        {
            if ((int) di->itemID >= (int) p->results.size()) break;
            const TgRecord& rec = p->results[di->itemID];

            HDC  hdc  = di->hDC;
            RECT r    = di->rcItem;
            bool sel  = (di->itemState & ODS_SELECTED) != 0;
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

            drawText (hdc, titleR,  rec.title,      TCol::textBright, p->fontBold,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            drawText (hdc, artistR, rec.bandleader,  TCol::textDim,    p->fontSmall,
                      DT_LEFT  | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            std::wstring yearStr = rec.year.empty()
                ? (rec.date.size() >= 4 ? rec.date.substr (0, 4) : rec.date)
                : rec.year;
            drawText (hdc, yearR,   yearStr,         TCol::textDim,    p->fontSmall,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            HPEN pen = CreatePen (PS_SOLID, 1, TCol::cardBorder);
            HPEN old = (HPEN) SelectObject (hdc, pen);
            MoveToEx (hdc, r.left, r.bottom - 1, nullptr);
            LineTo   (hdc, r.right, r.bottom - 1);
            SelectObject (hdc, old);
            DeleteObject (pen);
            return TRUE;
        }
```

- [ ] **Step 5: Remove IDC_BTN_SEARCH_VDJ from generic button draw**

Remove the `IDC_BTN_SEARCH_VDJ` special-case in the generic button drawing at the bottom of WM_DRAWITEM.

- [ ] **Step 6: Commit**

```bash
git add Source/TigerTandaUI.cpp
git commit -m "refactor: WM_DRAWITEM — 3-column candidates/results, remove old tab/search drawing"
```

---

### Task 9: Update WM_TIMER polling — add year, update edit sync

**Files:**
- Modify: `Source/TigerTandaUI.cpp` (WM_TIMER section, ~lines 888–1037)

- [ ] **Step 1: Poll year from VDJ and populate year edit**

In the song polling section, add year:
```cpp
        std::wstring newTitle  = p->vdjGetString ("get_browsed_song 'title'");
        std::wstring newArtist = p->vdjGetString ("get_browsed_song 'artist'");
        std::wstring newYear   = p->vdjGetString ("get_browsed_song 'year'");

        if (!newTitle.empty()
            && (newTitle != p->lastSeenTitle || newArtist != p->lastSeenArtist))
        {
            p->lastSeenTitle  = newTitle;
            p->lastSeenArtist = newArtist;
            if (p->hEditTitle)  SetWindowTextW (p->hEditTitle,  newTitle.c_str());
            if (p->hEditArtist) SetWindowTextW (p->hEditArtist, newArtist.c_str());
            if (p->hEditYear)   SetWindowTextW (p->hEditYear,   newYear.c_str());
            p->runIdentification (newTitle, newArtist);
        }
```

- [ ] **Step 2: Add TIMER_SEARCH_DEBOUNCE to WM_DESTROY cleanup**

```cpp
    case WM_DESTROY:
        KillTimer (hwnd, TIMER_BROWSE_POLL);
        KillTimer (hwnd, TIMER_SMART_SEARCH);
        KillTimer (hwnd, TIMER_WAVE_UPDATE);
        KillTimer (hwnd, TIMER_SEARCH_DEBOUNCE);
```

- [ ] **Step 3: Commit**

```bash
git add Source/TigerTandaUI.cpp
git commit -m "feat: poll year from VDJ, populate year edit, cleanup debounce timer"
```

---

### Task 10: Update WM_CTLCOLOREDIT for year edit + final cleanup

**Files:**
- Modify: `Source/TigerTandaUI.cpp`

- [ ] **Step 1: Add year edit to color handler**

Update the `WM_CTLCOLOREDIT` handler to include the year edit:
```cpp
        if (p && (hed == p->hEditTitle || hed == p->hEditArtist || hed == p->hEditYear))
```

- [ ] **Step 2: Remove drawScoreDot function**

Delete the `drawScoreDot` helper — no longer used (score dots removed from candidates).

- [ ] **Step 3: Build and verify**

```bash
cmake --build build --config Release
```

Expected: Clean build, output `build/Release/TigerTanda.dll`

- [ ] **Step 4: Commit**

```bash
git add Source/TigerTandaUI.cpp
git commit -m "feat: complete two-column layout redesign"
```
