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

// ─────────────────────────────────────────────────────────────────────────────
//  Phase 1: Identification
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::runIdentification (const std::wstring& title, const std::wstring& artist)
{
    candidates.clear();
    confirmedIdx = -1;

    if (title.empty()) return;

    if (!artist.empty())
        candidates = matcher.findCandidatesForArtist (title, artist, 3, 40.0f);

    if (candidates.empty())
        candidates = matcher.findCandidates (title, 3, 40.0f);

    if ((int) candidates.size() > 3)
        candidates.resize (3);

    uiResetCandidatesList();
    for (int i = 0; i < (int) candidates.size(); ++i)
        uiAddCandidateRow();

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

    uiInvalidateCandidates();
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
        results.clear();
        uiResetResultsList();
        uiInvalidateDialog();
        return;
    }

    const TgRecord& ref = candidates[confirmedIdx].record;
    int refYear = parseYear (ref.year);

    int total = matcher.getRecordCount();
    for (int i = 0; i < total; ++i)
    {
        const TgRecord& rec = matcher.getRecord (i);

        if (wiEqual (rec.title, ref.title)
            && wiEqual (rec.bandleader, ref.bandleader)
            && wiEqual (rec.year, ref.year))
            continue;

        if (filterSameArtist    && !wiEqual (rec.bandleader, ref.bandleader)) continue;
        if (filterSameSinger    && !wiEqual (rec.singer,     ref.singer))     continue;
        if (filterSameGrouping  && !wiEqual (rec.grouping,   ref.grouping))   continue;
        if (filterSameGenre     && !wiEqual (rec.genre,      ref.genre))      continue;
        if (filterSameOrchestra && !wiEqual (rec.orchestra,  ref.orchestra))  continue;
        if (filterSameLabel     && !wiEqual (rec.label,      ref.label))      continue;
        if (filterSameTrack     && !wiEqual (rec.title,      ref.title))      continue;

        if (filterUseYearRange && yearRange > 0 && refYear > 0)
        {
            int recYear = parseYear (rec.year);
            if (recYear > 0 && std::abs (recYear - refYear) > yearRange)
                continue;
        }

        results.push_back (rec);
    }

    std::sort (results.begin(), results.end(), [] (const TgRecord& a, const TgRecord& b) {
        return parseYear (a.year) < parseYear (b.year);
    });

    if ((int) results.size() > 20)
        results.resize (20);

    uiResetResultsList();
    for (int i = 0; i < (int) results.size(); ++i)
        uiAddResultRow();

    browseItems.clear();
    selectedBrowseIdx = -1;
    uiResetBrowseList();
    syncBrowseListVisibility();

    uiInvalidateDialog();
}

// ─────────────────────────────────────────────────────────────────────────────
//  syncBrowseListVisibility
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::syncBrowseListVisibility()
{
#if defined(_WIN32)
    if (!hBrowseList) return;
    bool mainView = (activeTab == 0);
    bool shouldShow = mainView && !browseItems.empty();
    ShowWindow (hBrowseList, shouldShow ? SW_SHOW : SW_HIDE);
    if (hDlg && mainView)
        InvalidateRect (hDlg, nullptr, FALSE);
#elif defined(__APPLE__)
    // Mac UI handles visibility in its own draw cycle
    uiInvalidateDialog();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  triggerBrowserSearch
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::triggerBrowserSearch (const TgRecord& rec)
{
    bool sameTarget = (rec.title == lastSmartSearchTitle
                    && rec.bandleader == lastSmartSearchArtist);
    if (sameTarget)
        return;

    lastSmartSearchTitle  = rec.title;
    lastSmartSearchArtist = rec.bandleader;

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

    uiInvalidateDialog();
    uiSetTimer (TT_TIMER_SMART_SEARCH, 500);
}

// ─────────────────────────────────────────────────────────────────────────────
//  addSelectedBrowseToAutomix
// ─────────────────────────────────────────────────────────────────────────────

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

    std::wstring userFolder;
    {
        std::wstring curFolder = vdjGetString ("get_browsed_folder_path");
        if (!curFolder.empty())
            userFolder = curFolder;
        else if (!savedBrowseFolder.empty())
            userFolder = savedBrowseFolder;
    }

    auto restoreAndUnlock = [&] () {
        if (!userFolder.empty())
        {
            vdjSend ("browser_gotofolder \"" + toUtf8 (userFolder) + "\"");
            ttSleep (40);
        }
        lastSeenTitle  = vdjGetString ("get_browsed_song 'title'");
        lastSeenArtist = vdjGetString ("get_browsed_song 'artist'");
        smartSearchPending = false;
    };

    std::string pathCmd = std::string (vdjCommand)
                        + " \"" + toUtf8 (bi.filePath) + "\"";
    vdjSend (pathCmd);
    ttSleep (200);

    restoreAndUnlock();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Smart search: read VDJ browser results, score & rank, keep top 4
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::runSmartSearch()
{
    int myToken = smartSearchActiveToken;

    int totalItems = (int) vdjGetValue ("file_count");
    if (totalItems <= 0)
    {
        const int kMaxRetries = 10;
        if (smartSearchRetryCount < kMaxRetries)
        {
            ++smartSearchRetryCount;
            uiSetTimer (TT_TIMER_SMART_SEARCH, 300);
            return;
        }

        browseItems.clear();
        selectedBrowseIdx = -1;
        smartSearchNoResults = true;
        uiResetBrowseList();
        syncBrowseListVisibility();

        if (!savedBrowseFolder.empty())
        {
            vdjSend ("browser_gotofolder \"" + toUtf8 (savedBrowseFolder) + "\"");
            ttSleep (20);
            savedBrowseFolder.clear();
        }
        lastSeenTitle  = vdjGetString ("get_browsed_song 'title'");
        lastSeenArtist = vdjGetString ("get_browsed_song 'artist'");
        smartSearchPending = false;
        uiInvalidateDialog();
        return;
    }

    const int limit = totalItems < 50 ? totalItems : 50;
    int targetYear = parseYear (searchTargetYear);

    struct ScoredItem
    {
        BrowseItem item;
        float      compositeScore;
    };
    std::vector<ScoredItem> scored;

    vdjSend ("browser_scroll 'top'");
    ttSleep (80);

    for (int i = 0; i < limit; ++i)
    {
        if ((i % 5) == 0 && myToken != smartSearchActiveToken)
            return;

        BrowseItem bi;

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

            if (++attempts >= maxAttempts) break;
            ttSleep (80);
        }

        bi.filePath     = fpEnd;
        bi.browserIndex = i;

        if (bi.title.empty() && bi.artist.empty())
            break;

        float artistScore = 0.0f;
        if (!searchTargetArtist.empty() && !bi.artist.empty())
        {
            auto vdjParts = TangoMatcher::splitArtistParts (bi.artist);

            float bestPartScore = 0.0f;
            for (auto& part : vdjParts)
            {
                float s = TangoMatcher::artistMatchScore (part, searchTargetArtist);
                if (s > bestPartScore) bestPartScore = s;
            }

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

        float composite = artistScore * 0.4f + titleScore * 0.4f
                        + yearScore * 0.1f + qualityScore * 0.1f;

        bi.score = composite;
        scored.push_back ({ bi, composite });

        if (i + 1 < limit)
        {
            vdjSend ("browser_scroll +1");
            ttSleep (50);
        }
    }

    std::sort (scored.begin(), scored.end(), [] (const ScoredItem& a, const ScoredItem& b) {
        return a.compositeScore > b.compositeScore;
    });

    bool stale = (myToken != smartSearchActiveToken);

    browseItems.clear();
    selectedBrowseIdx = -1;
    if (!stale)
    {
        int keep = (int) scored.size() < 4 ? (int) scored.size() : 4;
        for (int i = 0; i < keep; ++i)
            browseItems.push_back (scored[i].item);

        for (const auto& bi : browseItems)
            if (!bi.filePath.empty())
                CoverArt::getForPath (bi.filePath);
    }

    browseListCount = totalItems;

    uiResetBrowseList();
    for (int i = 0; i < (int) browseItems.size(); ++i)
        uiAddBrowseRow();
    syncBrowseListVisibility();

    if (!savedBrowseFolder.empty())
    {
        vdjSend ("browser_gotofolder \"" + toUtf8 (savedBrowseFolder) + "\"");
        ttSleep (20);
        savedBrowseFolder.clear();
    }
    else
    {
        vdjSend ("goto_last_folder");
        ttSleep (20);
    }

    lastSeenTitle  = vdjGetString ("get_browsed_song 'title'");
    lastSeenArtist = vdjGetString ("get_browsed_song 'artist'");

    smartSearchPending = false;
    uiInvalidateDialog();
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

    uiResetCandidatesList();
    uiResetResultsList();
    uiResetBrowseList();
    uiSetEditText (IDC_EDIT_TITLE,  L"");
    uiSetEditText (IDC_EDIT_ARTIST, L"");
    uiSetEditText (IDC_EDIT_YEAR,   L"");

    browseItems.clear();
    syncBrowseListVisibility();

    uiInvalidateDialog();
}
