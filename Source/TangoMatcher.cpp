#include "TangoMatcher.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  String Utilities
// ─────────────────────────────────────────────────────────────────────────────

std::wstring TangoMatcher::utf8ToWide (const std::string& utf8)
{
#ifdef _WIN32
    if (utf8.empty()) return {};
    int len = MultiByteToWideChar (CP_UTF8, 0, utf8.c_str(), (int) utf8.size(), nullptr, 0);
    if (len <= 0) return {};
    std::wstring wide (len, L'\0');
    MultiByteToWideChar (CP_UTF8, 0, utf8.c_str(), (int) utf8.size(), &wide[0], len);
    return wide;
#else
    // Proper UTF-8 decode for macOS (wchar_t is 4 bytes = UTF-32)
    if (utf8.empty()) return {};
    std::wstring result;
    result.reserve (utf8.size());
    size_t i = 0;
    while (i < utf8.size())
    {
        uint32_t cp = 0;
        unsigned char c = (unsigned char) utf8[i];
        if (c < 0x80)      { cp = c; i += 1; }
        else if (c < 0xC0) { cp = L'?'; i += 1; }
        else if (c < 0xE0) { cp = c & 0x1F; if (i+1 < utf8.size()) cp = (cp << 6) | (utf8[i+1] & 0x3F); i += 2; }
        else if (c < 0xF0) { cp = c & 0x0F; if (i+1 < utf8.size()) cp = (cp << 6) | (utf8[i+1] & 0x3F); if (i+2 < utf8.size()) cp = (cp << 6) | (utf8[i+2] & 0x3F); i += 3; }
        else               { cp = c & 0x07; if (i+1 < utf8.size()) cp = (cp << 6) | (utf8[i+1] & 0x3F); if (i+2 < utf8.size()) cp = (cp << 6) | (utf8[i+2] & 0x3F); if (i+3 < utf8.size()) cp = (cp << 6) | (utf8[i+3] & 0x3F); i += 4; }
        result += (wchar_t) cp;
    }
    return result;
#endif
}

std::wstring TangoMatcher::toLower (const std::wstring& s)
{
    std::wstring result = s;
    for (auto& ch : result) ch = towlower (ch);
    return result;
}

std::wstring TangoMatcher::trim (const std::wstring& s)
{
    auto start = s.find_first_not_of (L" \t\r\n");
    if (start == std::wstring::npos) return {};
    auto end = s.find_last_not_of (L" \t\r\n");
    return s.substr (start, end - start + 1);
}

std::wstring TangoMatcher::stripAccents (const std::wstring& text)
{
    std::wstring result;
    result.reserve (text.size());

    for (wchar_t ch : text)
    {
        switch (ch)
        {
            case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5:
                result += L'A'; break;
            case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5:
                result += L'a'; break;
            case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
                result += L'E'; break;
            case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
                result += L'e'; break;
            case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
                result += L'I'; break;
            case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
                result += L'i'; break;
            case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6:
                result += L'O'; break;
            case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6:
                result += L'o'; break;
            case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
                result += L'U'; break;
            case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
                result += L'u'; break;
            case 0x00D1: result += L'N'; break;
            case 0x00F1: result += L'n'; break;
            default:
                if (ch < 0x0300 || ch > 0x036F)
                    result += ch;
                break;
        }
    }

    return toLower (result);
}

std::vector<std::wstring> TangoMatcher::tokenize (const std::wstring& text)
{
    std::vector<std::wstring> tokens;
    std::wstring lower = toLower (trim (text));
    std::wistringstream ss (lower);
    std::wstring token;
    while (ss >> token)
        tokens.push_back (token);
    return tokens;
}

// ─────────────────────────────────────────────────────────────────────────────
//  CSV Parsing
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::wstring> TangoMatcher::parseCsvLine (const std::string& line)
{
    std::vector<std::wstring> fields;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i)
    {
        char ch = line[i];

        if (ch == '"')
        {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"')
            {
                current += '"';
                ++i;
            }
            else
            {
                inQuotes = !inQuotes;
            }
        }
        else if (ch == ',' && !inQuotes)
        {
            auto wide = utf8ToWide (current);
            fields.push_back (trim (wide));
            current.clear();
        }
        else
        {
            current += ch;
        }
    }

    auto wide = utf8ToWide (current);
    fields.push_back (trim (wide));
    return fields;
}

void TangoMatcher::loadCsv (const std::wstring& path)
{
    std::ifstream file { fs::path (path) };
    if (!file.is_open()) return;

    std::string headerLine;
    if (!std::getline (file, headerLine)) return;

    // Strip UTF-8 BOM
    if (headerLine.size() >= 3
        && (unsigned char) headerLine[0] == 0xEF
        && (unsigned char) headerLine[1] == 0xBB
        && (unsigned char) headerLine[2] == 0xBF)
    {
        headerLine = headerLine.substr (3);
    }
    if (!headerLine.empty() && headerLine.back() == '\r')
        headerLine.pop_back();

    auto headers = parseCsvLine (headerLine);

    auto col = [&] (const std::wstring& name) -> int
    {
        for (int i = 0; i < (int) headers.size(); ++i)
            if (toLower (headers[i]) == toLower (name))
                return i;
        return -1;
    };

    int cBandleader = col (L"Bandleader"),  cOrchestra = col (L"Orchestra");
    int cTitle      = col (L"Title"),       cAltTitle  = col (L"AltTitle");
    if (cAltTitle < 0) cAltTitle = col (L"Alternative_Title");
    int cGenre      = col (L"Genre"),       cDate      = col (L"Date");
    int cSinger     = col (L"Singer"),      cComposer  = col (L"Composer");
    int cAuthor     = col (L"Author"),      cLabel     = col (L"Label");
    int cMaster     = col (L"Master"),      cGrouping  = col (L"Grouping");
    int cArranger   = col (L"Arranger");
    int cPianist    = col (L"Pianist");     if (cPianist < 0) cPianist = col (L"Piano");
    int cBassist    = col (L"Bassist");
    int cBandoneons = col (L"Bandoneons");  if (cBandoneons < 0) cBandoneons = col (L"Bandoneon");
    int cStrings    = col (L"Strings");

    auto safe = [&] (const std::vector<std::wstring>& fields, int idx) -> std::wstring
    {
        return (idx >= 0 && idx < (int) fields.size()) ? fields[idx] : std::wstring {};
    };

    std::string line {};
    while (std::getline (file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty()) continue;

        auto fields = parseCsvLine (line);
        TgRecord rec;

        rec.bandleader = safe (fields, cBandleader);
        rec.orchestra  = safe (fields, cOrchestra);
        rec.title      = safe (fields, cTitle);
        rec.altTitle   = safe (fields, cAltTitle);
        rec.genre      = safe (fields, cGenre);
        rec.date       = safe (fields, cDate);
        rec.singer     = safe (fields, cSinger);
        rec.composer   = safe (fields, cComposer);
        rec.author     = safe (fields, cAuthor);
        rec.label      = safe (fields, cLabel);
        rec.master     = safe (fields, cMaster);
        rec.grouping   = safe (fields, cGrouping);
        rec.arranger   = safe (fields, cArranger);
        rec.pianist    = safe (fields, cPianist);
        rec.bassist    = safe (fields, cBassist);
        rec.bandoneons = safe (fields, cBandoneons);
        rec.strings    = safe (fields, cStrings);

        rec.normTitle = stripAccents (rec.title);

        if (!rec.date.empty())
        {
            std::wstring digits;
            std::wstring part;
            for (wchar_t c : rec.date)
            {
                if (c == L'/' || c == L'-' || c == L' ')
                {
                    if (part.size() == 4)
                    {
                        bool allDigit = true;
                        for (wchar_t d : part) if (!iswdigit (d)) allDigit = false;
                        if (allDigit) { rec.year = part; break; }
                    }
                    part.clear();
                }
                else
                {
                    part += c;
                }
            }
            if (rec.year.empty() && part.size() == 4)
            {
                bool allDigit = true;
                for (wchar_t d : part) if (!iswdigit (d)) allDigit = false;
                if (allDigit) rec.year = part;
            }
        }

        if (!rec.bandleader.empty())
        {
            bool found = false;
            for (auto& a : artistNames)
                if (a == rec.bandleader) { found = true; break; }
            if (!found)
                artistNames.push_back (rec.bandleader);
        }

        records.push_back (std::move (rec));
    }
}

void TangoMatcher::loadFolder (const std::wstring& folderPath)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator (folderPath, ec))
    {
        if (entry.path().extension() == L".csv")
            loadCsv (entry.path().wstring());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fuzzy Matching
// ─────────────────────────────────────────────────────────────────────────────

int TangoMatcher::levenshteinDistance (const std::wstring& s1, const std::wstring& s2)
{
    int m = (int) s1.size();
    int n = (int) s2.size();
    if (m == 0) return n;
    if (n == 0) return m;

    std::vector<int> prev (n + 1), curr (n + 1);
    for (int j = 0; j <= n; ++j) prev[j] = j;

    for (int i = 1; i <= m; ++i)
    {
        curr[0] = i;
        for (int j = 1; j <= n; ++j)
        {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            curr[j] = (std::min) ({ prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost });
        }
        std::swap (prev, curr);
    }

    return prev[n];
}

float TangoMatcher::tokenSortRatio (const std::wstring& s1, const std::wstring& s2)
{
    auto tokens1 = tokenize (stripAccents (s1));
    auto tokens2 = tokenize (stripAccents (s2));

    std::sort (tokens1.begin(), tokens1.end());
    std::sort (tokens2.begin(), tokens2.end());

    std::wstring sorted1, sorted2;
    for (size_t i = 0; i < tokens1.size(); ++i) { if (i) sorted1 += L' '; sorted1 += tokens1[i]; }
    for (size_t i = 0; i < tokens2.size(); ++i) { if (i) sorted2 += L' '; sorted2 += tokens2[i]; }

    if (sorted1.empty() && sorted2.empty()) return 100.0f;
    int maxLen = (int)(std::max) (sorted1.size(), sorted2.size());
    if (maxLen == 0) return 100.0f;

    int dist = levenshteinDistance (sorted1, sorted2);
    return (1.0f - (float) dist / (float) maxLen) * 100.0f;
}

static float yearBonus (const std::wstring& songYear, const std::wstring& recYear)
{
    if (songYear.empty() || recYear.empty()) return 0.0f;
    try
    {
        int sy = std::stoi (songYear);
        int ry = std::stoi (recYear);
        int diff = std::abs (sy - ry);
        if (diff == 0) return 8.0f;
        if (diff <= 2) return 4.0f;
        if (diff <= 5) return 2.0f;
    }
    catch (...) {}
    return 0.0f;
}

std::vector<TgMatchResult> TangoMatcher::findCandidates (const std::wstring& title,
                                                          int limit,
                                                          float threshold,
                                                          const std::wstring& year) const
{
    auto normQuery = stripAccents (title);

    struct Scored { int idx; float display; float sort; };
    std::vector<Scored> exact;
    for (int i = 0; i < (int) records.size(); ++i)
    {
        if (records[i].normTitle == normQuery)
            exact.push_back ({ i, 100.0f, 100.0f + yearBonus (year, records[i].year) });
    }
    if (!exact.empty())
    {
        std::stable_sort (exact.begin(), exact.end(),
                          [] (const Scored& a, const Scored& b) { return a.sort > b.sort; });
        std::vector<TgMatchResult> results;
        int count = (std::min) ((int) exact.size(), limit);
        for (int i = 0; i < count; ++i)
        {
            TgMatchResult mr;
            mr.score  = exact[i].display;
            mr.record = records[exact[i].idx];
            results.push_back (mr);
        }
        return results;
    }

    std::vector<Scored> scored;
    scored.reserve (records.size());

    for (int i = 0; i < (int) records.size(); ++i)
    {
        float s = tokenSortRatio (normQuery, records[i].normTitle);
        if (s >= threshold)
            scored.push_back ({ i, s, s + yearBonus (year, records[i].year) });
    }

    std::stable_sort (scored.begin(), scored.end(),
                      [] (const Scored& a, const Scored& b) { return a.sort > b.sort; });

    std::vector<TgMatchResult> results;
    int count = (std::min) ((int) scored.size(), limit);
    for (int i = 0; i < count; ++i)
    {
        TgMatchResult mr;
        mr.score  = scored[i].display;
        mr.record = records[scored[i].idx];
        results.push_back (mr);
    }
    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Name normalization and smart artist matching
// ─────────────────────────────────────────────────────────────────────────────

std::wstring TangoMatcher::normalizeArtistName (const std::wstring& name)
{
    auto trimmed = trim (name);
    if (trimmed.empty()) return {};

    // Handle "Last, First" → "First Last"  (e.g. "Di Sarli, Carlos" → "Carlos Di Sarli")
    auto comma = trimmed.find (L',');
    if (comma != std::wstring::npos)
    {
        auto last  = trim (trimmed.substr (0, comma));
        auto first = trim (trimmed.substr (comma + 1));
        if (!first.empty() && !last.empty())
            return first + L" " + last;
        return last;
    }
    return trimmed;
}

std::vector<std::wstring> TangoMatcher::splitArtistParts (const std::wstring& artist)
{
    std::vector<std::wstring> parts;
    std::wstring remaining = artist;

    // Split on common separators: " - ", " / ", " & ", " feat. ", " ft. "
    static const std::wstring seps[] = { L" - ", L" / ", L" & ", L" feat. ", L" ft. " };

    for (auto& sep : seps)
    {
        auto pos = remaining.find (sep);
        if (pos != std::wstring::npos)
        {
            auto left = trim (remaining.substr (0, pos));
            auto right = trim (remaining.substr (pos + sep.size()));
            if (!left.empty())  parts.push_back (normalizeArtistName (left));
            if (!right.empty()) parts.push_back (normalizeArtistName (right));
            return parts;
        }
    }

    // No separator found — single name
    auto norm = normalizeArtistName (remaining);
    if (!norm.empty()) parts.push_back (norm);
    return parts;
}

float TangoMatcher::artistMatchScore (const std::wstring& candidate,
                                       const std::wstring& bandleader)
{
    if (candidate.empty() || bandleader.empty()) return 0.0f;

    auto normCand   = stripAccents (candidate);
    auto normLeader = stripAccents (bandleader);

    // 1. Full name fuzzy match
    float fullScore = tokenSortRatio (normCand, normLeader);

    // 2. Last name match (e.g., "Di Sarli" vs full "Carlos Di Sarli")
    auto leaderLast = stripAccents (getLastName (bandleader));
    float lastScore = 0.0f;
    if (!leaderLast.empty())
    {
        // Direct comparison against last name
        lastScore = tokenSortRatio (normCand, leaderLast);
        // Boost if the candidate IS the last name (short input matching long name)
        if (lastScore >= 90.0f)
            lastScore = (std::min) (lastScore, 95.0f);
    }

    // 3. Check if candidate contains the bandleader's last name as a substring
    float containsScore = 0.0f;
    if (!leaderLast.empty() && normCand.find (leaderLast) != std::wstring::npos)
        containsScore = 80.0f;

    return (std::max) ({ fullScore, lastScore, containsScore });
}

std::wstring TangoMatcher::findArtistInPath (const std::wstring& filepath) const
{
    if (filepath.empty() || artistNames.empty()) return {};

    // Split path into directory components
    std::vector<std::wstring> components;
    std::wstring current;
    for (wchar_t c : filepath)
    {
        if (c == L'\\' || c == L'/')
        {
            auto t = trim (current);
            if (!t.empty()) components.push_back (t);
            current.clear();
        }
        else
        {
            current += c;
        }
    }

    // Check each path component against known artist names
    float bestScore = 0.0f;
    std::wstring bestArtist;

    for (auto& comp : components)
    {
        if (comp.size() < 3) continue;

        for (auto& artist : artistNames)
        {
            float score = artistMatchScore (comp, artist);
            if (score > bestScore && score >= 70.0f)
            {
                bestScore = score;
                bestArtist = artist;
            }
        }
    }

    return bestArtist;
}

// ─────────────────────────────────────────────────────────────────────────────

std::vector<TgMatchResult> TangoMatcher::findCandidatesForArtist (const std::wstring& title,
                                                                   const std::wstring& artist,
                                                                   int limit,
                                                                   float threshold,
                                                                   const std::wstring& year) const
{
    auto normQuery = stripAccents (title);

    auto parts = splitArtistParts (artist);

    struct Scored { int idx; float display; float sort; };
    std::vector<Scored> scored;

    for (int i = 0; i < (int) records.size(); ++i)
    {
        float bestArtist = 0.0f;
        for (auto& cand : parts)
        {
            float s = artistMatchScore (cand, records[i].bandleader);
            if (s > bestArtist) bestArtist = s;
        }

        if (bestArtist < 40.0f) continue;

        float titleScore = tokenSortRatio (normQuery, records[i].normTitle);
        float combined   = titleScore * 0.7f + bestArtist * 0.3f;
        if (combined >= threshold)
            scored.push_back ({ i, combined, combined + yearBonus (year, records[i].year) });
    }

    std::stable_sort (scored.begin(), scored.end(),
                      [] (const Scored& a, const Scored& b) { return a.sort > b.sort; });

    std::vector<TgMatchResult> results;
    int count = (std::min) ((int) scored.size(), limit);
    for (int i = 0; i < count; ++i)
    {
        TgMatchResult mr;
        mr.score  = scored[i].display;
        mr.record = records[scored[i].idx];
        results.push_back (mr);
    }
    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
//  TgRecord helpers
// ─────────────────────────────────────────────────────────────────────────────

std::wstring TangoMatcher::getLastName (const std::wstring& fullName)
{
    static const std::vector<std::wstring> prefixes = { L"De", L"Di", L"Del", L"Della", L"Dell", L"Da", L"Dos" };

    auto trimmed = trim (fullName);
    if (trimmed.empty()) return {};

    std::vector<std::wstring> parts;
    std::wistringstream ss (trimmed);
    std::wstring p;
    while (ss >> p) parts.push_back (p);

    if (parts.empty()) return {};
    if (parts.size() == 1) return parts[0];

    auto last = parts.back();
    if (parts.size() >= 2)
    {
        auto& secondToLast = parts[parts.size() - 2];
        for (auto& prefix : prefixes)
            if (toLower (secondToLast) == toLower (prefix))
                return secondToLast + L" " + last;
    }
    return last;
}

