#pragma once
#include <string>
#include <vector>

struct TgRecord
{
    std::wstring bandleader, orchestra, title, altTitle, genre, date, year;
    std::wstring singer, composer, author, label, master, grouping, arranger;
    std::wstring pianist, bassist, bandoneons, strings;
    std::wstring normTitle;
};

struct TgMatchResult
{
    float score = 0.0f;
    TgRecord record;
};

class TangoMatcher
{
public:
    void loadCsv (const std::wstring& path);
    void loadFolder (const std::wstring& folderPath);

    std::vector<TgMatchResult> findCandidates (const std::wstring& title,
                                               int limit = 10,
                                               float threshold = 60.0f,
                                               const std::wstring& year = {}) const;

    std::vector<TgMatchResult> findCandidatesForArtist (const std::wstring& title,
                                                        const std::wstring& artist,
                                                        int limit = 10,
                                                        float threshold = 60.0f,
                                                        const std::wstring& year = {}) const;

    int getRecordCount() const { return (int) records.size(); }
    const TgRecord& getRecord (int i) const { return records[i]; }
    const std::vector<std::wstring>& getAvailableArtists() const { return artistNames; }

    static std::wstring stripAccents (const std::wstring& text);
    static int  levenshteinDistance (const std::wstring& s1, const std::wstring& s2);
    static float tokenSortRatio (const std::wstring& s1, const std::wstring& s2);
    static std::wstring getLastName (const std::wstring& fullName);

    // Name normalization: "Di Sarli, Carlos" → "Carlos Di Sarli"
    static std::wstring normalizeArtistName (const std::wstring& name);

    // Split "Carlos Di Sarli - Roberto Rufino" → {"Carlos Di Sarli", "Roberto Rufino"}
    static std::vector<std::wstring> splitArtistParts (const std::wstring& artist);

    // Compute best artist match score considering full name, last name, and
    // "Last, First" variants
    static float artistMatchScore (const std::wstring& candidate,
                                   const std::wstring& bandleader);

    // Extract bandleader candidates from a file path by checking directory
    // names against known artist names
    std::wstring findArtistInPath (const std::wstring& filepath) const;

    static std::wstring toLower (const std::wstring& s);
    static std::wstring trim (const std::wstring& s);

private:
    std::vector<TgRecord>      records;
    std::vector<std::wstring>  artistNames;

    static std::vector<std::wstring> parseCsvLine (const std::string& line);
    static std::vector<std::wstring> tokenize (const std::wstring& text);
    static std::wstring utf8ToWide (const std::string& utf8);
};
