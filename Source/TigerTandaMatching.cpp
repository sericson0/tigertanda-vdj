//==============================================================================
// TigerTanda VDJ Plugin - Matching Logic
// Phase 1: Song identification | Phase 2: Tanda search
//==============================================================================

#include "TigerTanda.h"

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
        candidates = matcher.findCandidatesForArtist (title, artist, 5, 40.0f);

    // Fallback: title-only search
    if (candidates.empty())
        candidates = matcher.findCandidates (title, 5, 40.0f);

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

        // Year range filter
        if (yearRange > 0 && refYear > 0)
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

    // Force redraw
    if (hDlg)
        InvalidateRect (hDlg, nullptr, FALSE);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Smart search: read VDJ browser results, score & rank, keep top 5
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::runSmartSearch()
{
    int totalItems = (int) vdjGetValue ("browser_count");
    if (totalItems <= 0)
    {
        // VDJ hasn't populated results yet — retry in 300ms (keep pending=true)
        if (hDlg)
            SetTimer (hDlg, TIMER_SMART_SEARCH, 300, nullptr);
        return;
    }

    smartSearchPending = false;

    const int limit = totalItems < 200 ? totalItems : 200;
    int targetYear = parseYear (searchTargetYear);

    // Read all VDJ browser items and score them
    struct ScoredItem
    {
        BrowseItem item;
        float      compositeScore;
    };
    std::vector<ScoredItem> scored;

    for (int i = 0; i < limit; ++i)
    {
        std::string sq1 = "get_browser_file " + std::to_string (i) + " 'title'";
        std::string sq2 = "get_browser_file " + std::to_string (i) + " 'artist'";
        std::string sq3 = "get_browser_file " + std::to_string (i) + " 'year'";
        std::string sq4 = "get_browser_file " + std::to_string (i) + " 'stars'";
        std::string sq5 = "get_browser_file " + std::to_string (i) + " 'play_count'";

        BrowseItem bi;
        bi.title        = vdjGetString (sq1.c_str());
        bi.artist       = vdjGetString (sq2.c_str());
        bi.year         = vdjGetString (sq3.c_str());
        bi.browserIndex = i;

        if (bi.title.empty() && bi.artist.empty()) break;

        // Read stars and play count
        double starsVal    = vdjGetValue (sq4.c_str());
        double playVal     = vdjGetValue (sq5.c_str());
        int    stars       = (int) starsVal;
        int    playCount   = (int) playVal;
        if (stars < 0) stars = 0;
        if (stars > 5) stars = 5;
        if (playCount < 0) playCount = 0;

        // Score components (each 0-100)
        float artistScore = 0.0f;
        if (!searchTargetArtist.empty() && !bi.artist.empty())
            artistScore = TangoMatcher::tokenSortRatio (searchTargetArtist, bi.artist);

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

        float starsNorm    = (float) stars * 20.0f;     // 0-100
        float playRaw      = (float) playCount * 5.0f;
        float playNorm     = (playRaw < 100.0f) ? playRaw : 100.0f;  // 20+ plays = max
        float qualityScore = (starsNorm + playNorm) / 2.0f;

        // Composite: artist 40%, title 40%, year 10%, quality 10%
        float composite = artistScore * 0.4f + titleScore * 0.4f
                        + yearScore * 0.1f + qualityScore * 0.1f;

        bi.score = composite;
        scored.push_back ({ bi, composite });
    }

    // Sort by composite score descending
    std::sort (scored.begin(), scored.end(), [] (const ScoredItem& a, const ScoredItem& b) {
        return a.compositeScore > b.compositeScore;
    });

    // Keep top 5
    browseItems.clear();
    int keep = (int) scored.size() < 5 ? (int) scored.size() : 5;
    for (int i = 0; i < keep; ++i)
        browseItems.push_back (scored[i].item);

    // Update cached count so normal polling doesn't immediately overwrite
    browseListCount = totalItems;

    // Populate browse listbox
    if (hBrowseList)
    {
        SendMessageW (hBrowseList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < (int) browseItems.size(); ++i)
            SendMessageW (hBrowseList, LB_ADDSTRING, 0, (LPARAM) L"");
    }

    // Auto-scroll VDJ browser to #1 ranked result
    if (!browseItems.empty())
    {
        vdjSend ("browser_scroll 'top'");
        Sleep (20);
        int topIdx = browseItems[0].browserIndex;
        if (topIdx > 0)
            vdjSend ("browser_scroll +" + std::to_string (topIdx));
    }

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
    lastSeenTitle.clear();
    lastSeenArtist.clear();

    if (hCandList)    SendMessageW (hCandList,    LB_RESETCONTENT, 0, 0);
    if (hResultsList) SendMessageW (hResultsList, LB_RESETCONTENT, 0, 0);
    if (hEditTitle)   SetWindowTextW (hEditTitle,  L"");
    if (hEditArtist)  SetWindowTextW (hEditArtist, L"");

    if (hDlg) InvalidateRect (hDlg, nullptr, FALSE);
}
