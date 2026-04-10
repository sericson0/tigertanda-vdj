# Two-Column Layout Redesign

## Summary

Combine Track, Matches, and Browse tabs into a single two-column view. Simplifies the workflow from 4 tabs to 2 (main view + Settings).

## Layout

- **Window size**: ~700w x 380h (up from 360x336)
- **60/40 column split**, no vertical divider between columns

### Top Bar
- "Tiger Tanda" brand text (left, accent color, bold)
- Settings gear button (right of brand)
- Close X button (far right)
- No tab strip — only 2 views: main (default) and Settings

### Left Column (60%)
1. **Column headers**: TITLE / ARTIST / YEAR labels above inputs
2. **3 search inputs**: Title (flex:5), Artist (flex:4), Year (fixed ~52px, centered)
   - Auto-populated from VDJ browser polling (title, artist, year)
   - Editing any box triggers `runIdentification()` (no search button)
3. **2 candidate rows**: Aligned column-for-column with inputs (title | artist | year)
   - No color score dots
   - Selected candidate highlighted with left accent border
   - Clicking confirms candidate and runs tanda search
4. **Horizontal separator**
5. **"MATCHES (N)" section header**
6. **Scrollable matches list**: Each row shows title | artist | year (same column structure)
   - Clicking a match auto-triggers VDJ smart search (replaces FIND TRACK button)

### Right Column (40%)
1. **"SELECTED TRACK" section header**
2. **Detail box**: Bandleader · Singer / Date · Genre · Label / Orchestra / Group
   - Shows metadata for the selected match
3. **"VDJ BROWSER RESULTS" section header**
4. **Scrollable browse list**: Title | Artist columns (smart search ranked results)
5. **Prelisten + ADD row**: Play/pause button | waveform | ADD button

## Workflow
1. Browse in VDJ → inputs auto-fill → candidates appear (2 max)
2. Click candidate → tanda search → matches list populates
3. Click match → auto VDJ search → right column shows detail + browse results
4. Prelisten / ADD from right column

## Controls Removed
- `IDC_BTN_SEARCH` (search button) — replaced by live search on edit
- `IDC_BTN_SEARCH_VDJ` (FIND TRACK) — replaced by auto-search on match selection
- `IDC_BTN_TAB_TRACK`, `IDC_BTN_TAB_MATCHES`, `IDC_BTN_TAB_BROWSE` — replaced by single main view
- Tab strip — replaced by brand text in top bar

## Controls Added
- `IDC_EDIT_YEAR` — year input box
- Column header labels (painted, not controls)
- Section header labels (painted, not controls)

## Controls Retained
- `IDC_EDIT_TITLE`, `IDC_EDIT_ARTIST` — now with live search via EN_CHANGE
- `IDC_CANDIDATES_LIST` — reduced to 2 max, new column layout
- `IDC_RESULTS_LIST` — new 3-column layout (title|artist|year)
- `IDC_BROWSE_LIST`, `IDC_BTN_PRELISTEN`, `IDC_BTN_ADD_END` — unchanged
- `IDC_BTN_CLOSE` — moved to top bar right
- `IDC_BTN_TAB_SETTINGS` — still toggles Settings view
- All filter/settings controls — unchanged

## Key Behavior Changes
- `EN_CHANGE` on any input box runs identification with debounce (~300ms timer)
- Clicking a match in results list auto-fires smart search (same logic as old FIND TRACK)
- `activeTab` simplifies: 0 = main view, 1 = Settings
- VDJ polling now also reads year: `get_browsed_song 'year'`
- Candidates capped at 2 (was 5)
- `CAND_ITEM_H` reduced to single-row height (~24px, aligned with input columns)
