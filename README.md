# Tiger Tanda

A VirtualDJ plugin for tango DJs to build tandas by searching a curated metadata database.

Tiger Tanda identifies the song you're browsing in VirtualDJ, matches it against a tango metadata database, and helps you find complementary songs to build a complete tanda (a set of 3-4 songs by the same artist/orchestra).

## Features

- **Fuzzy song identification** — automatically detects the song selected in VirtualDJ's browser and matches it against the metadata database
- **Filtered tanda search** — find matching songs filtered by artist, singer, orchestra, genre, grouping, label, and year range
- **Smart VDJ browser search** — searches VirtualDJ's browser and scores results by artist, title, year closeness, stars, and play count
- **Prelisten waveform** — preview songs directly from the plugin with a visual waveform
- **Cross-platform** — Windows (DLL) and macOS (bundle)

## Installation

### Windows

1. Download `TigerTanda-Windows.zip` from [Releases](https://github.com/sericson0/tigertanda-vdj/releases)
2. Extract the zip — you'll get `TigerTanda.dll` and `metadata.csv`
3. Copy both files to your VirtualDJ plugins folder:
   ```
   Documents\VirtualDJ\Plugins64\SoundEffect\
   ```
4. Restart VirtualDJ
5. The Tiger Tanda window appears automatically when VirtualDJ is in the foreground

### macOS

1. Download `TigerTanda-macOS.zip` from [Releases](https://github.com/sericson0/tigertanda-vdj/releases)
2. Extract the zip — you'll get `TigerTanda.bundle` and `metadata.csv`
3. Copy both to your VirtualDJ plugins folder:
   ```
   ~/Library/Application Support/VirtualDJ/Plugins64/SoundEffect/
   ```
4. Restart VirtualDJ
5. The Tiger Tanda window appears automatically when VirtualDJ is in the foreground

## Usage

Tiger Tanda follows a three-step workflow:

### 1. Identify (Track tab)

Browse songs in VirtualDJ as you normally would. Tiger Tanda automatically detects the selected song and fuzzy-matches it against the metadata database. Up to 5 candidates appear in the Track tab.

Click a candidate to confirm the match.

### 2. Search (Matches tab)

After confirming a candidate, Tiger Tanda searches the full database for songs that would work in a tanda together. Results are filtered by your active settings (artist, singer, orchestra, genre, grouping, label, year range) and sorted by year.

Up to 20 matching songs appear in the Matches tab.

### 3. Browse (Browse tab)

Select a match and click **Search in VDJ** to search for it in VirtualDJ's browser. Tiger Tanda scores the browser results by how well they match (artist, title, year, stars, play count) and shows the top 5.

Click a result to jump to it in VirtualDJ's browser. Use the prelisten button to preview the song directly.

## Settings

Open the **Settings** tab to configure search filters:

- **Artist** — match songs by the same artist
- **Singer** — match songs with the same singer
- **Orchestra** — match songs by the same orchestra
- **Genre** — match songs in the same genre
- **Grouping** — match songs with the same grouping
- **Label** — match songs on the same label
- **Year range** — toggle on/off and cycle through ranges (±2, 3, 5, 8, or 10 years)

Filters are combined: only songs matching all active filters appear in results. Settings persist between sessions.

## Building from Source

### Prerequisites

- CMake 3.22+
- **Windows**: Visual Studio 2019+ with C++ desktop workload
- **macOS**: Xcode command-line tools

### Windows

```bash
cmake -B build -A x64
cmake --build build --config Release
```

Output: `build/Release/TigerTanda.dll` and `build/Release/metadata.csv`

### macOS

```bash
cmake -B build
cmake --build build --config Release
```

Output: `build/TigerTanda.bundle` and `build/metadata.csv`

## License

[MIT](LICENSE)
