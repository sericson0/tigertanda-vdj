//==============================================================================
// TigerTanda VDJ Plugin - Matching Logic
// Phase 1: Song identification | Phase 2: Tanda search
//==============================================================================

#include "TigerTanda.h"
#include "CoverArt.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Extract 4-digit year from a date string like "1/1/1935" or "1935"
static int parseYear (const std::wstring& dateStr)
{
    if (dateStr.empty()) return 0;

    // Scan for a 4-digit run that looks like a year (1900–2099)
    std::wstring part;
    for (size_t i = 0; i <= dateStr.size(); ++i)
    {
        wchar_t c = (i < dateStr.size()) ? dateStr[i] : L'\0';
        bool isSep = (c == L'/' || c == L'-' || c == L' ' || c == L'\0');

        if (!isSep)
        {
            part += c;
        }
        else
        {
            if (part.size() == 4)
            {
                bool allDigit = true;
                for (wchar_t d : part) if (!iswdigit (d)) { allDigit = false; break; }
                if (allDigit)
                {
                    int y = std::stoi (part);
                    if (y >= 1900 && y <= 2099)
                        return y;
                }
            }
            part.clear();
        }
    }
    return 0;
}

// Case-insensitive wide-string comparison
static bool wiequal (const std::wstring& a, const std::wstring& b)
{
    return _wcsicmp (a.c_str(), b.c_str()) == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Phase 1: Identification
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::runIdentification (const std::wstring& title, const std::wstring& artist)
{
    candidates.clear();
    confirmedIdx = -1;
    // NOTE: results and selectedResultIdx are intentionally NOT cleared here.
    // The results panel only updates when the user confirms a candidate.

    if (title.empty()) return;

    // Use artist-aware search when artist is available
    if (!artist.empty())
        candidates = matcher.findCandidatesForArtist (title, artist, 3, 40.0f);

    // Fallback: title-only search
    if (candidates.empty())
        candidates = matcher.findCandidates (title, 3, 40.0f);

    // Cap to 3 — UI shows exactly 3 rows
    if ((int) candidates.size() > 3)
        candidates.resize (3);

    // Refresh candidates listbox
    if (hCandList)
    {
        SendMessageW (hCandList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < (int) candidates.size(); ++i)
        {
            // LB_ADDSTRING with an empty string (owner-draw reads from candidates[])
            SendMessageW (hCandList, LB_ADDSTRING, 0, (LPARAM) L"");
        }
    }

    // Auto-confirm the first (best) candidate so matches populate immediately
    if (!candidates.empty())
        confirmCandidate (0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Confirm a candidate as the reference song
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::confirmCandidate (int idx)
{
    if (idx < 0 || idx >= (int) candidates.size()) return;
    confirmedIdx = idx;

    // Force repaint of candidates list so previous highlight clears
    if (hCandList)
        InvalidateRect (hCandList, nullptr, FALSE);

    // Auto-run tanda search
    runTandaSearch();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Phase 2: Tanda search
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::runTandaSearch()
{
    results.clear();
    selectedResultIdx = -1;
    selectedBrowseIdx = -1;

    if (confirmedIdx < 0 || confirmedIdx >= (int) candidates.size())
    {
        // No reference confirmed — clear results silently; the empty-state
        // paint code will show the appropriate placeholder.
        results.clear();
        if (hResultsList)
            SendMessageW (hResultsList, LB_RESETCONTENT, 0, 0);
        if (hDlg)
            InvalidateRect (hDlg, nullptr, FALSE);
        return;
    }

    const TgRecord& ref = candidates[confirmedIdx].record;
    int refYear = parseYear (ref.year);

    int total = matcher.getRecordCount();
    for (int i = 0; i < total; ++i)
    {
        const TgRecord& rec = matcher.getRecord (i);

        // Exclude the reference song itself
        if (wiequal (rec.title, ref.title)
            && wiequal (rec.bandleader, ref.bandleader)
            && wiequal (rec.year, ref.year))
            continue;

        // Optional filters
        if (filterSameArtist    && !wiequal (rec.bandleader, ref.bandleader)) continue;
        if (filterSameSinger    && !wiequal (rec.singer,     ref.singer))     continue;
        if (filterSameGrouping  && !wiequal (rec.grouping,   ref.grouping))   continue;
        if (filterSameGenre     && !wiequal (rec.genre,      ref.genre))      continue;
        if (filterSameOrchestra && !wiequal (rec.orchestra,  ref.orchestra))  continue;
        if (filterSameLabel     && !wiequal (rec.label,      ref.label))      continue;
        if (filterSameTrack     && !wiequal (rec.title,      ref.title))      continue;

        // Year range filter
        if (filterUseYearRange && yearRange > 0 && refYear > 0)
        {
            int recYear = parseYear (rec.year);
            if (recYear > 0 && std::abs (recYear - refYear) > yearRange)
                continue;
        }

        results.push_back (rec);
    }

    // Sort by year ascending (natural tanda order)
    std::sort (results.begin(), results.end(), [] (const TgRecord& a, const TgRecord& b) {
        return parseYear (a.year) < parseYear (b.year);
    });

    // Limit to 20
    if ((int) results.size() > 20)
        results.resize (20);

    // Populate results listbox
    if (hResultsList)
    {
        SendMessageW (hResultsList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < (int) results.size(); ++i)
            SendMessageW (hResultsList, LB_ADDSTRING, 0, (LPARAM) L"");
    }

    // Auto-select first match visually. Do NOT touch VDJ — the user must
    // explicitly click "Find in VDJ" to trigger a browser search. This is
    // the core of the Option A workflow: passive observer (polling) and
    // active controller (VDJ commands) never run in the same cycle.
    if (!results.empty())
    {
        selectedResultIdx = 0;
        if (hResultsList)
            SendMessageW (hResultsList, LB_SETCURSEL, 0, 0);
    }

    // Clear any stale browse results — they belong to a previous match.
    browseItems.clear();
    selectedBrowseIdx = -1;
    if (hBrowseList)
        SendMessageW (hBrowseList, LB_RESETCONTENT, 0, 0);
    syncBrowseListVisibility();

    // Force redraw
    if (hDlg)
        InvalidateRect (hDlg, nullptr, FALSE);
}

// ─────────────────────────────────────────────────────────────────────────────
//  triggerBrowserSearch — begin a search cycle for the given match record
//
//  Saves the current VDJ folder so it can be restored at the end of
//  runSmartSearch, then issues the VDJ search command and schedules the
//  timer that will read results 500ms later.
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::syncBrowseListVisibility()
{
    if (!hBrowseList) return;
    bool mainView = (activeTab == 0);
    bool shouldShow = mainView && !browseItems.empty();
    ShowWindow (hBrowseList, shouldShow ? SW_SHOW : SW_HIDE);
    if (hDlg && mainView)
        InvalidateRect (hDlg, nullptr, FALSE);
}

void TigerTandaPlugin::triggerBrowserSearch (const TgRecord& rec)
{
    // Skip if we already searched for this target — avoids redundant
    // browser disturbance when polling re-identifies the same song.
    bool sameTarget = (rec.title == lastSmartSearchTitle
                    && rec.bandleader == lastSmartSearchArtist);
    if (sameTarget)
        return;

    lastSmartSearchTitle  = rec.title;
    lastSmartSearchArtist = rec.bandleader;

    // Save the user's current folder so we can restore it afterwards.
    // Only save when we don't already have one stashed — if a previous
    // search cycle is still in flight (user clicked FIND IN VDJ again),
    // VDJ's current view is a search-results state and reading it would
    // clobber the real folder we need to restore to.
    if (savedBrowseFolder.empty())
    {
        std::wstring curFolder = vdjGetString ("get_browsed_folder_path");
        if (!curFolder.empty())
            savedBrowseFolder = curFolder;
    }

    std::wstring query = normalizeForSearch (rec.title);
    if (!rec.bandleader.empty())
        query += L" " + normalizeForSearch (rec.bandleader);

    lastBrowserSearchQuery = query;
    searchTargetTitle  = rec.title;
    searchTargetArtist = rec.bandleader;
    searchTargetYear   = rec.year;
    smartSearchPending = true;
    smartSearchActiveToken = ++smartSearchToken;
    smartSearchRetryCount = 0;
    smartSearchNoResults = false;

    vdjSend ("browser_window 'songs'");
    vdjSend ("search \"" + toUtf8 (query) + "\"");

    // Repaint so the placeholder in the empty browse area updates to
    // "Searching VDJ...". Also repaint the FIND IN VDJ button so its
    // pending state is visible.
    if (hDlg) InvalidateRect (hDlg, nullptr, FALSE);

    if (hDlg)
        SetTimer (hDlg, TIMER_SMART_SEARCH, 500, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
//  addSelectedBrowseToAutomix — ADD button handler
//
//  Re-issues the same search that produced the current browseItems, scrolls
//  to the saved browserIndex for the selected item, sends playlist_add, then
//  restores the saved folder. All synchronous (single-threaded UI) but
//  bracketed by smartSearchPending so polling stays silent.
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::addSelectedBrowseToAutomix()
{
    if (selectedBrowseIdx < 0 || selectedBrowseIdx >= (int) browseItems.size())
        return;
    if (lastBrowserSearchQuery.empty())
        return;

    const BrowseItem& bi = browseItems[selectedBrowseIdx];
    if (bi.browserIndex < 0)
        return;

    // Lock polling for the entire cycle.
    smartSearchPending = true;

    // Save current folder only if we don't already have one stashed from
    // a previous search cycle (which may still be mid-restore).
    if (savedBrowseFolder.empty())
    {
        std::wstring curFolder = vdjGetString ("get_browsed_folder_path");
        if (!curFolder.empty())
            savedBrowseFolder = curFolder;
    }

    // Re-issue the exact same query the smart search used. VDJ's result
    // ordering is stable for the same query, so browserIndex still lines up.
    vdjSend ("browser_window 'songs'");
    vdjSend ("search \"" + toUtf8 (lastBrowserSearchQuery) + "\"");

    // Wait up to 2 seconds for VDJ to populate search results — on a sluggish
    // host a fixed sleep can return before results are in, causing us to scroll
    // into an empty list and add the wrong file (or nothing).
    int waited = 0;
    int count = 0;
    while (waited < 2000)
    {
        Sleep (50);
        waited += 50;
        count = (int) vdjGetValue ("file_count");
        if (count > 0) break;
    }
    if (count == 0)
    {
        // Search produced nothing — abort rather than add the wrong file.
        // Still restore the saved folder so we don't strand the user on an
        // empty search-results view.
        if (!savedBrowseFolder.empty())
        {
            vdjSend ("browser_gotofolder \"" + toUtf8 (savedBrowseFolder) + "\"");
            Sleep (20);
            savedBrowseFolder.clear();
        }
        lastSeenTitle  = vdjGetString ("get_browsed_song 'title'");
        lastSeenArtist = vdjGetString ("get_browsed_song 'artist'");
        smartSearchPending = false;
        return;
    }

    // Scroll to the saved index within the search results.
    vdjSend ("browser_scroll 'top'");
    Sleep (20);
    if (bi.browserIndex > 0)
    {
        vdjSend ("browser_scroll +" + std::to_string (bi.browserIndex));
        Sleep (20);
    }

    // Append to automix bottom.
    vdjSend ("playlist_add");
    Sleep (20);

    // Restore the user's folder.
    if (!savedBrowseFolder.empty())
    {
        vdjSend ("browser_gotofolder \"" + toUtf8 (savedBrowseFolder) + "\"");
        Sleep (20);
        savedBrowseFolder.clear();
    }

    // Refresh polling baseline so the next tick sees "no change".
    lastSeenTitle  = vdjGetString ("get_browsed_song 'title'");
    lastSeenArtist = vdjGetString ("get_browsed_song 'artist'");

    smartSearchPending = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Smart search: read VDJ browser results, score & rank, keep top 4
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::runSmartSearch()
{
    // Snapshot the token so we can detect if the user canceled this search
    // (by clicking a different match) while our Sleep-driven browser
    // enumeration is running below.
    int myToken = smartSearchActiveToken;

    // Use file_count (the correct VDJ API — browser_count does not exist)
    int totalItems = (int) vdjGetValue ("file_count");
    if (totalItems <= 0)
    {
        // VDJ hasn't populated results yet — retry up to ~3 seconds total
        // (10 attempts * 300ms). If we hit the cap, treat it as "no results"
        // and unfreeze the UI.
        const int kMaxRetries = 10;
        if (smartSearchRetryCount < kMaxRetries)
        {
            ++smartSearchRetryCount;
            if (hDlg)
                SetTimer (hDlg, TIMER_SMART_SEARCH, 300, nullptr);
            return;
        }

        // Timed out — no matches in VDJ library. Clear state, restore
        // folder, and mark the browse list as "no results found" so the
        // placeholder shows a helpful message.
        browseItems.clear();
        selectedBrowseIdx = -1;
        smartSearchNoResults = true;
        if (hBrowseList)
            SendMessageW (hBrowseList, LB_RESETCONTENT, 0, 0);
        syncBrowseListVisibility();

        if (!savedBrowseFolder.empty())
        {
            vdjSend ("browser_gotofolder \"" + toUtf8 (savedBrowseFolder) + "\"");
            Sleep (20);
            savedBrowseFolder.clear();
        }
        lastSeenTitle  = vdjGetString ("get_browsed_song 'title'");
        lastSeenArtist = vdjGetString ("get_browsed_song 'artist'");
        smartSearchPending = false;
        if (hDlg) InvalidateRect (hDlg, nullptr, FALSE);
        return;
    }
    // NOTE: Keep smartSearchPending=true through the entire body — we clear
    // it at the very end after goto_last_folder + lastSeen* refresh. That
    // guarantees any WM_TIMER tick racing with our in-progress browser
    // manipulation will skip polling.

    const int limit = totalItems < 50 ? totalItems : 50;
    int targetYear = parseYear (searchTargetYear);

    struct ScoredItem
    {
        BrowseItem item;
        float      compositeScore;
    };
    std::vector<ScoredItem> scored;

    // Scroll to top then read each item one by one — the correct VDJ pattern
    // (get_browser_file does not exist; use browser_scroll + get_browsed_song)
    vdjSend ("browser_scroll 'top'");
    Sleep (20);

    for (int i = 0; i < limit; ++i)
    {
        // Early-bail token check: if the user confirmed a different candidate
        // (or reset) while we're mid-enumeration, abandon without mutating
        // browseItems or any other state. The newer search will manage
        // smartSearchPending on its own path.
        if ((i % 5) == 0 && myToken != smartSearchActiveToken)
            return;

        BrowseItem bi;
        bi.title        = vdjGetString ("get_browsed_song 'title'");
        bi.artist       = vdjGetString ("get_browsed_song 'artist'");
        bi.year         = vdjGetString ("get_browsed_song 'year'");
        bi.filePath     = vdjGetString ("get_browsed_filepath");
        bi.album        = vdjGetString ("get_browsed_song 'album'");
        bi.comment      = vdjGetString ("get_browsed_song 'comment'");
        bi.browserIndex = i;

        if (bi.title.empty() && bi.artist.empty())
            break;

        // Score components (each 0-100)
        // Artist matching: handle partial names and separators
        // e.g., "Troilo", "Troilo - Fiorentino", "Troilo / Fiorentino"
        // should all match "Anibal Troilo" equally
        float artistScore = 0.0f;
        if (!searchTargetArtist.empty() && !bi.artist.empty())
        {
            // Split VDJ artist on common separators (- / & feat. ft.)
            auto vdjParts = TangoMatcher::splitArtistParts (bi.artist);

            // Check each part of the VDJ artist against our bandleader
            float bestPartScore = 0.0f;
            for (auto& part : vdjParts)
            {
                float s = TangoMatcher::artistMatchScore (part, searchTargetArtist);
                if (s > bestPartScore) bestPartScore = s;
            }

            // Also try the full string in case it's just the full name
            float fullScore = TangoMatcher::artistMatchScore (bi.artist, searchTargetArtist);
            artistScore = (bestPartScore > fullScore) ? bestPartScore : fullScore;
        }

        float titleScore = 0.0f;
        if (!searchTargetTitle.empty() && !bi.title.empty())
            titleScore = TangoMatcher::tokenSortRatio (searchTargetTitle, bi.title);

        float yearScore = 0.0f;
        if (targetYear > 0)
        {
            int itemYear = parseYear (bi.year);
            if (itemYear > 0)
            {
                float diff = 100.0f - (float) std::abs (targetYear - itemYear) * 10.0f;
                yearScore = (diff > 0.0f) ? diff : 0.0f;
            }
        }

        double starsVal  = vdjGetValue ("get_browsed_song 'stars'");
        double playVal   = vdjGetValue ("get_browsed_song 'playcount'");
        int    stars     = (int) starsVal;
        int    playCount = (int) playVal;
        if (stars < 0) stars = 0;
        if (stars > 5) stars = 5;
        if (playCount < 0) playCount = 0;

        bi.stars     = stars;
        bi.playCount = playCount;

        float starsNorm    = (float) stars * 20.0f;
        float playRaw      = (float) playCount * 5.0f;
        float playNorm     = (playRaw < 100.0f) ? playRaw : 100.0f;
        float qualityScore = (starsNorm + playNorm) / 2.0f;

        // Composite: artist 40%, title 40%, year 10%, quality 10%
        float composite = artistScore * 0.4f + titleScore * 0.4f
                        + yearScore * 0.1f + qualityScore * 0.1f;

        bi.score = composite;
        scored.push_back ({ bi, composite });

        if (i + 1 < limit)
        {
            vdjSend ("browser_scroll +1");
            Sleep (10);
        }
    }

    // Sort by composite score descending
    std::sort (scored.begin(), scored.end(), [] (const ScoredItem& a, const ScoredItem& b) {
        return a.compositeScore > b.compositeScore;
    });

    // Token check: if the user clicked a different match or reset while we
    // were enumerating, smartSearchActiveToken will have changed. Discard
    // our scored results in that case — they belong to a stale target.
    bool stale = (myToken != smartSearchActiveToken);

    // Keep top 4 (unless stale)
    browseItems.clear();
    selectedBrowseIdx = -1;
    if (!stale)
    {
        int keep = (int) scored.size() < 4 ? (int) scored.size() : 4;
        for (int i = 0; i < keep; ++i)
            browseItems.push_back (scored[i].item);

        // Eagerly extract cover art so first paint is already cached
        for (const auto& bi : browseItems)
            if (!bi.filePath.empty())
                CoverArt::getForPath (bi.filePath);
    }

    // Update cached count so normal polling doesn't immediately overwrite
    browseListCount = totalItems;

    // Populate browse listbox (empty if stale)
    if (hBrowseList)
    {
        SendMessageW (hBrowseList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < (int) browseItems.size(); ++i)
            SendMessageW (hBrowseList, LB_ADDSTRING, 0, (LPARAM) L"");
    }
    syncBrowseListVisibility();

    // Return VDJ's browser to the user's saved folder. Prefer the explicit
    // savedBrowseFolder path (set by triggerBrowserSearch) over the less
    // precise goto_last_folder. Clear savedBrowseFolder after restoring so
    // the next search cycle captures a fresh folder.
    if (!savedBrowseFolder.empty())
    {
        vdjSend ("browser_gotofolder \"" + toUtf8 (savedBrowseFolder) + "\"");
        Sleep (20);
        savedBrowseFolder.clear();
    }
    else
    {
        vdjSend ("goto_last_folder");
        Sleep (20);
    }

    // Refresh lastSeen* so the next polling tick sees "no change" and
    // doesn't kick off another identify cycle for whatever song landed
    // under VDJ's cursor after the folder restore.
    lastSeenTitle  = vdjGetString ("get_browsed_song 'title'");
    lastSeenArtist = vdjGetString ("get_browsed_song 'artist'");

    smartSearchPending = false;

    // Force redraw
    if (hDlg)
        InvalidateRect (hDlg, nullptr, FALSE);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Reset everything
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::resetAll()
{
    candidates.clear();
    confirmedIdx = -1;
    results.clear();
    selectedResultIdx = -1;
    selectedBrowseIdx = -1;
    lastSeenTitle.clear();
    lastSeenArtist.clear();
    lastSmartSearchTitle.clear();
    lastSmartSearchArtist.clear();
    savedBrowseFolder.clear();
    lastBrowserSearchQuery.clear();

    if (hCandList)    SendMessageW (hCandList,    LB_RESETCONTENT, 0, 0);
    if (hResultsList) SendMessageW (hResultsList, LB_RESETCONTENT, 0, 0);
    if (hBrowseList)  SendMessageW (hBrowseList,  LB_RESETCONTENT, 0, 0);
    if (hEditTitle)   SetWindowTextW (hEditTitle,  L"");
    if (hEditArtist)  SetWindowTextW (hEditArtist, L"");
    if (hEditYear)    SetWindowTextW (hEditYear,   L"");

    browseItems.clear();
    syncBrowseListVisibility();

    if (hDlg) InvalidateRect (hDlg, nullptr, FALSE);
}
