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

    // No auto-selection — user must explicitly click or arrow-navigate to a match to trigger a library search.

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

    // Save the user's current folder so runSmartSearch can restore it
    // after enumeration completes. If a previous cycle is still in flight
    // (savedBrowseFolder already populated), leave it alone.
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

// Windows-aware filepath equality: case-insensitive and slash-agnostic.
// Needed because VDJ can return the same file with mixed case / mixed slash
// directions depending on the query pathway, which caused the ADD verifier
// to reject correctly-positioned rows and then re-seek to a wrong one.
static bool pathEq (const std::wstring& a, const std::wstring& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        wchar_t ca = a[i], cb = b[i];
        if (ca == L'/') ca = L'\\';
        if (cb == L'/') cb = L'\\';
        if (towlower (ca) != towlower (cb)) return false;
    }
    return true;
}

void TigerTandaPlugin::addSelectedBrowseToAutomix (const char* vdjCommand)
{
    if (!vdjCommand || !*vdjCommand)
        vdjCommand = "playlist_add";
    if (selectedBrowseIdx < 0 || selectedBrowseIdx >= (int) browseItems.size())
        return;
    if (lastBrowserSearchQuery.empty())
        return;

    const BrowseItem& bi = browseItems[selectedBrowseIdx];
    if (bi.filePath.empty())
        return;

    smartSearchPending = true;

    // Capture the user's current folder so we can restore it on the
    // fallback path. The fast path doesn't touch the browser, so this
    // is only used if attempt 0b runs.
    std::wstring userFolder;
    {
        std::wstring curFolder = vdjGetString ("get_browsed_folder_path");
        if (!curFolder.empty())
            userFolder = curFolder;
        else if (!savedBrowseFolder.empty())
            userFolder = savedBrowseFolder;
    }

    auto currentPath = [this] () {
        return vdjGetString ("get_browsed_filepath");
    };

    auto restoreAndUnlock = [&] () {
        if (!userFolder.empty())
        {
            vdjSend ("browser_gotofolder \"" + toUtf8 (userFolder) + "\"");
            Sleep (40);
        }
        lastSeenTitle  = vdjGetString ("get_browsed_song 'title'");
        lastSeenArtist = vdjGetString ("get_browsed_song 'artist'");
        smartSearchPending = false;
    };

    int automixBefore = (int) vdjGetValue ("file_count automix");

    // ── Attempt A (fast path): playlist_add "<full path>" ──
    //
    // VDJ's playlist_add (and sidelist_add) accept a filepath argument,
    // so the fast path is a single command + count check. No browser
    // navigation, so ADD is ~instant regardless of folder size. We infer
    // success from `file_count automix` growth — get_automix_song
    // 'filepath' N is a no-op in plugin context on this VDJ version.
    {
        std::string pathCmd = std::string (vdjCommand)
                            + " \"" + toUtf8 (bi.filePath) + "\"";
        vdjSend (pathCmd);
        Sleep (200);

        int afterA = (int) vdjGetValue ("file_count automix");
        if (afterA > automixBefore)
        {
            restoreAndUnlock();
            return;
        }
    }

    // ── Attempt 0b (slow fallback): browser_gotofolder parent + scroll-scan ──
    //
    // If the fast path failed for any reason, fall back to the documented
    // approach: navigate to the file's parent folder, scroll-scan for the
    // exact filepath, then fire the no-arg command on the parked row.
    // Physical folders have stable ordering so this is reliable — just
    // slow (one browser_scroll +1 per row).
    size_t lastSlash = bi.filePath.find_last_of (L"\\/");
    if (lastSlash == std::wstring::npos)
    {
        restoreAndUnlock();
        return;
    }

    std::wstring parentFolder = bi.filePath.substr (0, lastSlash);
    vdjSend ("browser_gotofolder \"" + toUtf8 (parentFolder) + "\"");
    Sleep (400);
    vdjSend ("browser_window 'songs'");
    Sleep (60);

    int folderCount = (int) vdjGetValue ("file_count");
    if (folderCount <= 0)
    {
        restoreAndUnlock();
        return;
    }

    vdjSend ("browser_scroll 'top'");
    Sleep (80);

    int scanLimit = folderCount > 1000 ? 1000 : folderCount;
    bool found = false;
    for (int k = 0; k < scanLimit; ++k)
    {
        if (pathEq (currentPath(), bi.filePath))
        {
            found = true;
            break;
        }
        vdjSend ("browser_scroll +1");
        Sleep (20);
    }

    if (found)
    {
        // Final verify, then fire the plain command.
        Sleep (50);
        if (pathEq (currentPath(), bi.filePath))
        {
            vdjSend (vdjCommand);
            Sleep (300);
        }
    }

    restoreAndUnlock();
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

        // Restore the user's saved folder so they don't get stranded on
        // an empty search-results view.
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
    // (get_browser_file does not exist; use browser_scroll + get_browsed_song).
    // Sleeps are now much more generous (50ms after scroll) because the
    // previous 10ms was causing read/scroll races: different get_browsed_*
    // queries would return data from different rows in the same iteration,
    // so the stored bi.filePath didn't match the stored bi.title.
    vdjSend ("browser_scroll 'top'");
    Sleep (80);

    for (int i = 0; i < limit; ++i)
    {
        // Early-bail token check: if the user confirmed a different candidate
        // (or reset) while we're mid-enumeration, abandon without mutating
        // browseItems or any other state. The newer search will manage
        // smartSearchPending on its own path.
        if ((i % 5) == 0 && myToken != smartSearchActiveToken)
            return;

        BrowseItem bi;

        // Bracket the read with TWO filepath reads. If they agree, VDJ's
        // "currently browsed song" state was stable throughout the window
        // we read the other metadata in, so the fields all belong to the
        // same row. If they disagree, we retry with a longer wait.
        std::wstring fpBegin, fpEnd;
        int attempts = 0;
        const int maxAttempts = 3;
        for (;;)
        {
            fpBegin  = vdjGetString ("get_browsed_filepath");
            bi.title  = vdjGetString ("get_browsed_song 'title'");
            bi.artist = vdjGetString ("get_browsed_song 'artist'");
            bi.year   = vdjGetString ("get_browsed_song 'year'");
            bi.album  = vdjGetString ("get_browsed_song 'album'");
            bi.comment = vdjGetString ("get_browsed_song 'comment'");
            fpEnd    = vdjGetString ("get_browsed_filepath");

            if (fpBegin == fpEnd)
                break;

            // Mid-read drift — wait longer, retry.
            if (++attempts >= maxAttempts) break;
            Sleep (80);
        }

        bi.filePath     = fpEnd;
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

        // Rating & playcount come back as STRING properties on this VDJ
        // version — vdjGetValue (numeric GetInfo) silently returns 0 for
        // both, so the old code showed every row as "no rating / 0 plays".
        // Read as strings and parse. VDJ stores rating internally on a
        // 0-100 scale (0, 20, 40, 60, 80, 100 = 0-5 stars) but some
        // property getters return the 0-5 form directly, so handle both.
        // The documented property name is 'rating'; 'stars' used to be
        // queried here but returns empty on current VDJ builds.
        auto parseIntLoose = [] (const std::wstring& s) -> int {
            int n = 0; bool any = false;
            for (wchar_t c : s)
            {
                if (c >= L'0' && c <= L'9') { n = n * 10 + (c - L'0'); any = true; }
                else if (any) break;
            }
            return any ? n : 0;
        };

        int rating   = parseIntLoose (vdjGetString ("get_browsed_song 'rating'"));
        int playCount = parseIntLoose (vdjGetString ("get_browsed_song 'playcount'"));

        // Normalize rating: VDJ's 0-100 scale → 0-5 stars. Anything already
        // in 0-5 range passes through; >5 is treated as the 0-100 form.
        int stars = (rating > 5) ? (rating + 10) / 20 : rating;
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
            // 50 ms is enough for VDJ to update its internal "browsed song"
            // state on the hosts we've tested. 10 ms was causing the reads
            // at the start of the next iteration to pick up data from the
            // previous row, producing rows where title/artist matched row N
            // but filepath pointed to row N-1.
            Sleep (50);
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

    // Restore the user's saved folder so their browser view goes back
    // to where they were. We've already captured bi.filePath for every
    // browseItem during enumeration, so the ADD button can navigate
    // directly to each target's parent folder on demand — no need to
    // keep VDJ parked on the search results.
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
