#pragma once
//==============================================================================
// TigerTanda VDJ Plugin - Cover Art Extraction
//
// Extracts embedded cover art from audio files (MP3, FLAC, AIFF, M4A/ALAC)
// and sidecar images (folder.jpg / cover.jpg / front.jpg next to the file).
// Decoded art is cached in memory keyed by file path; the cache owns the
// returned Gdiplus::Bitmap pointers.
//==============================================================================

#include <string>

namespace CoverArt
{
    // Returns a Gdiplus::Bitmap* (opaque to callers) for the given audio file,
    // or nullptr if no cover art could be extracted.
    //
    // The returned pointer is owned by the cache. Do NOT delete it.
    // The cache is single-threaded (UI thread only).
    //
    // First call for a path is slow (file I/O + image decode); subsequent calls
    // are O(1) hash lookups. Negative results (no art found) are also cached.
    void* getForPath (const std::wstring& filePath);

    // Release all cached bitmaps. Call once during plugin destruction.
    void shutdown();
}
