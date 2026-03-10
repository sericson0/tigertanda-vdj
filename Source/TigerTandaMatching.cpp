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
    results.clear();

    if (title.empty()) return;

    // Use artist-aware search when artist is available
    if (!artist.empty())
        candidates = matcher.findCandidatesForArtist (title, artist, 10, 40.0f);

    // Fallback: title-only search
    if (candidates.empty())
        candidates = matcher.findCandidates (title, 10, 40.0f);

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

    // Clear results list
    if (hResultsList)
        SendMessageW (hResultsList, LB_RESETCONTENT, 0, 0);
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
    {
        RECT r { RIGHT_X, 0, DLG_W, DLG_H };
        InvalidateRect (hDlg, &r, FALSE);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Reset everything
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::resetAll()
{
    candidates.clear();
    confirmedIdx = -1;
    results.clear();
    lastSeenTitle.clear();
    lastSeenArtist.clear();

    if (hCandList)    SendMessageW (hCandList,    LB_RESETCONTENT, 0, 0);
    if (hResultsList) SendMessageW (hResultsList, LB_RESETCONTENT, 0, 0);
    if (hEditTitle)   SetWindowTextW (hEditTitle,  L"");
    if (hEditArtist)  SetWindowTextW (hEditArtist, L"");

    if (hDlg) InvalidateRect (hDlg, nullptr, FALSE);
}
