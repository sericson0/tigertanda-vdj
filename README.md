<p align="center">
  <img src="TigerTandaLogoV2.png" alt="Tiger Tanda" width="200">
</p>

VirtualDJ plugin to help tango DJs build tandas.

Tiger Tanda identifies the song you're browsing in VirtualDJ, matches it against a curated tango metadata database, and helps you find complementary songs to build a complete tanda.

## Installation

### Windows (Installer)

1. Download `TigerTanda-Windows-Installer.exe` from [Releases](https://github.com/sericson0/tigertanda-vdj/releases)
2. Run the installer — it defaults to `Documents\VirtualDJ\Plugins64\SoundEffect\TigerTanda\`
3. Restart VirtualDJ
4. Click one of the Master Effects slots -> click More... -> Select TigerTanda -> Use the '>' button to add.
5. You can now add TigerTanda to an effects slot and open it.

### Windows (Manual)

1. Download `TigerTanda-Windows.zip` from [Releases](https://github.com/sericson0/tigertanda-vdj/releases)
2. Extract the zip — you'll get `TigerTanda.dll` and `metadata.csv`
3. Copy both files to:
   ```
   Documents\VirtualDJ\Plugins64\SoundEffect\TigerTanda\
   ```
4. Restart VirtualDJ
5. Click one of the Master Effects slots -> click More... -> Select TigerTanda -> Use the '>' button to add.
6. You can now add TigerTanda to an effects slot and open it.

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

Tiger Tanda uses a two-column layout with a Main view and a Settings view, toggled via the gear icon.

### Main View

The main view has two columns:

**Left column — Identify & Match:**

1. **Search row** — title, artist, and year fields auto-populate as you browse songs in VirtualDJ. Use the lock icon to freeze the search fields while you navigate away.
2. **Candidates** — up to 3 fuzzy-matched candidates from the metadata database. Click one to confirm the match and trigger a tanda search.
3. **Matches** — up to 20 songs that pair well with the confirmed track, filtered by your active settings and sorted by year. Double-click a match to search for it in VirtualDJ's browser.

**Right column — Browse & Add:**

4. **Detail box** — shows detailed metadata for the selected match.
5. **Browse list** — top results from VirtualDJ's browser, scored by how well they match (artist, title, year, stars, play count) and displayed with cover art thumbnails.
6. **Prelisten & Add** — preview a song with the waveform display, then click **ADD** to add it to VirtualDJ's automix playlist (or right-click to add to the sidelist).

### Settings View

Toggle the gear icon to open Settings. The left column has search filters, and the right column explains how the plugin works.

**Search filters:**

- **Artist** — match songs by the same artist
- **Singer** — match songs with the same singer
- **Orchestra** — match songs by the same orchestra
- **Genre** — match songs in the same genre
- **Grouping** — match songs with the same grouping
- **Label** — match songs on the same label
- **Track** — match songs with the same track name
- **Year range** — toggle on/off and adjust the range with +/- buttons (±2, 3, 5, 8, or 10 years)

Filters are combined: only songs matching all active filters appear in results. Settings persist between sessions.

Questions or suggestions? Email [tangotoolkit@gmail.com](mailto:tangotoolkit@gmail.com).

## License

[MIT](LICENSE)
