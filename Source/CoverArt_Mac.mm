//==============================================================================
// TigerTanda VDJ Plugin - Cover Art Extraction (macOS)
//
// Same binary parsers as Windows, but uses NSImage instead of GDI+.
// File I/O uses fopen (UTF-8 paths) instead of _wfopen (wide paths).
//==============================================================================

#include "CoverArt.h"

#import <Cocoa/Cocoa.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// Forward: convert wstring to UTF-8 std::string for fopen
static std::string wToUtf8 (const std::wstring& wide)
{
    std::string result;
    result.reserve (wide.size() * 2);
    for (wchar_t wc : wide)
    {
        uint32_t cp = (uint32_t) wc;
        if (cp < 0x80)
            result += (char) cp;
        else if (cp < 0x800)
        {
            result += (char) (0xC0 | (cp >> 6));
            result += (char) (0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            result += (char) (0xE0 | (cp >> 12));
            result += (char) (0x80 | ((cp >> 6) & 0x3F));
            result += (char) (0x80 | (cp & 0x3F));
        }
        else
        {
            result += (char) (0xF0 | (cp >> 18));
            result += (char) (0x80 | ((cp >> 12) & 0x3F));
            result += (char) (0x80 | ((cp >> 6) & 0x3F));
            result += (char) (0x80 | (cp & 0x3F));
        }
    }
    return result;
}

namespace CoverArt
{

// ─────────────────────────────────────────────────────────────────────────────
//  Cache (path → NSImage*, single-threaded UI access)
// ─────────────────────────────────────────────────────────────────────────────

static std::unordered_map<std::wstring, void*> g_cache;

// ─────────────────────────────────────────────────────────────────────────────
//  NSImage decode helper
// ─────────────────────────────────────────────────────────────────────────────

static void* decodeBytes (const uint8_t* data, size_t len)
{
    if (!data || len == 0) return nullptr;

    @autoreleasepool {
        NSData* nsData = [NSData dataWithBytes:data length:len];
        NSImage* img = [[NSImage alloc] initWithData:nsData];
        if (img && img.isValid)
            return (__bridge_retained void*) img;
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sidecar image
// ─────────────────────────────────────────────────────────────────────────────

static void* trySidecar (const std::wstring& path)
{
    fs::path p;
    try { p = fs::path (wToUtf8 (path)); } catch (...) { return nullptr; }
    fs::path dir = p.parent_path();
    if (dir.empty()) return nullptr;

    static const char* kNames[] = {
        "folder.jpg", "cover.jpg", "front.jpg",
        "Folder.jpg", "Cover.jpg", "Front.jpg",
        "folder.png", "cover.png", "front.png",
        "folder.jpeg", "cover.jpeg", "front.jpeg",
        nullptr
    };

    for (int i = 0; kNames[i]; ++i)
    {
        fs::path candidate = dir / kNames[i];
        std::error_code ec;
        if (!fs::exists (candidate, ec) || ec) continue;

        @autoreleasepool {
            NSString* nsPath = [NSString stringWithUTF8String:candidate.c_str()];
            NSImage* img = [[NSImage alloc] initWithContentsOfFile:nsPath];
            if (img && img.isValid)
                return (__bridge_retained void*) img;
        }
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ID3v2 parser (shared with Windows — pure C binary parsing)
// ─────────────────────────────────────────────────────────────────────────────

static uint32_t synchsafeToUint (const uint8_t* b)
{
    return ((uint32_t)(b[0] & 0x7f) << 21)
         | ((uint32_t)(b[1] & 0x7f) << 14)
         | ((uint32_t)(b[2] & 0x7f) << 7)
         |  (uint32_t)(b[3] & 0x7f);
}

static uint32_t plainBE32 (const uint8_t* b)
{
    return ((uint32_t) b[0] << 24)
         | ((uint32_t) b[1] << 16)
         | ((uint32_t) b[2] << 8)
         |  (uint32_t) b[3];
}

static void* parseID3v2Body (const uint8_t* body, size_t bodySize, int majorVer)
{
    size_t pos = 0;
    while (pos + 10 <= bodySize)
    {
        const uint8_t* fh = body + pos;
        if (fh[0] == 0 && fh[1] == 0 && fh[2] == 0 && fh[3] == 0)
            break;

        uint32_t frameSize = (majorVer == 4)
            ? synchsafeToUint (fh + 4)
            : plainBE32 (fh + 4);

        if (frameSize == 0 || pos + 10 + frameSize > bodySize)
            break;

        if (fh[0] == 'A' && fh[1] == 'P' && fh[2] == 'I' && fh[3] == 'C')
        {
            const uint8_t* f = fh + 10;
            size_t fsz = frameSize;
            size_t i = 0;

            if (i >= fsz) break;
            uint8_t encoding = f[i++];

            while (i < fsz && f[i] != 0) ++i;
            if (i >= fsz) break;
            ++i;

            if (i >= fsz) break;
            ++i;

            if (encoding == 0 || encoding == 3)
            {
                while (i < fsz && f[i] != 0) ++i;
                if (i >= fsz) break;
                ++i;
            }
            else
            {
                while (i + 1 < fsz && !(f[i] == 0 && f[i + 1] == 0)) i += 2;
                if (i + 1 >= fsz) break;
                i += 2;
            }

            if (i >= fsz) break;
            return decodeBytes (f + i, fsz - i);
        }

        pos += 10 + frameSize;
    }
    return nullptr;
}

static void* parseID3v2 (FILE* f, long fileOffset)
{
    if (fseek (f, fileOffset, SEEK_SET) != 0) return nullptr;
    uint8_t hdr[10];
    if (fread (hdr, 1, 10, f) != 10) return nullptr;
    if (hdr[0] != 'I' || hdr[1] != 'D' || hdr[2] != '3') return nullptr;

    int majorVer = hdr[3];
    if (majorVer < 2 || majorVer > 4) return nullptr;
    uint8_t flags = hdr[5];
    uint32_t tagSize = synchsafeToUint (hdr + 6);
    if (tagSize == 0 || tagSize > 64 * 1024 * 1024) return nullptr;

    long bodyOffset = 0;
    if (flags & 0x40)
    {
        uint8_t ext[4];
        if (fread (ext, 1, 4, f) != 4) return nullptr;
        uint32_t extSize = (majorVer == 4) ? synchsafeToUint (ext) : plainBE32 (ext);
        if (extSize > tagSize) return nullptr;
        if (extSize > 4 && fseek (f, (long)(extSize - 4), SEEK_CUR) != 0) return nullptr;
        bodyOffset = (long) extSize;
    }

    long bodySize = (long) tagSize - bodyOffset;
    if (bodySize <= 0) return nullptr;

    std::vector<uint8_t> body ((size_t) bodySize);
    if (fread (body.data(), 1, (size_t) bodySize, f) != (size_t) bodySize)
        return nullptr;

    return parseID3v2Body (body.data(), body.size(), majorVer);
}

// ─────────────────────────────────────────────────────────────────────────────
//  MP3 — ID3v2 tag at file offset 0
// ─────────────────────────────────────────────────────────────────────────────

static void* tryMp3 (const std::wstring& path)
{
    FILE* f = fopen (wToUtf8 (path).c_str(), "rb");
    if (!f) return nullptr;
    void* img = parseID3v2 (f, 0);
    fclose (f);
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AIFF
// ─────────────────────────────────────────────────────────────────────────────

static void* tryAiff (const std::wstring& path)
{
    FILE* f = fopen (wToUtf8 (path).c_str(), "rb");
    if (!f) return nullptr;

    uint8_t hdr[12];
    if (fread (hdr, 1, 12, f) != 12
        || memcmp (hdr, "FORM", 4) != 0
        || (memcmp (hdr + 8, "AIFF", 4) != 0 && memcmp (hdr + 8, "AIFC", 4) != 0))
    {
        fclose (f);
        return nullptr;
    }

    fseek (f, 0, SEEK_END);
    long fileEnd = ftell (f);

    long pos = 12;
    while (pos + 8 <= fileEnd)
    {
        if (fseek (f, pos, SEEK_SET) != 0) break;
        uint8_t chunk[8];
        if (fread (chunk, 1, 8, f) != 8) break;
        uint32_t chunkSize = plainBE32 (chunk + 4);

        if (memcmp (chunk, "ID3 ", 4) == 0)
        {
            void* img = parseID3v2 (f, pos + 8);
            fclose (f);
            return img;
        }

        long next = pos + 8 + (long) chunkSize;
        if (chunkSize & 1) next++;
        if (next <= pos) break;
        pos = next;
    }

    fclose (f);
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  FLAC
// ─────────────────────────────────────────────────────────────────────────────

static void* tryFlac (const std::wstring& path)
{
    FILE* f = fopen (wToUtf8 (path).c_str(), "rb");
    if (!f) return nullptr;

    uint8_t magic[4];
    if (fread (magic, 1, 4, f) != 4 || memcmp (magic, "fLaC", 4) != 0)
    {
        fclose (f);
        return nullptr;
    }

    for (int safety = 0; safety < 256; ++safety)
    {
        uint8_t bhdr[4];
        if (fread (bhdr, 1, 4, f) != 4) break;
        uint8_t blockType = bhdr[0] & 0x7f;
        bool last = (bhdr[0] & 0x80) != 0;
        uint32_t blockSize = ((uint32_t) bhdr[1] << 16)
                           | ((uint32_t) bhdr[2] << 8)
                           |  (uint32_t) bhdr[3];
        if (blockSize > 64 * 1024 * 1024) break;

        if (blockType == 6)
        {
            std::vector<uint8_t> data ((size_t) blockSize);
            if (fread (data.data(), 1, blockSize, f) != blockSize) break;

            auto read32 = [&] (size_t idx) -> uint32_t
            {
                return ((uint32_t) data[idx]     << 24)
                     | ((uint32_t) data[idx + 1] << 16)
                     | ((uint32_t) data[idx + 2] << 8)
                     |  (uint32_t) data[idx + 3];
            };

            size_t i = 0;
            if (i + 4 > blockSize) break; i += 4;
            if (i + 4 > blockSize) break;
            uint32_t mimeLen = read32 (i); i += 4;
            if (i + mimeLen > blockSize) break; i += mimeLen;
            if (i + 4 > blockSize) break;
            uint32_t descLen = read32 (i); i += 4;
            if (i + descLen > blockSize) break; i += descLen;
            if (i + 16 > blockSize) break; i += 16;
            if (i + 4 > blockSize) break;
            uint32_t imgLen = read32 (i); i += 4;
            if (i + imgLen > blockSize) break;

            void* img = decodeBytes (data.data() + i, imgLen);
            fclose (f);
            return img;
        }

        if (fseek (f, (long) blockSize, SEEK_CUR) != 0) break;
        if (last) break;
    }

    fclose (f);
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MP4 / M4A / ALAC
// ─────────────────────────────────────────────────────────────────────────────

static bool findChildAtom (FILE* f,
                           long containerPayloadStart,
                           long containerPayloadEnd,
                           const char* type,
                           long* outPayloadOffset,
                           long* outPayloadSize)
{
    long pos = containerPayloadStart;
    while (pos + 8 <= containerPayloadEnd)
    {
        if (fseek (f, pos, SEEK_SET) != 0) return false;
        uint8_t hdr[8];
        if (fread (hdr, 1, 8, f) != 8) return false;

        uint32_t atomSize = plainBE32 (hdr);
        long headerLen = 8;

        if (atomSize == 1)
        {
            uint8_t ext[8];
            if (fread (ext, 1, 8, f) != 8) return false;
            uint64_t big = 0;
            for (int k = 0; k < 8; ++k) big = (big << 8) | ext[k];
            if (big > 0x7fffffff) return false;
            atomSize = (uint32_t) big;
            headerLen = 16;
        }
        else if (atomSize == 0)
        {
            return false;
        }

        if (atomSize < (uint32_t) headerLen) return false;
        if ((long) atomSize > containerPayloadEnd - pos) return false;

        if (memcmp (hdr + 4, type, 4) == 0)
        {
            *outPayloadOffset = pos + headerLen;
            *outPayloadSize   = (long) atomSize - headerLen;
            return true;
        }

        pos += (long) atomSize;
    }
    return false;
}

static void* tryMp4 (const std::wstring& path)
{
    FILE* f = fopen (wToUtf8 (path).c_str(), "rb");
    if (!f) return nullptr;

    fseek (f, 0, SEEK_END);
    long fileSize = ftell (f);
    if (fileSize <= 0) { fclose (f); return nullptr; }

    long moovStart = 0, moovSize = 0;
    if (!findChildAtom (f, 0, fileSize, "moov", &moovStart, &moovSize))
    { fclose (f); return nullptr; }

    long udtaStart = 0, udtaSize = 0;
    if (!findChildAtom (f, moovStart, moovStart + moovSize, "udta", &udtaStart, &udtaSize))
    { fclose (f); return nullptr; }

    long metaStart = 0, metaSize = 0;
    if (!findChildAtom (f, udtaStart, udtaStart + udtaSize, "meta", &metaStart, &metaSize))
    { fclose (f); return nullptr; }

    long ilstStart = 0, ilstSize = 0;
    bool foundIlst = false;
    if (metaSize >= 4)
        foundIlst = findChildAtom (f, metaStart + 4, metaStart + metaSize,
                                   "ilst", &ilstStart, &ilstSize);
    if (!foundIlst)
        foundIlst = findChildAtom (f, metaStart, metaStart + metaSize,
                                   "ilst", &ilstStart, &ilstSize);
    if (!foundIlst) { fclose (f); return nullptr; }

    long covrStart = 0, covrSize = 0;
    if (!findChildAtom (f, ilstStart, ilstStart + ilstSize, "covr", &covrStart, &covrSize))
    { fclose (f); return nullptr; }

    long dataStart = 0, dataSize = 0;
    if (!findChildAtom (f, covrStart, covrStart + covrSize, "data", &dataStart, &dataSize))
    { fclose (f); return nullptr; }

    if (dataSize < 8) { fclose (f); return nullptr; }
    long imgOffset = dataStart + 8;
    long imgSize   = dataSize - 8;
    if (imgSize <= 0 || imgSize > 64 * 1024 * 1024) { fclose (f); return nullptr; }

    if (fseek (f, imgOffset, SEEK_SET) != 0) { fclose (f); return nullptr; }
    std::vector<uint8_t> buf ((size_t) imgSize);
    if (fread (buf.data(), 1, (size_t) imgSize, f) != (size_t) imgSize)
    { fclose (f); return nullptr; }
    fclose (f);

    return decodeBytes (buf.data(), (size_t) imgSize);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Dispatch by extension
// ─────────────────────────────────────────────────────────────────────────────

static std::wstring lowerExt (const std::wstring& path)
{
    size_t dot = path.find_last_of (L'.');
    if (dot == std::wstring::npos) return L"";
    std::wstring ext = path.substr (dot);
    for (wchar_t& c : ext) c = (wchar_t) towlower (c);
    return ext;
}

static void* tryEmbedded (const std::wstring& path)
{
    std::wstring ext = lowerExt (path);
    if (ext == L".mp3")                        return tryMp3  (path);
    if (ext == L".flac")                       return tryFlac (path);
    if (ext == L".aiff" || ext == L".aif")     return tryAiff (path);
    if (ext == L".m4a"  || ext == L".mp4"
        || ext == L".alac" || ext == L".m4b")  return tryMp4  (path);
    return nullptr;
}

static void* tryExtract (const std::wstring& path)
{
    if (void* img = tryEmbedded (path)) return img;
    if (void* img = trySidecar  (path)) return img;
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void* getForPath (const std::wstring& filePath)
{
    if (filePath.empty()) return nullptr;

    auto it = g_cache.find (filePath);
    if (it != g_cache.end())
        return it->second;

    void* img = tryExtract (filePath);
    g_cache[filePath] = img;
    return img;
}

void shutdown()
{
    for (auto& kv : g_cache)
    {
        if (kv.second)
        {
            NSImage* img = (__bridge_transfer NSImage*) kv.second;
            img = nil;  // ARC releases
        }
    }
    g_cache.clear();
}

} // namespace CoverArt
