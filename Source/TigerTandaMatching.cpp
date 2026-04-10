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
        candidates = matcher.findCandidatesForArtist (title, artist, 2, 40.0f);

    // Fallback: title-only search
    if (candidates.empty())
        candidates = matcher.findCandidates (title, 2, 40.0f);

    // Cap to 2 candidates (UI shows 2 rows)
    if ((int) candidates.size() > 2)
        candidates.resize (2);

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
        // No reference confirmed — nothing to do
        if (hResultsList)
            SendMessageW (hResultsList, LB_RESETCONTENT, 0, 0);
        if (hDlg)
            MessageBoxW (hDlg, L"Please confirm a reference song first by clicking a candidate.",
                         L"TigerTanda", MB_OK | MB_ICONINFORMATION);
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

    // Auto-select first match and trigger VDJ browse search
    if (!results.empty())
    {
        selectedResultIdx = 0;
        if (hResultsList)
            SendMessageW (hResultsList, LB_SETCURSEL, 0, 0);

        const TgRecord& rec = results[0];

        // Skip the search entirely if we already searched for this exact
        // target recently — this kills the re-entry cycle where polling
        // re-fires identification whose top match is the same song.
        bool sameTarget = (rec.title == lastSmartSearchTitle
                        && rec.bandleader == lastSmartSearchArtist);

        if (!sameTarget)
        {
            lastSmartSearchTitle  = rec.title;
            lastSmartSearchArtist = rec.bandleader;

            std::wstring query = normalizeForSearch (rec.title);
            if (!rec.bandleader.empty())
                query += L" " + normalizeForSearch (rec.bandleader);

            vdjSend ("browser_window 'songs'");
            vdjSend ("search \"" + toUtf8 (query) + "\"");

            searchTargetTitle  = rec.title;
            searchTargetArtist = rec.bandleader;
            searchTargetYear   = rec.year;
            smartSearchPending = true;

            if (hDlg)
                SetTimer (hDlg, TIMER_SMART_SEARCH, 500, nullptr);
        }
    }

    // Force redraw
    if (hDlg)
        InvalidateRect (hDlg, nullptr, FALSE);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Smart search: read VDJ browser results, score & rank, keep top 5
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::runSmartSearch()
{
    // Use file_count (the correct VDJ API — browser_count does not exist)
    int totalItems = (int) vdjGetValue ("file_count");
    if (totalItems <= 0)
    {
        // VDJ hasn't populated results yet — retry in 300ms (keep pending=true)
        if (hDlg)
            SetTimer (hDlg, TIMER_SMART_SEARCH, 300, nullptr);
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
        BrowseItem bi;
        bi.title        = vdjGetString ("get_browsed_song 'title'");
        bi.artist       = vdjGetString ("get_browsed_song 'artist'");
        bi.year         = vdjGetString ("get_browsed_song 'year'");
        bi.filePath     = vdjGetString ("get_browsed_filepath");
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

    // Keep top 5
    browseItems.clear();
    selectedBrowseIdx = -1;
    int keep = (int) scored.size() < 5 ? (int) scored.size() : 5;
    for (int i = 0; i < keep; ++i)
        browseItems.push_back (scored[i].item);

    // Eagerly extract cover art so first paint is already cached
    for (const auto& bi : browseItems)
        if (!bi.filePath.empty())
            CoverArt::getForPath (bi.filePath);

    // Update cached count so normal polling doesn't immediately overwrite
    browseListCount = totalItems;

    // Populate browse listbox
    if (hBrowseList)
    {
        SendMessageW (hBrowseList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < (int) browseItems.size(); ++i)
            SendMessageW (hBrowseList, LB_ADDSTRING, 0, (LPARAM) L"");
    }

    // Return VDJ's browser to the user's previous folder. Our own browseList
    // control shows the ranked results, so there's no need to leave VDJ
    // parked on a search-results view.
    vdjSend ("goto_last_folder");
    Sleep (20);

    // After goto_last_folder, VDJ's browse cursor may have landed on a
    // different song than the one the user was on. Refresh lastSeen* to
    // whatever is under VDJ's cursor now, so the next polling tick doesn't
    // treat it as a "new song" and kick off another identify → search cycle.
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

    if (hCandList)    SendMessageW (hCandList,    LB_RESETCONTENT, 0, 0);
    if (hResultsList) SendMessageW (hResultsList, LB_RESETCONTENT, 0, 0);
    if (hEditTitle)   SetWindowTextW (hEditTitle,  L"");
    if (hEditArtist)  SetWindowTextW (hEditArtist, L"");
    if (hEditYear)    SetWindowTextW (hEditYear,   L"");

    if (hDlg) InvalidateRect (hDlg, nullptr, FALSE);
}
