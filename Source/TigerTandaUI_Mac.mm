//==============================================================================
// TigerTanda VDJ Plugin - macOS UI
// Custom NSView drawing to pixel-match the Windows version
//==============================================================================

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>

#include "TigerTanda.h"
#include "CoverArt.h"

#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline NSColor* ttNSColor (TTColor c)
{
    return [NSColor colorWithCalibratedRed:c.r green:c.g blue:c.b alpha:1.0];
}

static inline void cgFill (CGContextRef ctx, CGRect r, TTColor c)
{
    CGContextSetRGBFillColor (ctx, c.r, c.g, c.b, 1.0);
    CGContextFillRect (ctx, r);
}

static inline CGRect cgR (int x, int y, int w, int h)
{
    return CGRectMake (x, y, w, h);
}

// Draw text in a rect with specified font, color, alignment
static void cgDrawText (CGContextRef ctx, NSString* text, CGRect r,
                        NSFont* font, TTColor color, NSTextAlignment align = NSTextAlignmentLeft,
                        BOOL singleLine = YES, BOOL ellipsis = YES)
{
    if (!text || text.length == 0) return;

    NSMutableParagraphStyle* para = [[NSMutableParagraphStyle alloc] init];
    para.alignment = align;
    if (singleLine)
        para.lineBreakMode = ellipsis ? NSLineBreakByTruncatingTail : NSLineBreakByClipping;
    else
        para.lineBreakMode = NSLineBreakByWordWrapping;

    NSDictionary* attrs = @{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: ttNSColor (color),
        NSParagraphStyleAttributeName: para
    };

    // Vertically center single-line text
    if (singleLine)
    {
        NSSize sz = [text sizeWithAttributes:attrs];
        CGFloat yOff = (r.size.height - sz.height) / 2.0;
        if (yOff < 0) yOff = 0;
        CGRect textR = CGRectMake (r.origin.x, r.origin.y + yOff,
                                   r.size.width, r.size.height - yOff);
        [text drawInRect:textR withAttributes:attrs];
    }
    else
    {
        [text drawInRect:r withAttributes:attrs];
    }
}

static NSString* toNS (const std::wstring& ws)
{
    if (ws.empty()) return @"";
    std::string utf8;
    utf8.reserve (ws.size() * 2);
    for (wchar_t wc : ws)
    {
        uint32_t cp = (uint32_t) wc;
        if (cp < 0x80)
            utf8 += (char) cp;
        else if (cp < 0x800)
        {
            utf8 += (char) (0xC0 | (cp >> 6));
            utf8 += (char) (0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            utf8 += (char) (0xE0 | (cp >> 12));
            utf8 += (char) (0x80 | ((cp >> 6) & 0x3F));
            utf8 += (char) (0x80 | (cp & 0x3F));
        }
        else
        {
            utf8 += (char) (0xF0 | (cp >> 18));
            utf8 += (char) (0x80 | ((cp >> 12) & 0x3F));
            utf8 += (char) (0x80 | ((cp >> 6) & 0x3F));
            utf8 += (char) (0x80 | (cp & 0x3F));
        }
    }
    return [NSString stringWithUTF8String:utf8.c_str()];
}

static std::wstring fromNS (NSString* ns)
{
    if (!ns || ns.length == 0) return {};
    const char* utf8 = [ns UTF8String];
    return toWide (std::string (utf8));
}

// Extract last name from "First Last" or "Last, First"
static std::wstring lastName (const std::wstring& name)
{
    if (name.empty()) return {};
    auto comma = name.find (L',');
    if (comma != std::wstring::npos)
        return name.substr (0, comma);
    auto space = name.rfind (L' ');
    if (space != std::wstring::npos)
        return name.substr (space + 1);
    return name;
}

static std::wstring formatArtist (const TgRecord& rec)
{
    std::wstring out = lastName (rec.bandleader);
    if (!rec.singer.empty())
    {
        std::wstring s = lastName (rec.singer);
        if (!s.empty())
        {
            if (!out.empty()) out += L" - ";
            out += s;
        }
    }
    return out;
}

static std::wstring formatDateYMD (const std::wstring& d)
{
    if (d.empty()) return {};
    // Try M/D/YYYY
    int m = 0, day = 0, y = 0;
    if (swscanf (d.c_str(), L"%d/%d/%d", &m, &day, &y) == 3 && y > 1900)
    {
        wchar_t buf[32];
        swprintf (buf, sizeof(buf)/sizeof(buf[0]), L"%04d-%02d-%02d", y, m, day);
        return buf;
    }
    return d;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
//  TTCenteredTextFieldCell — vertically centers text in NSTextField (#1)
// ─────────────────────────────────────────────────────────────────────────────

@interface TTCenteredTextFieldCell : NSTextFieldCell
@end

@implementation TTCenteredTextFieldCell
- (NSRect)adjustedFrameToVerticallyCenterText:(NSRect)frame
{
    NSAttributedString* as = self.attributedStringValue;
    NSSize textSize = [as size];
    CGFloat yOff = (frame.size.height - textSize.height) / 2.0;
    if (yOff < 0) yOff = 0;
    return NSMakeRect (frame.origin.x, frame.origin.y + yOff,
                       frame.size.width, textSize.height);
}

- (void)editWithFrame:(NSRect)r inView:(NSView*)v editor:(NSText*)t delegate:(id)d event:(NSEvent*)e
{
    [super editWithFrame:[self adjustedFrameToVerticallyCenterText:r] inView:v editor:t delegate:d event:e];
}

- (void)selectWithFrame:(NSRect)r inView:(NSView*)v editor:(NSText*)t delegate:(id)d start:(NSInteger)s length:(NSInteger)l
{
    [super selectWithFrame:[self adjustedFrameToVerticallyCenterText:r] inView:v editor:t delegate:d start:s length:l];
}

- (void)drawInteriorWithFrame:(NSRect)r inView:(NSView*)v
{
    [super drawInteriorWithFrame:[self adjustedFrameToVerticallyCenterText:r] inView:v];
}
@end

@class TTListView;

// ─────────────────────────────────────────────────────────────────────────────
//  TigerTandaMacUI — Main NSView (interface declared early so TTListView can use it)
// ─────────────────────────────────────────────────────────────────────────────

@interface TigerTandaMacUI : NSView <NSTextFieldDelegate, NSWindowDelegate>
{
@public
    TigerTandaPlugin* plugin;

    NSTextField* editTitle;
    NSTextField* editArtist;
    NSTextField* editYear;

    NSScrollView* candScroll;
    NSScrollView* resultsScroll;
    NSScrollView* browseScroll;

    TTListView* candList;
    TTListView* resultsList;
    TTListView* browseList;

    NSTimer* browsePollTimer;
    NSTimer* searchDebounceTimer;

    // Cached rects for settings tab click handling
    CGRect settingsToggleRect;
    CGRect yearToggleRect;
    CGRect yearMinusRect;
    CGRect yearPlusRect;
    CGRect filterRects[7];
}
- (instancetype)initWithFrame:(NSRect)frame plugin:(TigerTandaPlugin*)p;
- (void)layoutUI;
@end

// ─────────────────────────────────────────────────────────────────────────────
//  TTListView — custom scrollable list with owner-drawn rows
// ─────────────────────────────────────────────────────────────────────────────

@interface TTListView : NSView
{
@public
    TigerTandaPlugin* plugin;
    int listType;  // 0=candidates, 1=results, 2=browse
    int scrollOffset;
}
@property (nonatomic, weak) TigerTandaMacUI* parentUI;
@end

@implementation TTListView

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (int)itemHeight
{
    if (listType == 0) return CAND_ITEM_H;
    if (listType == 1) return RESULT_ITEM_H;
    return BROWSE_ITEM_H;
}

- (int)itemCount
{
    if (!plugin) return 0;
    if (listType == 0) return (int) plugin->candidates.size();
    if (listType == 1) return (int) plugin->results.size();
    return (int) plugin->browseItems.size();
}

- (int)selectedIndex
{
    if (!plugin) return -1;
    if (listType == 0) return plugin->confirmedIdx;
    if (listType == 1) return plugin->selectedResultIdx;
    return plugin->selectedBrowseIdx;
}

- (void)drawRect:(NSRect)dirtyRect
{
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    NSRect bounds = self.bounds;

    // Fill background
    cgFill (ctx, bounds, TCol::bg);

    int count = [self itemCount];
    int itemH = [self itemHeight];
    int selIdx = [self selectedIndex];
    int w = (int) bounds.size.width;

    NSFont* fontNorm = [NSFont systemFontOfSize:FONT_SIZE_NORMAL];
    NSFont* fontBold = [NSFont boldSystemFontOfSize:FONT_SIZE_NORMAL];
    NSFont* fontSm   = [NSFont systemFontOfSize:FONT_SIZE_SMALL];
    NSFont* fontSmB  = [NSFont boldSystemFontOfSize:FONT_SIZE_SMALL];

    for (int i = 0; i < count; ++i)
    {
        int y = i * itemH;
        if (y + itemH < (int) dirtyRect.origin.y) continue;
        if (y > (int) (dirtyRect.origin.y + dirtyRect.size.height)) break;

        bool sel = (i == selIdx);
        bool even = (i % 2 == 0);

        CGRect rowR = cgR (0, y, w, itemH);
        cgFill (ctx, rowR, sel ? TCol::selSubtle : even ? TCol::card : TCol::panel);

        // Left accent bar
        if (sel)
            cgFill (ctx, cgR (0, y, 3, itemH), TCol::accent);

        // Bottom border
        cgFill (ctx, cgR (0, y + itemH - 1, w, 1), TCol::cardBorder);

        if (listType == 0 && i < (int) plugin->candidates.size())
        {
            // Candidates: title | artist | year
            const TgRecord& rec = plugin->candidates[i].record;
            int gap = 4, textInset = 6;
            int titleColW = (w - YEAR_COL_W - gap * 2) * 55 / 100;
            int artistColW = w - titleColW - YEAR_COL_W - gap * 2;

            NSFont* rowFont = sel ? fontBold : fontNorm;
            cgDrawText (ctx, toNS (rec.title), cgR (textInset, y, titleColW - textInset * 2, itemH),
                        rowFont, TCol::textBright);
            cgDrawText (ctx, toNS (formatArtist (rec)), cgR (titleColW + gap + textInset, y, artistColW - textInset * 2, itemH),
                        rowFont, sel ? TCol::textBright : TCol::textDim);
            cgDrawText (ctx, toNS (rec.year), cgR (w - YEAR_COL_W, y, YEAR_COL_W, itemH),
                        rowFont, sel ? TCol::textBright : TCol::textDim, NSTextAlignmentCenter);
        }
        else if (listType == 1 && i < (int) plugin->results.size())
        {
            // Results: title | artist | year
            const TgRecord& rec = plugin->results[i];
            int gap = 4, textInset = 6;
            int titleColW = (w - YEAR_COL_W - gap * 2) * 55 / 100;
            int artistColW = w - titleColW - YEAR_COL_W - gap * 2;

            NSFont* rowFont = sel ? fontBold : fontNorm;
            cgDrawText (ctx, toNS (rec.title), cgR (textInset, y, titleColW - textInset * 2, itemH),
                        rowFont, TCol::textBright);
            cgDrawText (ctx, toNS (formatArtist (rec)), cgR (titleColW + gap + textInset, y, artistColW - textInset * 2, itemH),
                        rowFont, sel ? TCol::textBright : TCol::textDim);

            std::wstring yearStr = rec.year.empty()
                ? (rec.date.size() >= 4 ? rec.date.substr (0, 4) : rec.date)
                : rec.year;
            cgDrawText (ctx, toNS (yearStr), cgR (w - YEAR_COL_W, y, YEAR_COL_W, itemH),
                        rowFont, sel ? TCol::textBright : TCol::textDim, NSTextAlignmentCenter);
        }
        else if (listType == 2 && i < (int) plugin->browseItems.size())
        {
            // Browse: 2 rows + album art
            const BrowseItem& bi = plugin->browseItems[i];
            int artPad = 4;
            int artSize = itemH - artPad * 2;
            int artX = w - artSize - artPad;
            int tx = 6;
            int textRight = artX - 6;
            int halfH = itemH / 2;

            // Album art
            void* coverImg = CoverArt::getForPath (bi.filePath);
            if (coverImg)
            {
                NSImage* img = (__bridge NSImage*) coverImg;
                NSRect artRect = NSMakeRect (artX, y + artPad, artSize, artSize);
                [img drawInRect:artRect fromRect:NSZeroRect
                      operation:NSCompositingOperationSourceOver fraction:1.0
                 respectFlipped:YES hints:@{NSImageHintInterpolation: @(NSImageInterpolationHigh)}];
            }
            else
            {
                cgFill (ctx, cgR (artX, y + artPad, artSize, artSize), TCol::waveformBg);
                cgFill (ctx, cgR (artX, y + artPad, artSize, 1), TCol::cardBorder);
                cgFill (ctx, cgR (artX, y + artPad + artSize - 1, artSize, 1), TCol::cardBorder);
                cgFill (ctx, cgR (artX, y + artPad, 1, artSize), TCol::cardBorder);
                cgFill (ctx, cgR (artX + artSize - 1, y + artPad, 1, artSize), TCol::cardBorder);
            }

            // Row 1: title + year
            int titleW = textRight - tx - YEAR_COL_W - 4;
            cgDrawText (ctx, toNS (bi.title), cgR (tx, y, titleW, halfH),
                        fontBold, TCol::textBright);
            cgDrawText (ctx, toNS (bi.year), cgR (textRight - YEAR_COL_W, y, YEAR_COL_W, halfH),
                        fontSm, sel ? TCol::textBright : TCol::textDim, NSTextAlignmentRight);

            // Row 2: artist
            cgDrawText (ctx, toNS (bi.artist), cgR (tx, y + halfH, textRight - tx, halfH),
                        fontSm, sel ? TCol::textBright : TCol::textDim);
        }
    }

    // Update intrinsic content size for scroll view
    int totalH = count * itemH;
    if (totalH < 1) totalH = 1;
    NSSize frameSize = self.enclosingScrollView ? self.enclosingScrollView.contentSize : bounds.size;
    if (totalH > (int) frameSize.height)
        [self setFrameSize:NSMakeSize (frameSize.width, totalH)];
    else
        [self setFrameSize:frameSize];
}

- (void)mouseDown:(NSEvent*)event
{
    NSPoint loc = [self convertPoint:[event locationInWindow] fromView:nil];
    int itemH = [self itemHeight];
    int idx = (int) (loc.y / itemH);
    int count = [self itemCount];
    if (idx < 0 || idx >= count) return;

    if (listType == 0)
    {
        plugin->confirmCandidate (idx);
        [self setNeedsDisplay:YES];
        [_parentUI setNeedsDisplay:YES];
    }
    else if (listType == 1)
    {
        if (idx == plugin->selectedResultIdx) return;
        plugin->selectedResultIdx = idx;
        plugin->smartSearchActiveToken = ++plugin->smartSearchToken;
        plugin->smartSearchNoResults = false;
        plugin->browseItems.clear();
        plugin->selectedBrowseIdx = -1;
        plugin->resultsLastInputWasMouse = true;
        plugin->lastSmartSearchTitle.clear();
        plugin->lastSmartSearchArtist.clear();
        plugin->triggerBrowserSearch (plugin->results[idx]);
        [self setNeedsDisplay:YES];
        [_parentUI setNeedsDisplay:YES];
    }
    else if (listType == 2)
    {
        plugin->selectedBrowseIdx = idx;
        rebuildPrelistenWaveBins (plugin->prelistenWaveBins,
                                  plugin->browseItems[idx].filePath);
        [self setNeedsDisplay:YES];
        [_parentUI setNeedsDisplay:YES];
    }
}

- (void)keyDown:(NSEvent*)event
{
    unsigned short keyCode = [event keyCode];
    // 125 = down, 126 = up, 36 = return, 53 = escape, 48 = tab
    int count = [self itemCount];
    int selIdx = [self selectedIndex];

    if (keyCode == 53) // Escape
    {
        [self.window close];
        return;
    }

    if (keyCode == 125) // Down
    {
        if (listType == 1)
        {
            if (selIdx < 0 && count > 0)
            {
                // Select first result
                plugin->selectedResultIdx = 0;
                plugin->resultsLastInputWasMouse = false;
                plugin->smartSearchActiveToken = ++plugin->smartSearchToken;
                plugin->uiSetTimer (TT_TIMER_MATCH_SELECT, 250);
                [self setNeedsDisplay:YES];
                [_parentUI setNeedsDisplay:YES];
                return;
            }
            if (selIdx + 1 < count)
            {
                plugin->selectedResultIdx = selIdx + 1;
                plugin->resultsLastInputWasMouse = false;
                plugin->uiSetTimer (TT_TIMER_MATCH_SELECT, 250);
                [self setNeedsDisplay:YES];
                [_parentUI setNeedsDisplay:YES];
                return;
            }
        }
        else if (listType == 0)
        {
            // At bottom of candidates, jump to results list
            if ((selIdx < 0 || selIdx >= count - 1) && !plugin->results.empty())
            {
                // Focus results list
                TTListView* resultsList = nil;
                for (NSView* v in self.superview.superview.subviews)
                {
                    if ([v isKindOfClass:[NSScrollView class]])
                    {
                        NSScrollView* sv = (NSScrollView*) v;
                        if ([sv.documentView isKindOfClass:[TTListView class]])
                        {
                            TTListView* lv = (TTListView*) sv.documentView;
                            if (lv->listType == 1)
                            {
                                resultsList = lv;
                                break;
                            }
                        }
                    }
                }
                if (resultsList)
                {
                    [self.window makeFirstResponder:resultsList];
                    if (plugin->selectedResultIdx < 0)
                    {
                        plugin->selectedResultIdx = 0;
                        plugin->resultsLastInputWasMouse = false;
                        plugin->uiSetTimer (TT_TIMER_MATCH_SELECT, 250);
                    }
                    [resultsList setNeedsDisplay:YES];
                    [_parentUI setNeedsDisplay:YES];
                    return;
                }
            }
        }
    }

    if (keyCode == 126) // Up
    {
        if (listType == 1 && selIdx > 0)
        {
            plugin->selectedResultIdx = selIdx - 1;
            plugin->resultsLastInputWasMouse = false;
            plugin->uiSetTimer (TT_TIMER_MATCH_SELECT, 250);
            [self setNeedsDisplay:YES];
            [_parentUI setNeedsDisplay:YES];
            return;
        }
    }

    if (keyCode == 36) // Return
    {
        if (listType == 1 && selIdx >= 0 && selIdx < count && !plugin->smartSearchPending)
        {
            plugin->lastSmartSearchTitle.clear();
            plugin->lastSmartSearchArtist.clear();
            plugin->triggerBrowserSearch (plugin->results[selIdx]);
            return;
        }
        if (listType == 2 && selIdx >= 0 && selIdx < count)
        {
            plugin->addSelectedBrowseToAutomix();
            return;
        }
    }

    [super keyDown:event];
}

@end

// ─────────────────────────────────────────────────────────────────────────────
//  TigerTandaMacUI — Main NSView (implementation)
// ─────────────────────────────────────────────────────────────────────────────

@implementation TigerTandaMacUI

- (instancetype)initWithFrame:(NSRect)frame plugin:(TigerTandaPlugin*)p
{
    self = [super initWithFrame:frame];
    if (!self) return nil;
    plugin = p;

    // ── Search fields ────────────────────────────────────────────────────
    auto makeField = [&](int x, int y, int w, int h) -> NSTextField* {
        NSTextField* f = [[NSTextField alloc] initWithFrame:NSMakeRect (x, y, w, h)];
        // Replace cell with vertically-centered version
        TTCenteredTextFieldCell* cell = [[TTCenteredTextFieldCell alloc] initTextCell:@""];
        cell.editable = YES;
        cell.selectable = YES;
        cell.scrollable = YES;
        cell.usesSingleLineMode = YES;
        [f setCell:cell];
        f.bordered = NO;
        f.drawsBackground = YES;
        f.backgroundColor = [NSColor colorWithCalibratedRed:32/255.0 green:36/255.0 blue:52/255.0 alpha:1.0];
        f.textColor = ttNSColor (TCol::textBright);
        f.font = [NSFont systemFontOfSize:FONT_SIZE_NORMAL];
        f.focusRingType = NSFocusRingTypeNone;
        f.delegate = self;
        [self addSubview:f];
        return f;
    };

    // Positions will be set in layoutSubviews
    editTitle  = makeField (0, 0, 100, EDIT_H);
    editArtist = makeField (0, 0, 100, EDIT_H);
    editYear   = makeField (0, 0, 100, EDIT_H);

    // ── List views ───────────────────────────────────────────────────────
    auto makeList = [&](int type) -> std::pair<NSScrollView*, TTListView*> {
        NSScrollView* sv = [[NSScrollView alloc] initWithFrame:NSZeroRect];
        sv.hasVerticalScroller = (type == 1); // only results list scrolls
        sv.hasHorizontalScroller = NO;
        sv.drawsBackground = NO;
        sv.borderType = NSNoBorder;

        TTListView* lv = [[TTListView alloc] initWithFrame:NSZeroRect];
        lv->plugin = plugin;
        lv->listType = type;
        lv->scrollOffset = 0;
        lv.parentUI = self;
        sv.documentView = lv;

        [self addSubview:sv];
        return {sv, lv};
    };

    auto [cs, cl] = makeList (0);
    candScroll = cs; candList = cl;
    auto [rs, rl] = makeList (1);
    resultsScroll = rs; resultsList = rl;
    auto [bs, bl] = makeList (2);
    browseScroll = bs; browseList = bl;

    // Settings toggle rect — drawn in drawRect, clicked in mouseDown
    settingsToggleRect = CGRectZero;

    // ── Timers ───────────────────────────────────────────────────────────
    browsePollTimer = [NSTimer scheduledTimerWithTimeInterval:0.25
        target:self selector:@selector(browsePollTick:) userInfo:nil repeats:YES];

    [self layoutUI];
    return self;
}

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (void)dealloc
{
    [browsePollTimer invalidate];
    [searchDebounceTimer invalidate];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────────────────────────────────────

- (void)layoutUI
{
    int leftW = DLG_W * LEFT_COL_PCT / 100;
    int rightW = DLG_W - leftW;
    int COL_GAP = 4;
    int lx = PAD;
    int lw = leftW - PAD - COL_GAP / 2;
    int rx = leftW + COL_GAP / 2;
    int rw = rightW - PAD - COL_GAP / 2;

    bool showMain = (plugin->activeTab == 0);

    // Settings toggle rect (drawn custom in drawRect, hit-tested in mouseDown)
    int toggleW = 140;
    int topY = (TOP_H - TAB_BTN_H) / 2;
    settingsToggleRect = cgR (DLG_W - PAD - toggleW, topY, toggleW, TAB_BTN_H);

    int bannerShift = plugin->metadataLoadFailed ? META_BANNER_H : 0;
    plugin->columnHeaderY = TOP_H + TOP_GAP + bannerShift;

    if (showMain)
    {
        // Search inputs
        int ly = plugin->columnHeaderY + 14;
        int gap = 4;
        int titleW = (lw - YEAR_COL_W - gap * 2) * 55 / 100;
        int artistW = lw - titleW - YEAR_COL_W - gap * 2;
        editTitle.frame  = NSMakeRect (lx, ly, titleW, EDIT_H);
        editArtist.frame = NSMakeRect (lx + titleW + gap, ly, artistW, EDIT_H);
        editYear.frame   = NSMakeRect (lx + lw - YEAR_COL_W, ly, YEAR_COL_W, EDIT_H);
        editTitle.hidden = NO;
        editArtist.hidden = NO;
        editYear.hidden = NO;
        ly += EDIT_H + TRACK_SEARCH_GAP;

        // Candidates list
        int candH = CAND_ITEM_H * 3 + 2;
        candScroll.frame = NSMakeRect (lx, ly, lw, candH);
        candScroll.hidden = NO;
        candList.frame = NSMakeRect (0, 0, lw, candH);
        ly += candH + 6;

        // Matches header
        plugin->matchHeaderY = ly;
        int matchListTop = ly + 14;
        int matchListBot = DLG_H - 4;
        resultsScroll.frame = NSMakeRect (lx, matchListTop, lw, matchListBot - matchListTop);
        resultsScroll.hidden = NO;
        resultsList.frame = NSMakeRect (0, 0, lw, matchListBot - matchListTop);

        // Right column
        int ry = TOP_H + bannerShift;
        int detailBot = ry + DETAIL_BOX_H;
        int browseTop = detailBot + BROWSE_HEADER_H;
        plugin->browseResultsHeaderY = detailBot + 2;
        int browseH = BROWSE_ITEM_H * 4 + 2;
        browseScroll.frame = NSMakeRect (rx, browseTop, rw, browseH);
        browseScroll.hidden = plugin->browseItems.empty();
        browseList.frame = NSMakeRect (0, 0, rw, browseH);
    }
    else
    {
        // Settings — hide main controls
        editTitle.hidden = YES;
        editArtist.hidden = YES;
        editYear.hidden = YES;
        candScroll.hidden = YES;
        resultsScroll.hidden = YES;
        browseScroll.hidden = YES;
    }

    [self setNeedsDisplay:YES];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Drawing
// ─────────────────────────────────────────────────────────────────────────────

- (void)drawRect:(NSRect)dirtyRect
{
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    NSRect bounds = self.bounds;

    // Background
    cgFill (ctx, bounds, TCol::bg);

    // Top bar
    cgFill (ctx, cgR (0, 0, DLG_W, TOP_H), TCol::panel);

    // Brand text: "Tiger Tanda"
    NSFont* fontBrand = [NSFont boldSystemFontOfSize:FONT_SIZE_BRAND];
    cgDrawText (ctx, @"Tiger Tanda", cgR (PAD, 0, DLG_W / 2, TOP_H),
                fontBrand, TCol::accent);

    // Settings toggle (custom-drawn rounded pill)
    {
        NSFont* fontSm = [NSFont boldSystemFontOfSize:FONT_SIZE_SMALL];
        CGRect tr = settingsToggleRect;
        bool isSettings = (plugin->activeTab == 1);
        NSString* label = isSettings ? @"Settings" : @"Tanda";

        // Rounded rect background
        CGFloat radius = tr.size.height / 2.0;
        NSBezierPath* pill = [NSBezierPath bezierPathWithRoundedRect:
            NSMakeRect(tr.origin.x, tr.origin.y, tr.size.width, tr.size.height)
            xRadius:radius yRadius:radius];
        [ttNSColor (TCol::buttonBg) setFill];
        [pill fill];

        // Active indicator on the correct half
        CGFloat halfW = tr.size.width / 2.0;
        CGRect activeR = isSettings
            ? cgR (tr.origin.x + halfW, tr.origin.y, halfW, tr.size.height)
            : cgR (tr.origin.x, tr.origin.y, halfW, tr.size.height);
        NSBezierPath* activePill = [NSBezierPath bezierPathWithRoundedRect:
            NSMakeRect(activeR.origin.x, activeR.origin.y, activeR.size.width, activeR.size.height)
            xRadius:radius yRadius:radius];
        [ttNSColor (TCol::accent) setFill];
        [activePill fill];

        // Labels
        cgDrawText (ctx, @"Tanda", cgR (tr.origin.x, tr.origin.y, halfW, tr.size.height),
                    fontSm, !isSettings ? TCol::textBright : TCol::textDim, NSTextAlignmentCenter);
        cgDrawText (ctx, @"Settings", cgR (tr.origin.x + halfW, tr.origin.y, halfW, tr.size.height),
                    fontSm, isSettings ? TCol::textBright : TCol::textDim, NSTextAlignmentCenter);
    }

    bool showMain = (plugin->activeTab == 0);

    if (showMain)
        [self drawMainTab:ctx];
    else
        [self drawSettingsTab:ctx];
}

- (void)drawMainTab:(CGContextRef)ctx
{
    int leftW = DLG_W * LEFT_COL_PCT / 100;
    int rightW = DLG_W - leftW;
    int COL_GAP = 4;
    int lx = PAD;
    int lw = leftW - PAD - COL_GAP / 2;
    int rx = leftW + COL_GAP / 2;
    int rw = rightW - PAD - COL_GAP / 2;

    NSFont* fontSm   = [NSFont systemFontOfSize:FONT_SIZE_SMALL];
    NSFont* fontBold  = [NSFont boldSystemFontOfSize:FONT_SIZE_NORMAL];
    NSFont* fontNorm  = [NSFont systemFontOfSize:FONT_SIZE_NORMAL];

    int bannerShift = plugin->metadataLoadFailed ? META_BANNER_H : 0;

    // Metadata load failure banner
    if (plugin->metadataLoadFailed)
    {
        int by = TOP_H + TOP_GAP;
        cgFill (ctx, cgR (PAD, by, DLG_W - PAD * 2, META_BANNER_H), TCol::filterActive);
        cgDrawText (ctx, @"metadata.csv not found — click to reload",
                    cgR (PAD + 6, by, DLG_W - PAD * 2 - 12, META_BANNER_H),
                    fontSm, TCol::textBright);
    }

    // Column headers
    {
        int hy = plugin->columnHeaderY;
        int gap = 4, usableW = lw;
        int titleColW = (usableW - YEAR_COL_W - gap * 2) * 55 / 100;
        int artistColW = usableW - titleColW - YEAR_COL_W - gap * 2;
        cgDrawText (ctx, @"TITLE", cgR (lx + 6, hy, titleColW, 14), fontSm, TCol::textDim);
        cgDrawText (ctx, @"ARTIST", cgR (lx + titleColW + gap + 6, hy, artistColW, 14), fontSm, TCol::textDim);
        cgDrawText (ctx, @"YEAR", cgR (lx + usableW - YEAR_COL_W, hy, YEAR_COL_W, 14), fontSm, TCol::textDim, NSTextAlignmentCenter);
    }

    // "MATCHES (N)" header
    {
        int my = plugin->matchHeaderY;
        int matchCount = (int) plugin->results.size();
        NSString* hdr = [NSString stringWithFormat:@"MATCHES (%d)", matchCount];
        cgDrawText (ctx, hdr, cgR (lx + 6, my, 200, 14), fontSm, TCol::textDim);
    }

    // Right column: Detail box
    int ry = TOP_H + bannerShift;
    [self drawDetailBox:ctx rect:cgR (rx, ry, rw, DETAIL_BOX_H)];

    // "VDJ BROWSER RESULTS" header
    cgDrawText (ctx, @"VDJ BROWSER RESULTS",
                cgR (rx + 6, plugin->browseResultsHeaderY, rw, BROWSE_HEADER_H),
                fontSm, TCol::textDim);

    // Browse placeholder (when empty)
    if (plugin->browseItems.empty())
    {
        int browseTop = ry + DETAIL_BOX_H + BROWSE_HEADER_H;
        int browseH = BROWSE_ITEM_H * 4 + 2;
        cgFill (ctx, cgR (rx, browseTop, rw, browseH), TCol::panel);

        NSString* msg = @"Select a match to search VDJ";
        if (plugin->smartSearchPending)
            msg = @"Searching VDJ library...";
        else if (plugin->smartSearchNoResults)
            msg = @"No matches found in VDJ library";

        cgDrawText (ctx, msg, cgR (rx + 6, browseTop, rw - 12, browseH),
                    fontSm, TCol::textDim, NSTextAlignmentCenter);
    }

    // Prelisten waveform area
    if (!plugin->browseItems.empty())
    {
        int browseTop = ry + DETAIL_BOX_H + BROWSE_HEADER_H;
        int browseH = BROWSE_ITEM_H * 4 + 2;
        int preRowY = browseTop + browseH + PRELISTEN_TOP_GAP;
        int waveX = rx + 32;
        int waveW = rw - 32 - 76;
        int waveH = BTN_H;

        cgFill (ctx, cgR (waveX, preRowY, waveW, waveH), TCol::waveformBg);

        // Draw waveform bins
        if (!plugin->prelistenWaveBins.empty())
        {
            int binCount = (int) plugin->prelistenWaveBins.size();
            float binW = (float) waveW / binCount;
            for (int i = 0; i < binCount; ++i)
            {
                int h = plugin->prelistenWaveBins[i];
                int bx = waveX + (int) (i * binW);
                int bw = (int) binW > 0 ? (int) binW : 1;
                cgFill (ctx, cgR (bx, preRowY + waveH - h, bw, h), TCol::textDim);
            }
        }

        // ADD button (rounded)
        {
            NSBezierPath* addPill = [NSBezierPath bezierPathWithRoundedRect:
                NSMakeRect (rx + rw - 72, preRowY, 72, BTN_H) xRadius:4 yRadius:4];
            [ttNSColor (TCol::buttonBg) setFill];
            [addPill fill];
            cgDrawText (ctx, @"ADD", cgR (rx + rw - 72, preRowY, 72, BTN_H),
                        fontBold, TCol::textBright, NSTextAlignmentCenter);
        }

        // Prelisten button (rounded)
        {
            NSBezierPath* prePill = [NSBezierPath bezierPathWithRoundedRect:
                NSMakeRect (rx, preRowY, 28, BTN_H) xRadius:4 yRadius:4];
            [ttNSColor (TCol::buttonBg) setFill];
            [prePill fill];
            cgDrawText (ctx, @"\u25B6", cgR (rx, preRowY, 28, BTN_H),
                        fontNorm, TCol::textBright, NSTextAlignmentCenter);
        }
    }
}

- (void)drawDetailBox:(CGContextRef)ctx rect:(CGRect)r
{
    cgFill (ctx, r, TCol::card);
    cgFill (ctx, cgR (r.origin.x, r.origin.y, r.size.width, 1), TCol::cardBorder);

    // Show selected match result (not the search candidate)
    if (plugin->selectedResultIdx < 0 || plugin->selectedResultIdx >= (int) plugin->results.size())
    {
        // Fall back to confirmed candidate if no match selected
        if (plugin->confirmedIdx < 0 || plugin->confirmedIdx >= (int) plugin->candidates.size())
        {
            NSFont* fontSm = [NSFont systemFontOfSize:FONT_SIZE_SMALL];
            cgDrawText (ctx, @"No track selected", cgR (r.origin.x + 6, r.origin.y, r.size.width - 12, r.size.height),
                        fontSm, TCol::textDim, NSTextAlignmentCenter);
            return;
        }
    }

    const TgRecord& rec = (plugin->selectedResultIdx >= 0 && plugin->selectedResultIdx < (int) plugin->results.size())
        ? plugin->results[plugin->selectedResultIdx]
        : plugin->candidates[plugin->confirmedIdx].record;
    NSFont* fontBold = [NSFont boldSystemFontOfSize:FONT_SIZE_NORMAL];
    NSFont* fontNorm = [NSFont systemFontOfSize:FONT_SIZE_NORMAL];

    int px = (int) r.origin.x + DETAIL_PAD_X;
    int py = (int) r.origin.y + DETAIL_PAD_Y;
    int pw = (int) r.size.width - DETAIL_PAD_X * 2;
    int rowH = 18;

    // Row 1: Title
    cgDrawText (ctx, toNS (rec.title), cgR (px, py, pw, rowH), fontBold, TCol::textBright);
    py += rowH + DETAIL_ROW_GAP;

    // Row 2: Bandleader · Singer
    std::wstring row2;
    if (!rec.bandleader.empty()) row2 = rec.bandleader;
    if (!rec.singer.empty())
    {
        if (!row2.empty()) row2 += L"  \u00B7  ";
        row2 += rec.singer;
    }
    cgDrawText (ctx, toNS (row2), cgR (px, py, pw, rowH), fontNorm, TCol::textNormal);
    py += rowH + DETAIL_ROW_GAP;

    // Row 3: Date · Genre
    std::wstring row3 = formatDateYMD (rec.date);
    if (!rec.genre.empty())
    {
        if (!row3.empty()) row3 += L"  \u00B7  ";
        row3 += rec.genre;
    }
    cgDrawText (ctx, toNS (row3), cgR (px, py, pw, rowH), fontNorm, TCol::textDim);
    py += rowH + DETAIL_ROW_GAP;

    // Row 4: Label
    if (!rec.label.empty())
    {
        cgDrawText (ctx, toNS (L"Label: " + rec.label), cgR (px, py, pw, rowH), fontNorm, TCol::textDim);
        py += rowH + DETAIL_ROW_GAP;
    }

    // Row 5: Group
    if (!rec.grouping.empty())
        cgDrawText (ctx, toNS (L"Group: " + rec.grouping), cgR (px, py, pw, rowH), fontNorm, TCol::textDim);
}

- (void)drawSettingsTab:(CGContextRef)ctx
{
    NSFont* fontBold = [NSFont boldSystemFontOfSize:FONT_SIZE_NORMAL];
    NSFont* fontSm   = [NSFont systemFontOfSize:FONT_SIZE_SMALL];

    int settingsL = DLG_W * 50 / 100;
    int lx = PAD;
    int rx = settingsL + 5;
    int rw = DLG_W - rx - PAD;

    int ly = TOP_H + TOP_GAP + 8;

    // Left column: FILTERS header
    cgDrawText (ctx, @"FILTERS", cgR (lx + 6, ly, 200, 18), fontBold, TCol::textBright);
    ly += 24;

    // Filter buttons (rounded rects)
    struct FilterInfo { const char* label; bool* value; };
    FilterInfo filters[] = {
        {"ARTIST",    &plugin->filterSameArtist},
        {"SINGER",    &plugin->filterSameSinger},
        {"GENRE",     &plugin->filterSameGenre},
        {"LABEL",     &plugin->filterSameLabel},
        {"GROUPING",  &plugin->filterSameGrouping},
        {"ORCHESTRA", &plugin->filterSameOrchestra},
        {"TRACK",     &plugin->filterSameTrack},
    };

    int btnW = 80, btnH = 22, btnGap = 4;
    int bx = lx + 6;
    int filterIdx = 0;
    for (auto& f : filters)
    {
        TTColor bg = *f.value ? TCol::filterActive : TCol::buttonBg;
        TTColor fg = *f.value ? TCol::textBright : TCol::textDim;

        NSRect btnRect = NSMakeRect (bx, ly, btnW, btnH);
        NSBezierPath* rr = [NSBezierPath bezierPathWithRoundedRect:btnRect xRadius:4 yRadius:4];
        [ttNSColor (bg) setFill];
        [rr fill];

        cgDrawText (ctx, [NSString stringWithUTF8String:f.label],
                    cgR (bx, ly, btnW, btnH), fontSm, fg, NSTextAlignmentCenter);

        filterRects[filterIdx++] = cgR (bx, ly, btnW, btnH);

        bx += btnW + btnGap;
        if (bx + btnW > settingsL - PAD)
        {
            bx = lx + 6;
            ly += btnH + btnGap;
        }
    }

    ly += btnH + 12;

    // Year toggle + spinner
    int yearBtnW = 100;
    NSString* yearLabel = plugin->filterUseYearRange
        ? [NSString stringWithFormat:@"YEAR: \u00B1%d YRS", plugin->yearRange]
        : @"YEAR: OFF";
    TTColor yearBg = plugin->filterUseYearRange ? TCol::filterActive : TCol::buttonBg;
    TTColor yearFg = plugin->filterUseYearRange ? TCol::textBright : TCol::textDim;

    NSBezierPath* yearPill = [NSBezierPath bezierPathWithRoundedRect:
        NSMakeRect (lx + 6, ly, yearBtnW, btnH) xRadius:4 yRadius:4];
    [ttNSColor (yearBg) setFill];
    [yearPill fill];
    cgDrawText (ctx, yearLabel, cgR (lx + 6, ly, yearBtnW, btnH), fontSm, yearFg, NSTextAlignmentCenter);
    yearToggleRect = cgR (lx + 6, ly, yearBtnW, btnH);

    // - button
    int spinX = lx + 6 + yearBtnW + 6;
    NSBezierPath* minusPill = [NSBezierPath bezierPathWithRoundedRect:
        NSMakeRect (spinX, ly, 28, btnH) xRadius:4 yRadius:4];
    [ttNSColor (TCol::buttonBg) setFill];
    [minusPill fill];
    cgDrawText (ctx, @"\u2212", cgR (spinX, ly, 28, btnH), fontSm, TCol::textBright, NSTextAlignmentCenter);
    yearMinusRect = cgR (spinX, ly, 28, btnH);

    // + button
    spinX += 32;
    NSBezierPath* plusPill = [NSBezierPath bezierPathWithRoundedRect:
        NSMakeRect (spinX, ly, 28, btnH) xRadius:4 yRadius:4];
    [ttNSColor (TCol::buttonBg) setFill];
    [plusPill fill];
    cgDrawText (ctx, @"+", cgR (spinX, ly, 28, btnH), fontSm, TCol::textBright, NSTextAlignmentCenter);
    yearPlusRect = cgR (spinX, ly, 28, btnH);

    // Right column: HOW IT WORKS
    int ry = TOP_H + TOP_GAP + 8;
    cgDrawText (ctx, @"HOW IT WORKS", cgR (rx, ry, rw, 18), fontBold, TCol::textBright);
    ry += 24;

    struct HowSection { const char* title; std::vector<const char*> lines; };
    HowSection sections[] = {
        {"TRACK", {"Browse a track in VDJ. Add or update search with inputs.",
                   "Matches to database. Top 3 presented, with first selected."}},
        {"TANDA MATCHES", {"Similar tracks presented based on filters.",
                           "Customize filters in settings tab.",
                           "Press enter or click to search your VDJ library."}},
        {"BROWSER RESULTS", {"Top 4 browser results presented.",
                             "Hover over for additional info.",
                             "ADD \u2192 automix  \u00B7  Right-click ADD \u2192 sidelist."}},
        {"FILTERS", {"ARTIST / SINGER filter based on bandleader and singer.",
                     "GENRE matches to Tango, Vals, Milonga.",
                     "YEAR limits matches to \u00B1N years of the pick.",
                     "LABEL \u2192 Record label, GROUPING \u2192 Similar era.",
                     "ORCHESTRA \u2192 orchestra name, TRACK \u2192 track name."}},
    };

    for (auto& sec : sections)
    {
        cgDrawText (ctx, [NSString stringWithUTF8String:sec.title],
                    cgR (rx, ry, rw, 14), fontSm, TCol::textDim);
        ry += 16;
        for (auto& line : sec.lines)
        {
            cgDrawText (ctx, [NSString stringWithUTF8String:line],
                        cgR (rx + 6, ry, rw - 6, 14), fontSm, TCol::textNormal);
            ry += 14;
        }
        ry += 4;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Events
// ─────────────────────────────────────────────────────────────────────────────

- (void)mouseDown:(NSEvent*)event
{
    NSPoint loc = [self convertPoint:[event locationInWindow] fromView:nil];
    CGPoint pt = CGPointMake (loc.x, loc.y);

    // Settings toggle (top bar)
    if (CGRectContainsPoint (settingsToggleRect, pt))
    {
        plugin->activeTab = (plugin->activeTab == 1) ? 0 : 1;
        plugin->saveSettings();
        [self layoutUI];
        return;
    }

    // Settings tab filter/year button clicks
    if (plugin->activeTab == 1)
    {
        bool* filterPtrs[] = {
            &plugin->filterSameArtist, &plugin->filterSameSinger,
            &plugin->filterSameGenre, &plugin->filterSameLabel,
            &plugin->filterSameGrouping, &plugin->filterSameOrchestra,
            &plugin->filterSameTrack
        };
        for (int i = 0; i < 7; ++i)
        {
            if (CGRectContainsPoint (filterRects[i], pt))
            {
                *filterPtrs[i] = !(*filterPtrs[i]);
                plugin->saveSettings();
                plugin->runTandaSearch();
                [self setNeedsDisplay:YES];
                return;
            }
        }

        if (CGRectContainsPoint (yearToggleRect, pt))
        {
            plugin->filterUseYearRange = !plugin->filterUseYearRange;
            plugin->saveSettings();
            plugin->runTandaSearch();
            [self setNeedsDisplay:YES];
            return;
        }
        if (CGRectContainsPoint (yearMinusRect, pt))
        {
            if (plugin->yearRange > 1) plugin->yearRange--;
            plugin->saveSettings();
            plugin->runTandaSearch();
            [self setNeedsDisplay:YES];
            return;
        }
        if (CGRectContainsPoint (yearPlusRect, pt))
        {
            if (plugin->yearRange < 20) plugin->yearRange++;
            plugin->saveSettings();
            plugin->runTandaSearch();
            [self setNeedsDisplay:YES];
            return;
        }
    }

    // ADD button click (right column, bottom)
    if (plugin->activeTab == 0 && !plugin->browseItems.empty())
    {
        int rightW = DLG_W - DLG_W * LEFT_COL_PCT / 100;
        int rx = DLG_W * LEFT_COL_PCT / 100 + 2;
        int rw = rightW - PAD - 2;
        int bannerShift = plugin->metadataLoadFailed ? META_BANNER_H : 0;
        int ry = TOP_H + bannerShift;
        int browseTop = ry + DETAIL_BOX_H + BROWSE_HEADER_H;
        int browseH = BROWSE_ITEM_H * 4 + 2;
        int preRowY = browseTop + browseH + PRELISTEN_TOP_GAP;
        int addX = rx + rw - 72;

        if (loc.x >= addX && loc.x <= addX + 72
            && loc.y >= preRowY && loc.y <= preRowY + BTN_H)
        {
            if (event.clickCount == 1)
                plugin->addSelectedBrowseToAutomix();
            return;
        }
    }

    // Metadata banner click
    if (plugin->metadataLoadFailed)
    {
        int by = TOP_H + TOP_GAP;
        if (loc.y >= by && loc.y <= by + META_BANNER_H)
        {
            plugin->loadMetadata();
            [self setNeedsDisplay:YES];
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Window delegate — notify VDJ when window closes (#8)
// ─────────────────────────────────────────────────────────────────────────────

- (void)windowWillClose:(NSNotification*)notification
{
    if (plugin)
        plugin->dialogRequestedOpen = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Text field delegate (search debounce)
// ─────────────────────────────────────────────────────────────────────────────

- (void)controlTextDidChange:(NSNotification*)notification
{
    if (plugin->suppressEditChange) return;

    [searchDebounceTimer invalidate];
    searchDebounceTimer = [NSTimer scheduledTimerWithTimeInterval:0.3
        target:self selector:@selector(searchDebouncefire:) userInfo:nil repeats:NO];
}

- (void)searchDebouncefire:(NSTimer*)timer
{
    if (plugin->smartSearchPending)
    {
        // Reschedule
        searchDebounceTimer = [NSTimer scheduledTimerWithTimeInterval:0.2
            target:self selector:@selector(searchDebouncefire:) userInfo:nil repeats:NO];
        return;
    }

    std::wstring title = fromNS (editTitle.stringValue);
    std::wstring artist = fromNS (editArtist.stringValue);
    if (!title.empty())
        plugin->runIdentification (title, artist);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Browse poll timer
// ─────────────────────────────────────────────────────────────────────────────

- (void)browsePollTick:(NSTimer*)timer
{
    if (!plugin || plugin->activeTab != 0) return;
    if (plugin->smartSearchPending) return;
    if (plugin->metadataLoading) return;

    std::wstring folder = plugin->vdjGetString ("get_browsed_folder_path");
    if (folder.empty()) return;

    std::wstring title  = plugin->vdjGetString ("get_browsed_song 'title'");
    std::wstring artist = plugin->vdjGetString ("get_browsed_song 'artist'");

    if (title == plugin->lastSeenTitle && artist == plugin->lastSeenArtist)
        return;

    plugin->lastSeenTitle  = title;
    plugin->lastSeenArtist = artist;

    // Update edit fields
    plugin->suppressEditChange = true;
    editTitle.stringValue  = toNS (title);
    editArtist.stringValue = toNS (artist);
    std::wstring year = plugin->vdjGetString ("get_browsed_song 'year'");
    editYear.stringValue = toNS (year);
    plugin->suppressEditChange = false;

    if (!title.empty())
        plugin->runIdentification (title, artist);

    [self setNeedsDisplay:YES];
    [candList setNeedsDisplay:YES];
    [resultsList setNeedsDisplay:YES];
    [browseList setNeedsDisplay:YES];
}

@end

// ─────────────────────────────────────────────────────────────────────────────
//  Window creation / destruction
// ─────────────────────────────────────────────────────────────────────────────

void createMacUI (TigerTandaPlugin* p, void* vdjWindow)
{
    @autoreleasepool {
        NSRect frame = NSMakeRect (200, 200, DLG_W, DLG_H);

        NSPanel* panel = [[NSPanel alloc]
            initWithContentRect:frame
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskNonactivatingPanel
            backing:NSBackingStoreBuffered
            defer:NO];

        panel.title = @"TigerTanda";
        panel.level = NSFloatingWindowLevel;
        panel.hidesOnDeactivate = NO;
        panel.becomesKeyOnlyIfNeeded = YES;
        panel.titlebarAppearsTransparent = YES;
        panel.titleVisibility = NSWindowTitleHidden;
        panel.backgroundColor = ttNSColor (TCol::bg);

        TigerTandaMacUI* ui = [[TigerTandaMacUI alloc] initWithFrame:NSMakeRect(0, 0, DLG_W, DLG_H)
                                                              plugin:p];
        panel.contentView = ui;
        panel.delegate = ui;  // windowWillClose notification

        [panel center];
        [panel makeKeyAndOrderFront:nil];

        p->macUI = (__bridge_retained void*) panel;
    }
}

void destroyMacUI (TigerTandaPlugin* p)
{
    if (p->macUI)
    {
        @autoreleasepool {
            NSPanel* panel = (__bridge_transfer NSPanel*) p->macUI;
            [panel close];
            p->macUI = nullptr;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI wrapper implementations (macOS)
// ─────────────────────────────────────────────────────────────────────────────

void TigerTandaPlugin::uiResetCandidatesList()
{
    if (!macUI) return;
    @autoreleasepool {
        NSPanel* panel = (__bridge NSPanel*) macUI;
        TigerTandaMacUI* ui = (TigerTandaMacUI*) panel.contentView;
        [ui->candList setNeedsDisplay:YES];
    }
}

void TigerTandaPlugin::uiAddCandidateRow()
{
    // No-op on Mac — list views read from plugin state directly
}

void TigerTandaPlugin::uiInvalidateCandidates()
{
    if (!macUI) return;
    @autoreleasepool {
        NSPanel* panel = (__bridge NSPanel*) macUI;
        TigerTandaMacUI* ui = (TigerTandaMacUI*) panel.contentView;
        [ui->candList setNeedsDisplay:YES];
    }
}

void TigerTandaPlugin::uiResetResultsList()
{
    if (!macUI) return;
    @autoreleasepool {
        NSPanel* panel = (__bridge NSPanel*) macUI;
        TigerTandaMacUI* ui = (TigerTandaMacUI*) panel.contentView;
        [ui->resultsList setNeedsDisplay:YES];
    }
}

void TigerTandaPlugin::uiAddResultRow()
{
    // No-op on Mac
}

void TigerTandaPlugin::uiResetBrowseList()
{
    if (!macUI) return;
    @autoreleasepool {
        NSPanel* panel = (__bridge NSPanel*) macUI;
        TigerTandaMacUI* ui = (TigerTandaMacUI*) panel.contentView;
        [ui->browseList setNeedsDisplay:YES];
        [ui layoutUI];
    }
}

void TigerTandaPlugin::uiAddBrowseRow()
{
    // No-op on Mac
}

void TigerTandaPlugin::uiInvalidateDialog()
{
    if (!macUI) return;
    @autoreleasepool {
        NSPanel* panel = (__bridge NSPanel*) macUI;
        TigerTandaMacUI* ui = (TigerTandaMacUI*) panel.contentView;
        [ui setNeedsDisplay:YES];
        [ui->candList setNeedsDisplay:YES];
        [ui->resultsList setNeedsDisplay:YES];
        [ui->browseList setNeedsDisplay:YES];
    }
}

void TigerTandaPlugin::uiSetTimer (int timerId, int ms)
{
    if (!macUI) return;
    @autoreleasepool {
        NSPanel* panel = (__bridge NSPanel*) macUI;
        TigerTandaMacUI* ui = (TigerTandaMacUI*) panel.contentView;

        // Use dispatch_after for one-shot timers
        double seconds = ms / 1000.0;
        dispatch_after (dispatch_time (DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                        dispatch_get_main_queue(), ^{
            if (timerId == TT_TIMER_SMART_SEARCH)
                this->runSmartSearch();
            else if (timerId == TT_TIMER_MATCH_SELECT)
            {
                if (selectedResultIdx >= 0 && selectedResultIdx < (int) results.size()
                    && !smartSearchPending)
                {
                    lastSmartSearchTitle.clear();
                    lastSmartSearchArtist.clear();
                    triggerBrowserSearch (results[selectedResultIdx]);
                }
            }
        });
    }
}

void TigerTandaPlugin::uiKillTimer (int timerId)
{
    // dispatch_after timers can't be cancelled; they check state at fire time
}

void TigerTandaPlugin::uiSetEditText (int ctrlId, const std::wstring& text)
{
    if (!macUI) return;
    @autoreleasepool {
        NSPanel* panel = (__bridge NSPanel*) macUI;
        TigerTandaMacUI* ui = (TigerTandaMacUI*) panel.contentView;
        NSString* ns = toNS (text);
        switch (ctrlId)
        {
            case IDC_EDIT_TITLE:  ui->editTitle.stringValue = ns;  break;
            case IDC_EDIT_ARTIST: ui->editArtist.stringValue = ns; break;
            case IDC_EDIT_YEAR:   ui->editYear.stringValue = ns;   break;
        }
    }
}
