//==============================================================================
// TigerTanda VDJ Plugin - Cover Art Extraction
//
// Supports:
//   - Sidecar images: folder.{jpg,png}, cover.{jpg,png}, front.{jpg,png}
//   - MP3:   ID3v2 APIC frame
//   - AIFF:  ID3 chunk containing ID3v2 APIC frame
//   - FLAC:  PICTURE metadata block (type 6)
//   - M4A / ALAC: moov.udta.meta.ilst.covr.data atom chain
//
// Image decoding is done by GDI+ via CreateStreamOnHGlobal + Gdiplus::Bitmap,
// which handles JPEG, PNG, BMP, GIF, and TIFF.
//==============================================================================

#include "CoverArt.h"

#include <windows.h>
#include <shlobj.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
using Gdiplus::Bitmap;

namespace CoverArt
{
// ─────────────────────────────────────────────────────────────────────────────
//  Cache (path → Bitmap*, single-threaded UI access)
// ─────────────────────────────────────────────────────────────────────────────

static std::unordered_map<std::wstring, Bitmap*> g_cache;

// ─────────────────────────────────────────────────────────────────────────────
//  GDI+ decode helpers
// ─────────────────────────────────────────────────────────────────────────────

static Bitmap* decodeBytes (const BYTE* data, size_t len)
{
    if (!data || len == 0) return nullptr;

    HGLOBAL hMem = GlobalAlloc (GMEM_MOVEABLE, len);
    if (!hMem) return nullptr;
    void* p = GlobalLock (hMem);
    if (!p) { GlobalFree (hMem); return nullptr; }
    memcpy (p, data, len);
    GlobalUnlock (hMem);

    IStream* stream = nullptr;
    if (FAILED (CreateStreamOnHGlobal (hMem, TRUE, &stream)))
    {
        GlobalFree (hMem);
        return nullptr;
    }

    Bitmap* bmp = new Bitmap (stream);
    stream->Release();

    if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok)
    {
        delete bmp;
        return nullptr;
    }
    return bmp;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sidecar image: folder/cover/front .jpg/.png next to the audio file
// ─────────────────────────────────────────────────────────────────────────────

static Bitmap* trySidecar (const std::wstring& path)
{
    fs::path p;
    try { p = fs::path (path); } catch (...) { return nullptr; }
    fs::path dir = p.parent_path();
    if (dir.empty()) return nullptr;

    static const wchar_t* kNames[] = {
        L"folder.jpg", L"cover.jpg", L"front.jpg",
        L"Folder.jpg", L"Cover.jpg", L"Front.jpg",
        L"folder.png", L"cover.png", L"front.png",
        L"folder.jpeg", L"cover.jpeg", L"front.jpeg",
        nullptr
    };

    for (int i = 0; kNames[i]; ++i)
    {
        fs::path candidate = dir / kNames[i];
        std::error_code ec;
        if (!fs::exists (candidate, ec) || ec) continue;

        Bitmap* bmp = new Bitmap (candidate.wstring().c_str());
        if (bmp && bmp->GetLastStatus() == Gdiplus::Ok)
            return bmp;
        delete bmp;
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ID3v2 parser (MP3 + AIFF-ID3-chunk share this)
//
//  Tag header (10 bytes):
//    "ID3" + major(1) + revision(1) + flags(1) + size(4, synchsafe)
//  Frame header (v2.3/v2.4, 10 bytes):
//    ID(4) + size(4, plain BE for v2.3 / synchsafe for v2.4) + flags(2)
//  APIC body:
//    encoding(1) + mime(null-term ASCII) + picture_type(1) +
//    description(null-term, encoding-dependent) + image_bytes
// ─────────────────────────────────────────────────────────────────────────────

static uint32_t synchsafeToUint (const BYTE* b)
{
    return ((uint32_t)(b[0] & 0x7f) << 21)
         | ((uint32_t)(b[1] & 0x7f) << 14)
         | ((uint32_t)(b[2] & 0x7f) << 7)
         |  (uint32_t)(b[3] & 0x7f);
}

static uint32_t plainBE32 (const BYTE* b)
{
    return ((uint32_t) b[0] << 24)
         | ((uint32_t) b[1] << 16)
         | ((uint32_t) b[2] << 8)
         |  (uint32_t) b[3];
}

static Bitmap* parseID3v2Body (const BYTE* body, size_t bodySize, int majorVer)
{
    size_t pos = 0;
    while (pos + 10 <= bodySize)
    {
        const BYTE* fh = body + pos;

        // Padding (all-zero frame ID) — stop scanning
        if (fh[0] == 0 && fh[1] == 0 && fh[2] == 0 && fh[3] == 0)
            break;

        uint32_t frameSize = (majorVer == 4)
            ? synchsafeToUint (fh + 4)
            : plainBE32 (fh + 4);

        if (frameSize == 0 || pos + 10 + frameSize > bodySize)
            break;

        if (fh[0] == 'A' && fh[1] == 'P' && fh[2] == 'I' && fh[3] == 'C')
        {
            const BYTE* f = fh + 10;
            size_t fsz = frameSize;
            size_t i = 0;

            if (i >= fsz) break;
            BYTE encoding = f[i++];

            // MIME type — always ISO-8859-1, null-terminated
            while (i < fsz && f[i] != 0) ++i;
            if (i >= fsz) break;
            ++i;

            // Picture type byte
            if (i >= fsz) break;
            ++i;

            // Description — null-terminated, encoding-dependent
            if (encoding == 0 || encoding == 3)  // ISO-8859-1 or UTF-8
            {
                while (i < fsz && f[i] != 0) ++i;
                if (i >= fsz) break;
                ++i;
            }
            else  // UTF-16 with BOM (1) or UTF-16BE (2) — terminator is 2 zero bytes on even boundary
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

// Parse an ID3v2 tag starting at fileOffset. Reads header, loads body into
// memory, and scans for APIC.
static Bitmap* parseID3v2 (FILE* f, long fileOffset)
{
    if (fseek (f, fileOffset, SEEK_SET) != 0) return nullptr;
    BYTE hdr[10];
    if (fread (hdr, 1, 10, f) != 10) return nullptr;
    if (hdr[0] != 'I' || hdr[1] != 'D' || hdr[2] != '3') return nullptr;

    int majorVer = hdr[3];
    if (majorVer < 2 || majorVer > 4) return nullptr;
    BYTE flags = hdr[5];
    uint32_t tagSize = synchsafeToUint (hdr + 6);
    if (tagSize == 0 || tagSize > 64 * 1024 * 1024) return nullptr;  // sanity cap 64 MB

    // Skip extended header if present
    long bodyOffset = 0;
    if (flags & 0x40)
    {
        BYTE ext[4];
        if (fread (ext, 1, 4, f) != 4) return nullptr;
        uint32_t extSize = (majorVer == 4) ? synchsafeToUint (ext) : plainBE32 (ext);
        if (extSize > tagSize) return nullptr;
        // Skip remainder of extended header
        if (extSize > 4 && fseek (f, (long)(extSize - 4), SEEK_CUR) != 0) return nullptr;
        bodyOffset = (long) extSize;
    }

    long bodySize = (long) tagSize - bodyOffset;
    if (bodySize <= 0) return nullptr;

    std::vector<BYTE> body ((size_t) bodySize);
    if (fread (body.data(), 1, (size_t) bodySize, f) != (size_t) bodySize)
        return nullptr;

    return parseID3v2Body (body.data(), body.size(), majorVer);
}

// ─────────────────────────────────────────────────────────────────────────────
//  MP3 — ID3v2 tag is at file offset 0
// ─────────────────────────────────────────────────────────────────────────────

static Bitmap* tryMp3 (const std::wstring& path)
{
    FILE* f = _wfopen (path.c_str(), L"rb");
    if (!f) return nullptr;
    Bitmap* bmp = parseID3v2 (f, 0);
    fclose (f);
    return bmp;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AIFF — "FORM" container with chunks; ID3 lives in "ID3 " chunk
// ─────────────────────────────────────────────────────────────────────────────

static Bitmap* tryAiff (const std::wstring& path)
{
    FILE* f = _wfopen (path.c_str(), L"rb");
    if (!f) return nullptr;

    BYTE hdr[12];
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
        BYTE chunk[8];
        if (fread (chunk, 1, 8, f) != 8) break;
        uint32_t chunkSize = plainBE32 (chunk + 4);

        if (memcmp (chunk, "ID3 ", 4) == 0)
        {
            Bitmap* bmp = parseID3v2 (f, pos + 8);
            fclose (f);
            return bmp;
        }

        long next = pos + 8 + (long) chunkSize;
        if (chunkSize & 1) next++;  // AIFF pads odd chunks to even boundary
        if (next <= pos) break;
        pos = next;
    }

    fclose (f);
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  FLAC — "fLaC" magic then metadata blocks; PICTURE is block type 6
//
//  Block header (4 bytes): [last_flag:1 | block_type:7][size:24 big-endian]
//  PICTURE body:
//    picType(4) + mimeLen(4) + mime + descLen(4) + desc +
//    width(4) + height(4) + depth(4) + colors(4) + imgLen(4) + img
// ─────────────────────────────────────────────────────────────────────────────

static Bitmap* tryFlac (const std::wstring& path)
{
    FILE* f = _wfopen (path.c_str(), L"rb");
    if (!f) return nullptr;

    BYTE magic[4];
    if (fread (magic, 1, 4, f) != 4 || memcmp (magic, "fLaC", 4) != 0)
    {
        fclose (f);
        return nullptr;
    }

    for (int safety = 0; safety < 256; ++safety)
    {
        BYTE bhdr[4];
        if (fread (bhdr, 1, 4, f) != 4) break;
        BYTE blockType = bhdr[0] & 0x7f;
        bool last = (bhdr[0] & 0x80) != 0;
        uint32_t blockSize = ((uint32_t) bhdr[1] << 16)
                           | ((uint32_t) bhdr[2] << 8)
                           |  (uint32_t) bhdr[3];
        if (blockSize > 64 * 1024 * 1024) break;  // 64 MB sanity

        if (blockType == 6)  // PICTURE
        {
            std::vector<BYTE> data ((size_t) blockSize);
            if (fread (data.data(), 1, blockSize, f) != blockSize) break;

            auto read32 = [&] (size_t idx) -> uint32_t
            {
                return ((uint32_t) data[idx]     << 24)
                     | ((uint32_t) data[idx + 1] << 16)
                     | ((uint32_t) data[idx + 2] << 8)
                     |  (uint32_t) data[idx + 3];
            };

            size_t i = 0;
            if (i + 4 > blockSize) break; i += 4;                      // picType
            if (i + 4 > blockSize) break;
            uint32_t mimeLen = read32 (i); i += 4;
            if (i + mimeLen > blockSize) break; i += mimeLen;
            if (i + 4 > blockSize) break;
            uint32_t descLen = read32 (i); i += 4;
            if (i + descLen > blockSize) break; i += descLen;
            if (i + 16 > blockSize) break; i += 16;                    // w, h, depth, colors
            if (i + 4 > blockSize) break;
            uint32_t imgLen = read32 (i); i += 4;
            if (i + imgLen > blockSize) break;

            Bitmap* bmp = decodeBytes (data.data() + i, imgLen);
            fclose (f);
            return bmp;
        }

        if (fseek (f, (long) blockSize, SEEK_CUR) != 0) break;
        if (last) break;
    }

    fclose (f);
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MP4 / M4A / ALAC — atom tree walker
//
//  Cover art location: moov → udta → meta → ilst → covr → data → (8 byte
//  type/locale header) + raw JPEG/PNG bytes.
//
//  'meta' is a FullBox: 4-byte version/flags field precedes its children.
//  We also auto-detect files that incorrectly omit this field.
// ─────────────────────────────────────────────────────────────────────────────

// Find a named child atom within a container. Returns true on success and
// writes the *payload* offset + size (excluding the 8-byte atom header).
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
        BYTE hdr[8];
        if (fread (hdr, 1, 8, f) != 8) return false;

        uint32_t atomSize = plainBE32 (hdr);
        long headerLen = 8;

        if (atomSize == 1)
        {
            // 64-bit extended size
            BYTE ext[8];
            if (fread (ext, 1, 8, f) != 8) return false;
            uint64_t big = 0;
            for (int k = 0; k < 8; ++k) big = (big << 8) | ext[k];
            if (big > 0x7fffffff) return false;
            atomSize = (uint32_t) big;
            headerLen = 16;
        }
        else if (atomSize == 0)
        {
            // Atom extends to EOF — unusual, treat as end
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

static Bitmap* tryMp4 (const std::wstring& path)
{
    FILE* f = _wfopen (path.c_str(), L"rb");
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

    // 'meta' is a FullBox — skip 4-byte version/flags. Auto-fallback for files
    // that omit it: try ilst at meta+4 first, then at meta+0.
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

    // Inside the 'data' atom: 4 bytes type-code + 4 bytes locale + raw bytes
    if (dataSize < 8) { fclose (f); return nullptr; }
    long imgOffset = dataStart + 8;
    long imgSize   = dataSize - 8;
    if (imgSize <= 0 || imgSize > 64 * 1024 * 1024) { fclose (f); return nullptr; }

    if (fseek (f, imgOffset, SEEK_SET) != 0) { fclose (f); return nullptr; }
    std::vector<BYTE> buf ((size_t) imgSize);
    if (fread (buf.data(), 1, (size_t) imgSize, f) != (size_t) imgSize)
    { fclose (f); return nullptr; }
    fclose (f);

    return decodeBytes (buf.data(), (size_t) imgSize);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Dispatch by file extension
// ─────────────────────────────────────────────────────────────────────────────

static std::wstring lowerExt (const std::wstring& path)
{
    size_t dot = path.find_last_of (L'.');
    if (dot == std::wstring::npos) return L"";
    std::wstring ext = path.substr (dot);
    for (wchar_t& c : ext) c = (wchar_t) towlower (c);
    return ext;
}

static Bitmap* tryExtract (const std::wstring& path)
{
    // Sidecar first — cheapest and highest visual quality when present
    if (Bitmap* bmp = trySidecar (path)) return bmp;

    std::wstring ext = lowerExt (path);
    if (ext == L".mp3")                        return tryMp3  (path);
    if (ext == L".flac")                       return tryFlac (path);
    if (ext == L".aiff" || ext == L".aif")     return tryAiff (path);
    if (ext == L".m4a"  || ext == L".mp4"
        || ext == L".alac" || ext == L".m4b")  return tryMp4  (path);
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
        return it->second;  // may be nullptr (negative result cached)

    Bitmap* bmp = tryExtract (filePath);
    g_cache[filePath] = bmp;
    return bmp;
}

void shutdown()
{
    for (auto& kv : g_cache)
        delete kv.second;
    g_cache.clear();
}

} // namespace CoverArt
