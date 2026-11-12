#include "BrowserPanels.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace pflow {

namespace {

// Levels mockup library metrics (light compare 2026-11-11).
constexpr int kSidebarHeaderH = 24;   // panel header band
constexpr int kSidebarSectionH = 22;  // section caps row
/** Same height as file-browser clip rows (metrics::listRowH). */
constexpr int kSidebarRowH = ds::region::listRowH;
constexpr int kOpenFolderH = 32;      // mockup secondary button height
constexpr int kCardHeaderH = 22;
constexpr int kSectionGap = 16;       // air between LIBRARY / FAVORITES blocks
constexpr int kHeaderToRows = 6;      // caps → first row
constexpr int kBodyPadT = 10;
constexpr int kBodyPadB = 10;
constexpr int kPadH = 10;             // sidebar horizontal gutter
constexpr int kRowPadX = 10;
constexpr int kIconCol = 14;
constexpr int kRowIconGap = 8;
constexpr int kOpenToLibrary = 18;    // buffer under Open Folder before LIBRARY
constexpr float kSectionTitlePt = 12.0f;
constexpr float kSectionLabelPt = 11.0f;
constexpr float kCtrlPt = 12.0f;
constexpr float kSidebarIconStroke = 1.35f;
constexpr float kSelectDot = 6.0f;

/** Fold lives on the Open Folder row — no duplicate sidebar header band. */
inline int sidebarHeaderH() { return 0; }
inline int sidebarSectionH() { return metrics::scaled(kSidebarSectionH); }
inline int sidebarRowH() { return metrics::scaled(kSidebarRowH); }
inline int sidebarSectionGap() { return metrics::scaled(kSectionGap); }
inline int sidebarHeaderToRows() { return metrics::scaled(kHeaderToRows); }
inline int sidebarBodyPadT() { return metrics::scaled(kBodyPadT); }
inline int sidebarPadH() { return metrics::scaled(kPadH); }
inline int sidebarRowPadX() { return metrics::scaled(kRowPadX); }
inline int sidebarIconCol() { return metrics::scaled(kIconCol); }
inline int sidebarRowIconGap() { return metrics::scaled(kRowIconGap); }
/** Folded rail icons match the Files-row sidebar fold toggle. */
inline int collapsedBtnSize() { return metrics::chromeIconButton(); }
inline int openFolderH() { return metrics::scaled(kOpenFolderH); }

struct SourceListPalette
{
    juce::Colour tint, card, control, text, text2, text3, accent, selectWash, border, gold, shadow;
};

inline SourceListPalette sl()
{
    // Groove Panel tokens.
    return {
        ds::surface100(),   // tint
        ds::surface100(),   // card
        ds::surface200(),   // control
        ds::ink(),          // text
        ds::inkMuted(),     // text2
        ds::inkMuted(),     // text3
        ds::acc(),          // accent
        ds::selsoft(),      // selectWash (selected row)
        ds::edge(),         // border
        ds::warning(),      // gold (favorite star)
        juce::Colours::transparentBlack,
    };
}

inline void fillControlSurface(juce::Graphics& g, juce::Rectangle<float> r, float radius,
                                bool drawShadow = true)
{
    // Groove toggle face: surface-200, edge border, radius-sm, no shadow.
    juce::ignoreUnused(radius, drawShadow);
    const auto p = sl();
    g.setColour(p.control);
    g.fillRoundedRectangle(r, ds::radius::sm);
    g.setColour(p.border);
    g.drawRoundedRectangle(r.reduced(0.5f), ds::radius::sm, 1.0f);
}

} // namespace

// ── FilterPanel ──────────────────────────────────────────────────────────────

FavoritesSidebar::FilterPanel::FilterPanel()
{
    auto wireCollapse = [this](auto& section)
    {
        section.onCollapsedChanged = [this]
        {
            resized();
            if (onLayoutChanged) onLayoutChanged();
        };
    };

    juce::StringArray keys;
    for (int i = 0; i < 12; ++i)
        keys.add(kNoteNames[(size_t) i]);
    keyGrid.title = "Key";
    keyGrid.columns = 6;
    keyGrid.multiSelect = true;
    keyGrid.sidebarLabelStyle = true;
    keyGrid.selectStyle = fx::ChipGrid::SelectStyle::SolidPrimary;
    keyGrid.collapsible = true;
    keyGrid.collapsed = false;
    wireCollapse(keyGrid);
    keyGrid.setItems(keys);
    keyGrid.onChange = [this]
    {
        if (onCriteriaChanged) onCriteriaChanged();
    };
    addAndMakeVisible(keyGrid);

    juce::StringArray kinds;
    for (int i = 0; i < kNumClipKinds; ++i)
        kinds.add(clipKindName((ClipKind) i));
    kindGrid.title = "Kind";
    kindGrid.columns = kNumClipKinds;   // one row: Bass · Keys · Lead · Arp · Drums
    kindGrid.multiSelect = true;
    kindGrid.sidebarLabelStyle = true;
    kindGrid.selectStyle = fx::ChipGrid::SelectStyle::Accent;
    kindGrid.countSummary = true;
    kindGrid.collapsible = true;
    kindGrid.collapsed = false;
    wireCollapse(kindGrid);
    kindGrid.setItems(kinds);
    // All kinds visible by default — user toggles off ones to hide.
    kindGrid.setSelectedMask((uint16_t) ((1u << kNumClipKinds) - 1), juce::dontSendNotification);
    kindGrid.onChange = [this]
    {
        if (onCriteriaChanged) onCriteriaChanged();
    };
    addAndMakeVisible(kindGrid);

    // Tags removed from the filter UI — keep tagGrid offscreen/hidden for API stability.
    tagGrid.setVisible(false);

    juce::StringArray timeSigs;
    for (int i = 0; i < kNumFilterTimeSigs; ++i)
        timeSigs.add(kFilterTimeSigs[i].label);
    timeSigGrid.title = "Time Signature";
    timeSigGrid.columns = 4;
    timeSigGrid.multiSelect = true;
    timeSigGrid.sidebarLabelStyle = true;
    timeSigGrid.collapsible = true;
    timeSigGrid.collapsed = true;
    wireCollapse(timeSigGrid);
    timeSigGrid.setItems(timeSigs);
    timeSigGrid.onChange = [this]
    {
        if (onCriteriaChanged) onCriteriaChanged();
    };
    addAndMakeVisible(timeSigGrid);

    for (auto* range : { &bpmRange, &barsRange, &complexityRange })
    {
        range->sidebarLabelStyle = true;
        range->collapsible = true;
        range->collapsed = true;
        wireCollapse(*range);
        range->onChange = [this](int, int)
        {
            if (onCriteriaChanged) onCriteriaChanged();
        };
        addAndMakeVisible(*range);
    }
    bpmRange.collapsed = false; // Tempo open by default
    bpmRange.setTooltip("Filter by tempo (BPM)");
    barsRange.setTooltip("Filter by length in bars");
    complexityRange.setTooltip("Filter by complexity score");

    sortPopup.setItems({ "Name", "BPM", "Key", "Bars", "Complexity", "DifNotes", "TimeSig", "Notes" }, 0);
    sortPopup.setTooltip("Sort file list");
    sortPopup.onChange = [this](int idx)
    {
        static constexpr BrowserSortColumn kCols[] = {
            BrowserSortColumn::Name,
            BrowserSortColumn::Tempo,
            BrowserSortColumn::Key,
            BrowserSortColumn::Bars,
            BrowserSortColumn::Complexity,
            BrowserSortColumn::DifNotes,
            BrowserSortColumn::TimeSig,
            BrowserSortColumn::Notes,
        };
        sortColumn = kCols[juce::jlimit(0, (int) (sizeof(kCols) / sizeof(kCols[0])) - 1, idx)];
        if (onSortChanged) onSortChanged(sortColumn);
    };
    addAndMakeVisible(sortPopup);
}

void FavoritesSidebar::FilterPanel::setSortColumn(BrowserSortColumn col)
{
    sortColumn = col;
    int idx = 0;
    switch (col)
    {
        case BrowserSortColumn::Name:       idx = 0; break;
        case BrowserSortColumn::Tempo:      idx = 1; break;
        case BrowserSortColumn::Key:        idx = 2; break;
        case BrowserSortColumn::Bars:       idx = 3; break;
        case BrowserSortColumn::Complexity: idx = 4; break;
        case BrowserSortColumn::DifNotes:   idx = 5; break;
        case BrowserSortColumn::TimeSig:    idx = 6; break;
        case BrowserSortColumn::Notes:      idx = 7; break;
        case BrowserSortColumn::Kind:       idx = 0; break;
    }
    sortPopup.setIndex(idx, juce::dontSendNotification);
}

void FavoritesSidebar::FilterPanel::lookAndFeelChanged()
{
    repaint();
}

int FavoritesSidebar::FilterPanel::idealHeight() const
{
    auto gapAfter = [](bool sectionOpen)
    {
        return metrics::scaled(sectionOpen ? 16 : 2);
    };
    // Include Subdirs + Hide Dupes first, then fold sections, then Sort.
    const int toggleBlock = metrics::scaled(22) * 2 + metrics::scaled(4) + metrics::scaled(6)
                          + metrics::scaled(26); // Sort by row
    return toggleBlock
         + keyGrid.idealHeight() + gapAfter(!keyGrid.collapsed)
         + kindGrid.idealHeight() + gapAfter(!kindGrid.collapsed)
         + bpmRange.idealHeight() + gapAfter(!bpmRange.collapsed)
         + barsRange.idealHeight() + gapAfter(!barsRange.collapsed)
         + complexityRange.idealHeight() + gapAfter(!complexityRange.collapsed)
         + timeSigGrid.idealHeight() + gapAfter(!timeSigGrid.collapsed)
         + 8;
}

void FavoritesSidebar::FilterPanel::setIncludeSubdirs(bool on)
{
    if (includeSubdirs == on)
        return;
    includeSubdirs = on;
    repaint();
}

void FavoritesSidebar::FilterPanel::setScanning(bool on)
{
    if (scanning == on)
        return;
    scanning = on;
    repaint();
}

void FavoritesSidebar::FilterPanel::applyTo(BrowserSearch& s) const
{
    const int bpmLo = bpmRange.getLo();
    const int bpmHi = bpmRange.getHi();
    if (!bpmRange.isFullRange())
    {
        s.bpmMin = (double) bpmLo;
        s.bpmMax = (double) bpmHi;
    }
    const int barsLo = barsRange.getLo();
    const int barsHi = barsRange.getHi();
    if (!barsRange.isFullRange())
    {
        s.barsMin = barsLo;
        s.barsMax = barsHi;
    }
    if (!complexityRange.isFullRange())
    {
        s.complexityMin = complexityRange.getLo();
        s.complexityMax = complexityRange.getHi();
    }
    s.keyMask = keyGrid.getSelectedMask();
    s.keyRoot = -1;
    if (s.keyMask != 0)
    {
        // Prefer a single-bit keyRoot for legacy saved-search labels.
        for (int i = 0; i < 12; ++i)
            if ((s.keyMask & (uint16_t) (1u << i)) != 0)
            {
                s.keyRoot = i;
                break;
            }
        int bits = 0;
        for (int i = 0; i < 12; ++i)
            if ((s.keyMask & (uint16_t) (1u << i)) != 0) ++bits;
        if (bits > 1)
            s.keyRoot = -1;
    }
    {
        const auto mask = (uint8_t) kindGrid.getSelectedMask();
        const uint8_t all = (uint8_t) ((1u << kNumClipKinds) - 1);
        // All selected ≡ no kind filter (store 0 for legacy "any").
        s.kindMask = (mask == all) ? 0 : mask;
    }
    s.tagMask = 0; // Tags filter removed from UI
    s.timeSigMask = timeSigGrid.getSelectedMask();
    s.removeDuplicates = hideDuplicates;
}

int FavoritesSidebar::FilterPanel::activeGroupCount() const
{
    int n = 0;
    if (keyGrid.getSelectedMask() != 0) ++n;
    {
        const uint8_t all = (uint8_t) ((1u << kNumClipKinds) - 1);
        if ((uint8_t) kindGrid.getSelectedMask() != all) ++n;
    }
    if (timeSigGrid.getSelectedMask() != 0) ++n;
    if (!bpmRange.isFullRange()) ++n;
    if (!barsRange.isFullRange()) ++n;
    if (!complexityRange.isFullRange()) ++n;
    if (!hideDuplicates) ++n;
    return n;
}

juce::String FavoritesSidebar::FilterPanel::describeActiveFilters() const
{
    juce::StringArray parts;
    if (keyGrid.getSelectedMask() != 0)
    {
        juce::StringArray keys;
        for (int i = 0; i < 12; ++i)
            if ((keyGrid.getSelectedMask() & (uint16_t) (1u << i)) != 0)
                keys.add(kNoteNames[(size_t) i]);
        parts.add("Key: " + keys.joinIntoString(", "));
    }
    {
        const uint8_t all = (uint8_t) ((1u << kNumClipKinds) - 1);
        const auto mask = (uint8_t) kindGrid.getSelectedMask();
        if (mask != all)
        {
            juce::StringArray kinds;
            for (int i = 0; i < kNumClipKinds; ++i)
                if ((mask & (uint8_t) (1u << i)) != 0)
                    kinds.add(clipKindName((ClipKind) i));
            parts.add("Kind: " + (kinds.isEmpty() ? juce::String("none") : kinds.joinIntoString(", ")));
        }
    }
    if (timeSigGrid.getSelectedMask() != 0)
    {
        juce::StringArray sigs;
        for (int i = 0; i < kNumFilterTimeSigs; ++i)
            if ((timeSigGrid.getSelectedMask() & (uint16_t) (1u << i)) != 0)
                sigs.add(kFilterTimeSigs[i].label);
        parts.add("Time Signature: " + sigs.joinIntoString(", "));
    }
    if (!bpmRange.isFullRange())
        parts.add("Tempo: " + juce::String(bpmRange.getLo()) + "–" + juce::String(bpmRange.getHi()));
    if (!barsRange.isFullRange())
        parts.add("Bars: " + juce::String(barsRange.getLo()) + "–" + juce::String(barsRange.getHi()));
    if (!complexityRange.isFullRange())
        parts.add("Complexity: " + juce::String(complexityRange.getLo())
                  + "–" + juce::String(complexityRange.getHi()));
    if (!hideDuplicates)
        parts.add("Showing duplicates");
    if (includeSubdirs)
        parts.add("Include subdirectories");
    if (parts.isEmpty())
        return "Filters";
    return "Filters\n" + parts.joinIntoString("\n");
}

void FavoritesSidebar::FilterPanel::loadFrom(const BrowserSearch& s)
{
    if (s.bpmMin <= 0.0 && s.bpmMax <= 0.0)
        bpmRange.setRange(40, 240, juce::dontSendNotification);
    else
        bpmRange.setRange(s.bpmMin > 0.0 ? (int) std::lround(s.bpmMin) : 40,
                          s.bpmMax > 0.0 ? (int) std::lround(s.bpmMax) : 240,
                          juce::dontSendNotification);
    if (s.barsMin <= 0 && s.barsMax <= 0)
        barsRange.setRange(1, 64, juce::dontSendNotification);
    else
        barsRange.setRange(s.barsMin > 0 ? s.barsMin : 1,
                           s.barsMax > 0 ? s.barsMax : 64,
                           juce::dontSendNotification);
    if (s.complexityMin <= 0 && s.complexityMax <= 0)
        complexityRange.setRange(0, 100, juce::dontSendNotification);
    else
        complexityRange.setRange(s.complexityMin > 0 ? s.complexityMin : 0,
                                 s.complexityMax > 0 ? s.complexityMax : 100,
                                 juce::dontSendNotification);

    uint16_t mask = s.keyMask;
    if (mask == 0 && s.keyRoot >= 0 && s.keyRoot < 12)
        mask = (uint16_t) (1u << s.keyRoot);
    keyGrid.setSelectedMask(mask, juce::dontSendNotification);
    {
        const uint8_t all = (uint8_t) ((1u << kNumClipKinds) - 1);
        kindGrid.setSelectedMask(s.kindMask == 0 ? all : s.kindMask, juce::dontSendNotification);
    }
    tagGrid.setSelectedMask(0, juce::dontSendNotification);
    timeSigGrid.setSelectedMask(s.timeSigMask, juce::dontSendNotification);
    hideDuplicates = s.removeDuplicates;
    repaint();
}

void FavoritesSidebar::FilterPanel::resetFilters()
{
    bpmRange.setRange(40, 240, juce::dontSendNotification);
    barsRange.setRange(1, 64, juce::dontSendNotification);
    complexityRange.setRange(0, 100, juce::dontSendNotification);
    keyGrid.setSelectedMask(0, juce::dontSendNotification);
    kindGrid.setSelectedMask((uint16_t) ((1u << kNumClipKinds) - 1), juce::dontSendNotification);
    timeSigGrid.setSelectedMask(0, juce::dontSendNotification);
    hideDuplicates = true;
    repaint();
    if (onCriteriaChanged)
        onCriteriaChanged();
}

void FavoritesSidebar::FilterPanel::setHistograms(const std::vector<StepClip>& clips)
{
    std::vector<int> bpm, bars, cx;
    // A few thousand samples is enough for the histogram shape.
    const int step = juce::jmax(1, (int) clips.size() / 4000);
    bpm.reserve(clips.size() / (size_t) step + 1);
    bars.reserve(clips.size() / (size_t) step + 1);
    cx.reserve(clips.size() / (size_t) step + 1);
    for (size_t i = 0; i < clips.size(); i += (size_t) step)
    {
        const auto& c = clips[i];
        if (c.bpm > 0.0)
            bpm.push_back(juce::jlimit(40, 240, (int) std::lround(c.bpm)));
        if (c.bars > 0)
            bars.push_back(juce::jlimit(1, 64, c.bars));
        if (c.complexity > 0)
            cx.push_back(juce::jlimit(0, 100, c.complexity));
    }
    bpmRange.setHistogramFromSamples(bpm, 16);
    barsRange.setHistogramFromSamples(bars, 16);
    complexityRange.setHistogramFromSamples(cx, 16);
}

void FavoritesSidebar::FilterPanel::paint(juce::Graphics& g)
{
    const auto& t = inspectorTokens();
    auto paintToggleRow = [&](juce::Rectangle<int> row, const juce::String& label, bool on,
                              juce::Rectangle<int> switchBounds)
    {
        if (row.isEmpty()) return;
        g.setFont(uiFontFixed(kCtrlPt));
        g.setColour(scanning ? ds::tx3() : ds::tx());
        auto labelR = row;
        if (!switchBounds.isEmpty())
            labelR.setRight(switchBounds.getX() - 8);
        g.drawText(label, labelR, juce::Justification::centredLeft, true);

        auto track = switchBounds.toFloat();
        if (track.isEmpty()) return;
        const float radius = track.getHeight() * 0.5f;
        // Levels switch: violet track when on, edge when off — clip to avoid white corner bleed.
        juce::ignoreUnused(t);
        juce::Path trackPath;
        trackPath.addRoundedRectangle(track, radius);
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(trackPath);
            g.setColour(on ? ds::acc() : ds::edge());
            g.fillRect(track);
            const float knob = juce::jmax(8.0f, track.getHeight() - 4.0f);
            const float kx = on ? track.getRight() - knob - 2.0f : track.getX() + 2.0f;
            g.setColour(ds::onPrimary());
            g.fillEllipse(kx, track.getCentreY() - knob * 0.5f, knob, knob);
        }
    };

    paintToggleRow(subdirRowBounds, "Include Subdirectories", includeSubdirs, subdirSwitchBounds);

    if (!hideDupesBounds.isEmpty())
    {
        const int switchW = metrics::scaled(34);
        const int switchH = metrics::scaled(18);
        auto sw = hideDupesBounds.withWidth(switchW)
                      .withX(hideDupesBounds.getRight() - switchW)
                      .withSizeKeepingCentre(switchW, switchH);
        paintToggleRow(hideDupesBounds, "Hide Duplicates", hideDuplicates, sw);
    }

    if (!sortRowBounds.isEmpty())
    {
        g.setFont(uiFontFixed(kCtrlPt));
        g.setColour(ds::tx());
        auto labelR = sortRowBounds;
        labelR.setRight(sortPopup.getX() - 8);
        g.drawText("Sort by", labelR, juce::Justification::centredLeft, true);
    }
}

void FavoritesSidebar::FilterPanel::mouseDown(const juce::MouseEvent& e)
{
    if (!scanning && subdirRowBounds.contains(e.getPosition()))
    {
        includeSubdirs = !includeSubdirs;
        repaint();
        if (onSubdirsChanged)
            onSubdirsChanged(includeSubdirs);
        return;
    }
    if (hideDupesBounds.contains(e.getPosition()))
    {
        hideDuplicates = !hideDuplicates;
        repaint();
        if (onCriteriaChanged)
            onCriteriaChanged();
        return;
    }
    if (resetBounds.contains(e.getPosition()))
        resetFilters();
}

void FavoritesSidebar::FilterPanel::resized()
{
    auto r = getLocalBounds();
    // Levels FilterPopover: 16 px between open sections; tight 2 px when folded.
    auto gapAfter = [](bool sectionOpen)
    {
        return metrics::scaled(sectionOpen ? 16 : 2);
    };

    const int toggleRowH = metrics::scaled(22);
    const int switchW = metrics::scaled(34);
    const int switchH = juce::jmin(metrics::scaled(18), toggleRowH - 2);
    resetBounds = {}; // Reset lives in the FilterPopover header.
    tagGrid.setBounds({});

    // Include Subdirectories + Hide Duplicates sit above Key.
    {
        auto row = r.removeFromTop(toggleRowH);
        subdirRowBounds = row;
        subdirSwitchBounds = row.removeFromRight(switchW)
                                 .withSizeKeepingCentre(switchW, switchH);
        r.removeFromTop(metrics::scaled(4));
    }
    {
        auto row = r.removeFromTop(toggleRowH);
        hideDupesBounds = row;
        r.removeFromTop(metrics::scaled(6));
    }
    {
        auto row = r.removeFromTop(metrics::scaled(26));
        sortRowBounds = row;
        const int popW = juce::jmin(juce::jmax(sortPopup.idealWidth(), 110), row.getWidth() / 2);
        sortPopup.setBounds(row.removeFromRight(popW).withSizeKeepingCentre(popW, metrics::scaled(22)));
        r.removeFromTop(metrics::scaled(8));
    }

    keyGrid.setBounds(r.removeFromTop(keyGrid.idealHeight()));
    r.removeFromTop(gapAfter(!keyGrid.collapsed));
    kindGrid.setBounds(r.removeFromTop(kindGrid.idealHeight()));
    r.removeFromTop(gapAfter(!kindGrid.collapsed));
    bpmRange.setBounds(r.removeFromTop(bpmRange.idealHeight()));
    r.removeFromTop(gapAfter(!bpmRange.collapsed));
    barsRange.setBounds(r.removeFromTop(barsRange.idealHeight()));
    r.removeFromTop(gapAfter(!barsRange.collapsed));
    complexityRange.setBounds(r.removeFromTop(complexityRange.idealHeight()));
    r.removeFromTop(gapAfter(!complexityRange.collapsed));
    timeSigGrid.setBounds(r.removeFromTop(timeSigGrid.idealHeight()));
}

// ── SearchQueryPanel ─────────────────────────────────────────────────────────

FavoritesSidebar::SearchQueryPanel::SearchQueryPanel()
{
    styleEditors();
    addAndMakeVisible(queryField);
    queryField.onTextChange = [this]
    {
        updateTrailingVisible();
        repaint();
    };
    queryField.onFocused = [this]
    {
        if (onActivate) onActivate();
    };
    queryField.onEscape = [this]
    {
        if (onDeactivate) onDeactivate();
    };
    queryField.onFocusChanged = [this] { repaint(); };
    queryField.onReturnKey = [this]
    {
        if (onReturn) onReturn();
    };
    queryField.setComponentID("searchQuery");

    clearBtn.ghost = true;
    clearBtn.iconScale = 0.85f;
    clearBtn.onClick = [this]
    {
        queryField.clear();
        updateTrailingVisible();
        if (onClear) onClear();
    };
    addChildComponent(clearBtn);

    updateTrailingVisible();
}

void FavoritesSidebar::SearchQueryPanel::styleEditors()
{
    queryField.setTextToShowWhenEmpty("Search", ds::inkMuted());
    queryField.setFont(uiFontFixed(12.0f));
    queryField.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::textColourId, ds::ink());
    queryField.setColour(juce::TextEditor::highlightedTextColourId, ds::onAccent());
    queryField.setColour(juce::TextEditor::highlightColourId, ds::acc().withAlpha(0.45f));
    queryField.setColour(juce::CaretComponent::caretColourId, ds::ink());
    // Existing text keeps its old colour unless re-applied (light/dark switch).
    queryField.applyColourToAllText(ds::ink(), true);
    queryField.setIndents(0, 0);
}

void FavoritesSidebar::SearchQueryPanel::lookAndFeelChanged()
{
    styleEditors();
    repaint();
}

void FavoritesSidebar::SearchQueryPanel::updateTrailingVisible()
{
    saveVisible = queryField.getText().isNotEmpty();
    clearBtn.setVisible(false); // 3c: Save chip replaces clear; Escape clears via onDeactivate
    resized();
    repaint();
}

juce::String FavoritesSidebar::SearchQueryPanel::getQuery() const
{
    return queryField.getText().trim();
}

void FavoritesSidebar::SearchQueryPanel::setQuery(const juce::String& q)
{
    queryField.setText(q, juce::dontSendNotification);
    updateTrailingVisible();
}

void FavoritesSidebar::SearchQueryPanel::clearQuery()
{
    queryField.clear();
    updateTrailingVisible();
}

void FavoritesSidebar::SearchQueryPanel::focusQuery()
{
    queryField.grabKeyboardFocus();
}

void FavoritesSidebar::SearchQueryPanel::blurQuery()
{
    if (queryField.hasKeyboardFocus(true))
        queryField.giveAwayKeyboardFocus();
}

bool FavoritesSidebar::SearchQueryPanel::isQueryFocused() const
{
    return queryField.hasKeyboardFocus(true);
}

void FavoritesSidebar::SearchQueryPanel::setFilterActive(bool on)
{
    if (filterActive == on)
        return;
    filterActive = on;
    repaint();
}

void FavoritesSidebar::SearchQueryPanel::mouseDown(const juce::MouseEvent& e)
{
    if (filterCapBounds.contains(e.getPosition()))
    {
        if (onToggleFilter) onToggleFilter();
        return;
    }
    if (saveVisible && saveChipBounds.contains(e.getPosition()))
    {
        if (onSave) onSave();
        return;
    }
    if (!queryWell.contains(e.getPosition()))
        return;
    if (onActivate) onActivate();
    focusQuery();
}

void FavoritesSidebar::SearchQueryPanel::paint(juce::Graphics& g)
{
    // Groove field: surface-300, edge border, radius-sm; magnifier in ink-muted.
    const bool focused = queryField.hasKeyboardFocus(true);
    auto well = queryWell.toFloat();
    if (!well.isEmpty())
    {
        // Levels field: surface, edge border, radius-md, 2px primary focus ring.
        g.setColour(ds::surface200());
        g.fillRoundedRectangle(well, ds::radius::md);
        g.setColour(ds::edge());
        g.drawRoundedRectangle(well.reduced(0.5f), ds::radius::md, 1.0f);
        if (focused)
            drawFocusRing(g, well, ds::radius::md);
    }
    auto iconArea = juce::Rectangle<float>(well.getX() + 5.0f, well.getY(), 10.0f, well.getHeight())
                        .withSizeKeepingCentre(10.0f, 10.0f);
    drawIcon(g, icons::search, iconArea, ds::inkMuted(), 1.3f);

    if (saveVisible && !saveChipBounds.isEmpty())
    {
        g.setColour(ds::primary());
        g.fillRoundedRectangle(saveChipBounds.toFloat(), ds::radius::sm);
        g.setColour(ds::onPrimary());
        g.setFont(uiFontFixed(11.0f, true));
        g.drawText("Save", saveChipBounds, juce::Justification::centred, false);
    }
}

void FavoritesSidebar::SearchQueryPanel::resized()
{
    auto querySlot = getLocalBounds();
    queryWell = querySlot;
    filterCapBounds = {};   // the browser toolbar's Filters toggle replaces the in-field cap

    saveChipBounds = {};
    if (saveVisible)
    {
        const int chipW = 40;
        saveChipBounds = querySlot.removeFromRight(chipW + 2).withTrimmedRight(2)
                             .withSizeKeepingCentre(chipW, juce::jmax(10, querySlot.getHeight() - 4));
    }
    auto field = querySlot.withTrimmedLeft(20);
    queryField.setBounds(field);
    // Vertically centre the text inside the field (JUCE TextEditor indents from the top).
    const float fontH = queryField.getFont().getHeight();
    const int top = juce::jmax(0, (int) std::round(((float) field.getHeight() - fontH) * 0.5f) - 1);
    queryField.setIndents(0, top);
}

// ── FavoritesSidebar (3c compact action row) ─────────────────────────────────

FavoritesSidebar::FavoritesSidebar()
{
    btnToggle.ghost = false;
    btnToggle.outlined = false; // Levels (LvApp): ghost fold icon beside Open Folder
    btnToggle.iconScale = 1.0f;
    btnToggle.setWantsKeyboardFocus(true);
    btnToggle.setTooltip("Collapse sidebar");
    btnToggle.onClick = [this] { setCollapsed(!isCollapsed()); };
    addAndMakeVisible(btnToggle);

    btnSettings.ghost = true;
    btnSettings.iconScale = 1.0f;
    btnSettings.setWantsKeyboardFocus(true);
    btnSettings.setTooltip("Settings");
    btnSettings.onClick = [this] { if (onOpenSettings) onOpenSettings(); };
    addChildComponent(btnSettings);   // Settings lives in the header now

    expandedWidth = metrics::sidebarExpandedWidth();
    setWantsKeyboardFocus(true);

    filterPanel.onCriteriaChanged = [this]
    {
        // While the popover is open, edits stay draft until Apply.
        if (!filterEditing)
            scheduleFilterApply();
        searchQuery.setFilterActive(filterOpen || activeFilterGroupCount() > 0);
        if (onFilterLayoutChanged) onFilterLayoutChanged();
        repaint();
    };
    filterPanel.onLayoutChanged = [this] { if (onFilterLayoutChanged) onFilterLayoutChanged(); };
    filterPanel.onSubdirsChanged = [this](bool on)
    {
        includeSubdirs = on;
        // Defer the rescan until Apply while the popover is open.
        if (!filterEditing && onScanSubdirs)
            onScanSubdirs();
    };
    filterPanel.onSortChanged = [this](BrowserSortColumn col)
    {
        if (onSortChanged) onSortChanged(col);
    };
    addChildComponent(filterPanel);

    searchQuery.onActivate = [this] { searchOpen = true; };
    searchQuery.onDeactivate = [this] { deactivateSearch(false); };
    searchQuery.onClear = [this]
    {
        searchQuery.clearQuery();
        deactivateSearch(true);
        if (onExitSearch) onExitSearch();
    };
    searchQuery.onSave = [this]
    {
        if (onSaveCurrentSearch) onSaveCurrentSearch();
    };
    searchQuery.onReturn = [this]
    {
        if (onRunSearch) onRunSearch(getCriteria());
    };
    searchQuery.onToggleFilter = [this] { setFilterOpen(!filterOpen); };
    addChildComponent(searchQuery);

    rebuildRows();
}

void FavoritesSidebar::notifyHeightChanged()
{
    layoutChildren();
    repaint();
    if (onContentHeightChanged) onContentHeightChanged();
}

juce::Component& FavoritesSidebar::searchComponent() { return searchQuery; }
juce::Component& FavoritesSidebar::filterComponent() { return filterPanel; }
int FavoritesSidebar::filterIdealHeight() const { return filterPanel.idealHeight(); }
int FavoritesSidebar::activeFilterCount() const { return filterPanel.activeGroupCount(); }
juce::String FavoritesSidebar::describeActiveFilters() const { return filterPanel.describeActiveFilters(); }
void FavoritesSidebar::resetFilters() { filterPanel.resetFilters(); }
void FavoritesSidebar::setSortColumn(BrowserSortColumn col) { filterPanel.setSortColumn(col); }
bool FavoritesSidebar::getHideDuplicates() const { return filterPanel.getHideDuplicates(); }
void FavoritesSidebar::toggleHideDuplicates() { filterPanel.toggleHideDuplicates(); }

void FavoritesSidebar::FilterPanel::setHideDuplicates(bool on)
{
    if (hideDuplicates == on) return;
    hideDuplicates = on;
    repaint();
    if (onCriteriaChanged) onCriteriaChanged();
}

void FavoritesSidebar::FilterPanel::toggleHideDuplicates()
{
    setHideDuplicates(!hideDuplicates);
}

void FavoritesSidebar::setFilterOpen(bool open)
{
    if (filterOpen == open)
        return;
    filterOpen = open;
    if (filterOpen)
    {
        filterDraftSnapshot = getFilter();
        filterEditing = true;
        filterPanel.setHistograms(histogramClips);
    }
    else
    {
        filterEditing = false;
    }
    searchQuery.setFilterActive(filterOpen || activeFilterGroupCount() > 0);
    if (onFilterOpenChanged) onFilterOpenChanged(filterOpen);
}

void FavoritesSidebar::applyFilterDraft()
{
    const bool subdirsChanged = includeSubdirs != filterDraftSnapshot.subdirs;
    filterEditing = false;
    filterDraftSnapshot = getFilter();
    filterApplyPending = false;
    if (onFilterChanged) onFilterChanged();
    if (subdirsChanged && onScanSubdirs)
        onScanSubdirs();
    setFilterOpen(false);
}

void FavoritesSidebar::cancelFilterDraft()
{
    if (filterEditing)
    {
        filterPanel.loadFrom(filterDraftSnapshot);
        filterPanel.setIncludeSubdirs(filterDraftSnapshot.subdirs);
        includeSubdirs = filterDraftSnapshot.subdirs;
    }
    filterEditing = false;
    setFilterOpen(false);
}

BrowserSearch FavoritesSidebar::getFilter() const
{
    BrowserSearch s;
    filterPanel.applyTo(s);
    s.subdirs = includeSubdirs;
    return s;
}

void FavoritesSidebar::setIncludeSubdirs(bool on)
{
    filterPanel.setIncludeSubdirs(on);
    if (includeSubdirs == on) return;
    includeSubdirs = on;
    repaint();
}

void FavoritesSidebar::setCurrentClipCount(int n)
{
    currentClipCount = juce::jmax(0, n);
    repaint();
}

void FavoritesSidebar::setStarredCount(int n)
{
    starredCount = juce::jmax(0, n);
    repaint();
}

void FavoritesSidebar::setScanning(bool on)
{
    if (scanning == on) return;
    scanning = on;
    if (!scanning)
    {
        scanProgressDone = 0;
        scanProgressTotal = 0;
    }
    if (scanning)
    {
        startTimerHz(30); // spin Update icon; filter debounce waits until idle
    }
    else if (filterApplyPending)
    {
        startTimer(280);
    }
    else
    {
        stopTimer();
    }
    filterPanel.setScanning(on);
    repaint();
}

void FavoritesSidebar::setScanFinished(bool finished)
{
    if (scanFinished == finished)
        return;
    scanFinished = finished;
    repaint();
}

void FavoritesSidebar::setScanProgress(int done, int total)
{
    scanProgressDone = juce::jmax(0, done);
    scanProgressTotal = juce::jmax(0, total);
    if (scanning)
        repaint(refreshBtnBounds);
}

void FavoritesSidebar::setFilterHistograms(const std::vector<StepClip>& clips)
{
    histogramClips = clips;
    filterPanel.setHistograms(clips);
}

BrowserSearch FavoritesSidebar::getCriteria() const
{
    BrowserSearch s = getFilter();
    s.query = searchQuery.getQuery();
    return s;
}

void FavoritesSidebar::setCriteria(const BrowserSearch& s)
{
    searchQuery.setQuery(s.query);
    filterPanel.loadFrom(s);
    setIncludeSubdirs(s.subdirs);
    repaint();
}

void FavoritesSidebar::scheduleFilterApply()
{
    filterApplyPending = true;
    if (!scanning)
        startTimer(280);
}

void FavoritesSidebar::timerCallback()
{
    if (scanning)
    {
        scanAngle += 0.28f;
        if (scanAngle > juce::MathConstants<float>::twoPi)
            scanAngle -= juce::MathConstants<float>::twoPi;
        repaint(refreshBtnBounds);
        return;
    }
    stopTimer();
    if (filterApplyPending)
    {
        filterApplyPending = false;
        if (onFilterChanged) onFilterChanged();
    }
}

bool FavoritesSidebar::isCurrentFolderFavorited() const
{
    return active.isNotEmpty() && dirs.contains(active);
}

int FavoritesSidebar::activeFilterGroupCount() const
{
    return filterPanel.activeGroupCount();
}

int FavoritesSidebar::folderCardHeight() const
{
    if (active.isEmpty() || !juce::File(active).isDirectory())
        return 0;
    // Card inset + title row + chip-height action row (refresh, scan, filters).
    int h = metrics::scaled(10) * 2
          + metrics::scaled(kCardHeaderH)
          + metrics::scaled(6)
          + 24;
    if (filterOpen)
        h += metrics::scaled(12) + filterPanel.idealHeight();
    return h;
}

int FavoritesSidebar::contentTopY() const
{
    return sidebarHeaderH();
}

int FavoritesSidebar::footerPad() const
{
    return sidebarBodyPadT();
}

int FavoritesSidebar::footerHeight() const
{
    return metrics::scaled(kBodyPadB);
}

std::vector<FavoritesSidebar::LayoutItem> FavoritesSidebar::collectLayout() const
{
    // Expanded: Open Folder… button, then Folders / Favorites / Saved Searches, rows edge to edge.
    // Collapsed: a rail of 28 px icon buttons.
    std::vector<LayoutItem> out;
    const int W = getWidth();
    const int padH = sidebarPadH();
    int y = contentTopY();

    if (collapsed)
    {
        // Rail: unfold · open folder · Files (star) · Folders · kept dirs · searches.
        const int btn = collapsedBtnSize();
        const int gap = metrics::scaled(10); // airier rail — icons were too tight at 4
        y += gap;
        out.push_back({ LayoutKind::FoldBtn, -1,
                        juce::Rectangle<int>(0, y, W, btn).withSizeKeepingCentre(btn, btn) });
        y += btn + gap;
        auto placeBtn = [&](int rowIdx)
        {
            out.push_back({ LayoutKind::Row, rowIdx,
                            juce::Rectangle<int>(0, y, W, btn).withSizeKeepingCentre(btn, btn) });
            y += btn + gap;
        };
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::Open) placeBtn(i);
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::Starred) placeBtn(i);
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::StarredFolders) placeBtn(i);
        y += gap;
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::SavedDir) placeBtn(i);
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::SavedSearch) placeBtn(i);
        out.push_back({ LayoutKind::End, -1, juce::Rectangle<int>(0, y, W, 0) });
        return out;
    }

    y += sidebarBodyPadT();
    out.push_back({ LayoutKind::OpenBtn, -1, juce::Rectangle<int>(padH, y, W - padH * 2, openFolderH()) });
    y += openFolderH() + metrics::scaled(kOpenToLibrary);
    if (active.isNotEmpty() && juce::File(active).isDirectory())
    {
        // Active-folder card only when not already listed under LIBRARY.
        out.push_back({ LayoutKind::FolderCard, -1,
                        juce::Rectangle<int>(padH, y, W - padH * 2, sidebarRowH()) });
        y += sidebarRowH() + sidebarSectionGap();
    }

    auto placeSection = [&](SectionId id)
    {
        out.push_back({ LayoutKind::Section, (int) id,
                        juce::Rectangle<int>(padH, y, W - padH * 2, sidebarSectionH()) });
        y += sidebarSectionH() + sidebarHeaderToRows();
    };
    auto rowHeightFor = [&](RowKind, int) -> int
    {
        // Favorites / Files / Folders / Saved Searches — same height as a grid clip.
        return sidebarRowH();
    };
    auto placeRows = [&](RowKind kind)
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == kind)
            {
                const int rh = rowHeightFor(kind, i);
                out.push_back({ LayoutKind::Row, i, juce::Rectangle<int>(0, y, W - 1, rh) });
                y += rh;
            }
    };

    auto hasKind = [&](RowKind kind)
    {
        for (const auto& row : rows)
            if (row.kind == kind) return true;
        return false;
    };
    // Mockup: omit empty LIBRARY so FAVORITES isn't stranded under a blank header.
    if (hasKind(RowKind::SavedDir))
    {
        placeSection(SectionId::Library);
        placeRows(RowKind::SavedDir);
        y += sidebarSectionGap();
    }
    placeSection(SectionId::Search);
    placeRows(RowKind::Starred);
    placeRows(RowKind::StarredFolders);
    if (hasKind(RowKind::SavedSearch))
    {
        y += sidebarSectionGap();
        placeSection(SectionId::Saved);
        placeRows(RowKind::SavedSearch);
    }
    out.push_back({ LayoutKind::End, -1, juce::Rectangle<int>(0, y, W, 0) });
    return out;
}

int FavoritesSidebar::contentBottomY() const
{
    int bottom = contentTopY();
    for (const auto& item : collectLayout())
    {
        if (item.kind == LayoutKind::End)
            bottom = item.bounds.getY();
        else if (!item.bounds.isEmpty())
            bottom = juce::jmax(bottom, item.bounds.getBottom());
    }
    return bottom;
}

int FavoritesSidebar::idealMinHeight() const
{
    return contentBottomY() + footerHeight();
}

int FavoritesSidebar::idealWidth() const
{
    if (collapsed)
        return metrics::sidebarRailWidth();
    return metrics::sidebarExpandedWidth();
}

void FavoritesSidebar::setExpandedWidth(int w)
{
    const int maxW = metrics::sidebarExpandedWidth();
    const int minW = metrics::sidebarRailWidth();
    expandedWidth = juce::jlimit(minW, maxW, w);
}

void FavoritesSidebar::setSavedDirs(const juce::StringArray& paths, const juce::String& activePath)
{
    dirs = paths;
    active = activePath;
    rebuildRows();
    layoutChildren();
    notifyHeightChanged();
}

void FavoritesSidebar::setSavedSearches(const std::vector<SavedSearchEntry>& searches, int activeIdx)
{
    savedSearches = searches;
    activeSearchIdx = activeIdx;
    rebuildRows();
    layoutChildren();
    notifyHeightChanged();
}

void FavoritesSidebar::setBrowseMode(int mode)
{
    browseMode = mode;
    rebuildRows();
    repaint();
}

void FavoritesSidebar::setStarredFilter(bool on)
{
    if (starredFilter == on) return;
    starredFilter = on;
    rebuildRows();
    repaint();
}

void FavoritesSidebar::setCollapsed(bool shouldCollapse)
{
    if (collapsed == shouldCollapse)
        return;
    collapsed = shouldCollapse;
    btnToggle.icon = collapsed ? icons::sidebarRight : icons::sidebar;
    btnToggle.setTooltip(collapsed ? "Expand sidebar" : "Collapse sidebar");
    btnToggle.active = collapsed;
    btnToggle.repaint();
    if (onCollapsedChanged) onCollapsedChanged();
    notifyHeightChanged();
}

void FavoritesSidebar::rebuildRows()
{
    rows.clear();
    rows.push_back({ RowKind::Open });
    for (int i = 0; i < dirs.size(); ++i)
        rows.push_back({ RowKind::SavedDir, i });
    rows.push_back({ RowKind::Starred });
    rows.push_back({ RowKind::StarredFolders });
    for (int i = 0; i < (int) savedSearches.size(); ++i)
        rows.push_back({ RowKind::SavedSearch, i });
    rebuildNavItems();
}

void FavoritesSidebar::rebuildNavItems()
{
    navItems.clear();
    if (!collapsed)
        navItems.push_back({ NavKind::OpenFolder, -1 });
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto k = rows[(size_t) i].kind;
        if (collapsed || k != RowKind::Open)
            navItems.push_back({ NavKind::Row, i });
    }
    if (!juce::isPositiveAndBelow(navIndex, (int) navItems.size()))
        navIndex = navItems.empty() ? -1 : 0;
}

juce::Rectangle<int> FavoritesSidebar::navItemBounds(const NavItem& item) const
{
    switch (item.kind)
    {
        case NavKind::OpenFolder:  return libraryPlusBounds;
        case NavKind::Star:        return starBounds;
        case NavKind::Refresh:     return refreshBtnBounds;
        case NavKind::ScanSubdirs: return scanBtnBounds;
        case NavKind::Filters:
        {
            if (!searchQuery.isVisible())
                return {};
            auto cap = searchQuery.getLocalBounds().removeFromRight(searchQuery.getHeight());
            return cap.translated(searchQuery.getX(), searchQuery.getY());
        }
        case NavKind::Settings:   return settingsRowBounds;
        case NavKind::Row:        return rowBounds(item.rowIdx);
    }
    return {};
}

void FavoritesSidebar::activateNavItem(const NavItem& item)
{
    switch (item.kind)
    {
        case NavKind::OpenFolder:
            if (onOpenFolder) onOpenFolder();
            break;
        case NavKind::Star:
            if (onToggleStarFolder) onToggleStarFolder();
            else if (onAddCurrent) onAddCurrent();
            break;
        case NavKind::Refresh:
            if (!scanning && onRefreshFolder) onRefreshFolder();
            break;
        case NavKind::ScanSubdirs:
            if (!scanning && onScanSubdirs) onScanSubdirs();
            break;
        case NavKind::Filters:
            toggleFilterPanel();
            break;
        case NavKind::Settings:
            if (onOpenSettings) onOpenSettings();
            break;
        case NavKind::Row:
        {
            if (!juce::isPositiveAndBelow(item.rowIdx, (int) rows.size()))
                break;
            const auto& row = rows[(size_t) item.rowIdx];
            switch (row.kind)
            {
                case RowKind::Open:
                    if (onOpenFolder) onOpenFolder();
                    break;
                case RowKind::SavedDir:
                    if (onPickDir) onPickDir(dirs[row.index]);
                    break;
                case RowKind::Starred:
                    if (onShowStarred) onShowStarred();
                    break;
                case RowKind::StarredFolders:
                    if (onShowStarredFolders) onShowStarredFolders();
                    break;
                case RowKind::SavedSearch:
                    if (onPickSavedSearch) onPickSavedSearch(row.index);
                    break;
            }
            break;
        }
    }
    rebuildNavItems();
    repaint();
}

void FavoritesSidebar::moveNav(int delta)
{
    if (navItems.empty()) return;
    if (navIndex < 0) navIndex = 0;
    else navIndex = (navIndex + delta + (int) navItems.size()) % (int) navItems.size();
    repaint();
}

void FavoritesSidebar::updateTooltipForPos(juce::Point<int> pos)
{
    if (starBounds.contains(pos))
    {
        setTooltip(currentFolderStarred ? "Remove from Favorite Folders" : "Add to Favorite Folders");
        return;
    }
    if (refreshBtnBounds.contains(pos))
    {
        setTooltip(scanning
                       ? (scanProgressTotal > 0
                              ? ("Scanning " + juce::String(scanProgressDone) + "/"
                                 + juce::String(scanProgressTotal))
                              : juce::String("Scanning..."))
                       : juce::String("Refresh this folder"));
        return;
    }
    if (scanBtnBounds.contains(pos))
    {
        setTooltip("Scan subdirectories");
        return;
    }
    if (filtersBtnBounds.contains(pos))
    {
        setTooltip("Filters");
        return;
    }
    if (libraryPlusBounds.contains(pos))
    {
        setTooltip("Open Folder");
        return;
    }
    const auto hit = rowHitAt(pos);
    if (hit.valid && hit.removeZone)
    {
        setTooltip("Remove");
        return;
    }
    // Collapsed rail: hover labels for each icon.
    if (collapsed)
    {
        for (const auto& item : collectLayout())
        {
            if (!item.bounds.contains(pos))
                continue;
            if (item.kind == LayoutKind::FoldBtn)
            {
                setTooltip("Expand sidebar");
                return;
            }
            if (item.kind == LayoutKind::Row
                && juce::isPositiveAndBelow(item.id, (int) rows.size()))
            {
                const auto& row = rows[(size_t) item.id];
                switch (row.kind)
                {
                    case RowKind::Open:
                        setTooltip("Open Folder");
                        return;
                    case RowKind::Starred:
                        setTooltip("Favorite Files");
                        return;
                    case RowKind::StarredFolders:
                        setTooltip("Favorite Folders");
                        return;
                    case RowKind::SavedDir:
                    {
                        const juce::File dir(dirs[row.index]);
                        const auto name = dir.getFileName();
                        setTooltip(name.isNotEmpty() ? name : dirs[row.index]);
                        return;
                    }
                    case RowKind::SavedSearch:
                    {
                        const auto& entry = savedSearches[(size_t) row.index];
                        setTooltip(entry.name.isNotEmpty() ? entry.name
                                   : entry.search.query.isNotEmpty() ? entry.search.query
                                                                     : juce::String("Saved search"));
                        return;
                    }
                }
            }
        }
    }
    setTooltip({});
}

void FavoritesSidebar::focusGained(FocusChangeType)
{
    if (navIndex < 0 && !navItems.empty())
        navIndex = 0;
    repaint();
}

void FavoritesSidebar::focusLost(FocusChangeType)
{
    repaint();
}

void FavoritesSidebar::layoutChildren()
{
    openFolderBounds = {};
    folderCardBounds = {};
    starBounds = {};
    refreshBtnBounds = {};
    scanBtnBounds = {};
    filtersBtnBounds = {};
    libraryHeaderBounds = {};
    libraryPlusBounds = {};
    searchHeaderBounds = {};
    savedHeaderBounds = {};

    const int ctl = metrics::chromeIconButton();
    btnToggle.setVisible(true);

    if (collapsed)
    {
        juce::Rectangle<int> foldBounds;
        for (const auto& item : collectLayout())
            if (item.kind == LayoutKind::FoldBtn)
                foldBounds = item.bounds;
        if (!foldBounds.isEmpty())
            btnToggle.setBounds(foldBounds);
        else
            btnToggle.setBounds({});
        return;
    }

    for (const auto& item : collectLayout())
    {
        if (item.kind == LayoutKind::OpenBtn)
        {
            auto row = item.bounds;
            // Mockup: fold chip matches Open Folder height, with a clear gap.
            const int gap = metrics::scaled(6);
            const int foldH = juce::jmin(row.getHeight(), openFolderH());
            const auto fold = row.removeFromRight(foldH).withSizeKeepingCentre(foldH, foldH);
            row.removeFromRight(gap);
            btnToggle.setBounds(fold);
            libraryPlusBounds = row;
            openFolderBounds = row;
        }
        else if (item.kind == LayoutKind::FolderCard)
        {
            folderCardBounds = item.bounds;
            auto row = item.bounds;
            starBounds = row.removeFromLeft(metrics::scaled(22))
                             .withSizeKeepingCentre(metrics::scaled(18), metrics::scaled(18));
        }
        else if (item.kind == LayoutKind::Section)
        {
            switch ((SectionId) item.id)
            {
                case SectionId::Library: libraryHeaderBounds = item.bounds; break;
                case SectionId::Search:  searchHeaderBounds = item.bounds; break;
                case SectionId::Saved:   savedHeaderBounds = item.bounds; break;
            }
        }
    }
    searchQuery.setFilterActive(filterOpen || activeFilterGroupCount() > 0);
}

juce::Rectangle<int> FavoritesSidebar::rowBounds(int rowIdx) const
{
    for (const auto& item : collectLayout())
        if (item.kind == LayoutKind::Row && item.id == rowIdx)
            return item.bounds;
    return {};
}

FavoritesSidebar::RowHit FavoritesSidebar::rowHitAt(juce::Point<int> pos) const
{
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto r = rowBounds(i);
        if (!r.contains(pos)) continue;
        RowHit hit;
        hit.valid = true;
        hit.kind = rows[(size_t) i].kind;
        hit.index = rows[(size_t) i].index;
        hit.removeZone = !collapsed
            && (hit.kind == RowKind::SavedDir || hit.kind == RowKind::SavedSearch)
            && pos.x > r.getRight() - 22;
        return hit;
    }
    return {};
}

FavoritesSidebar::ChromeHit FavoritesSidebar::chromeHitAt(juce::Point<int> pos) const
{
    if (settingsRowBounds.contains(pos)) return ChromeHit::Settings;
    if (collapsed) return ChromeHit::None;
    if (libraryPlusBounds.contains(pos)) return ChromeHit::OpenFolder;
    if (starBounds.contains(pos)) return ChromeHit::Star;
    if (refreshBtnBounds.contains(pos)) return ChromeHit::Refresh;
    if (scanBtnBounds.contains(pos)) return ChromeHit::ScanSubdirs;
    if (filtersBtnBounds.contains(pos)) return ChromeHit::Filters;
    return ChromeHit::None;
}

void FavoritesSidebar::setSearchFormOpen(bool open)
{
    searchOpen = open;
    if (open)
        searchQuery.focusQuery();
    else
        searchQuery.blurQuery();
}

void FavoritesSidebar::deactivateSearch(bool clearCriteria)
{
    searchQuery.blurQuery();
    if (clearCriteria)
        searchQuery.clearQuery();
}

void FavoritesSidebar::toggleFilterPanel()
{
    setFilterOpen(!filterOpen);
}

void FavoritesSidebar::resized()
{
    btnToggle.setVisible(false);
    btnToggle.setBounds({});
    settingsRowBounds = {};
    btnSettings.setVisible(false);
    layoutChildren();
    rebuildNavItems();
}

void FavoritesSidebar::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);

    const bool overSettings = settingsRowBounds.contains(e.getPosition());
    const auto chrome = chromeHitAt(e.getPosition());
    const auto hit = rowHitAt(e.getPosition());
    const int rowIdx = hit.valid ? [&]
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == hit.kind && rows[(size_t) i].index == hit.index)
                return i;
        return -1;
    }() : -1;
    if (rowIdx != hoverRow || hit.removeZone != hoverRemove
        || overSettings != hoverSettings || chrome != hoverChrome)
    {
        hoverRow = rowIdx;
        hoverRemove = hit.removeZone;
        hoverSettings = overSettings;
        hoverChrome = chrome;
        repaint();
    }
    updateTooltipForPos(e.getPosition());
}

void FavoritesSidebar::mouseExit(const juce::MouseEvent&)
{
    if (!resizing)
        setMouseCursor(juce::MouseCursor::NormalCursor);
    hoverRow = -1;
    hoverRemove = false;
    hoverSettings = false;
    hoverChrome = ChromeHit::None;
    setTooltip({});
    repaint();
}

void FavoritesSidebar::mouseDown(const juce::MouseEvent& e)
{
    if (btnSettings.isVisible() && btnSettings.getBounds().contains(e.getPosition()))
        return;
    if (btnToggle.isVisible() && btnToggle.getBounds().contains(e.getPosition()))
        return;

    if (! hasKeyboardFocus(true) && ! searchQuery.isQueryFocused())
        grabKeyboardFocus();

    auto syncNav = [this](NavKind kind, int rowIdx = -1)
    {
        for (int i = 0; i < (int) navItems.size(); ++i)
            if (navItems[(size_t) i].kind == kind
                && (rowIdx < 0 || navItems[(size_t) i].rowIdx == rowIdx))
            {
                navIndex = i;
                break;
            }
        repaint();
    };

    switch (chromeHitAt(e.getPosition()))
    {
        case ChromeHit::Settings:
            syncNav(NavKind::Settings);
            if (onOpenSettings) onOpenSettings();
            return;
        case ChromeHit::OpenFolder:
            syncNav(NavKind::OpenFolder);
            if (onOpenFolder) onOpenFolder();
            return;
        case ChromeHit::Star:
            syncNav(NavKind::Star);
            if (onToggleStarFolder) onToggleStarFolder();
            else if (onAddCurrent) onAddCurrent();
            return;
        case ChromeHit::Refresh:
            syncNav(NavKind::Refresh);
            if (!scanning && onRefreshFolder) onRefreshFolder();
            return;
        case ChromeHit::ScanSubdirs:
            syncNav(NavKind::ScanSubdirs);
            if (!scanning && onScanSubdirs) onScanSubdirs();
            return;
        case ChromeHit::Filters:
            syncNav(NavKind::Filters);
            toggleFilterPanel();
            return;
        case ChromeHit::None:
            break;
    }

    const auto hit = rowHitAt(e.getPosition());
    if (!hit.valid) return;

    int rowIdx = -1;
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].kind == hit.kind && rows[(size_t) i].index == hit.index)
            rowIdx = i;

    if (hit.removeZone)
    {
        if (hit.kind == RowKind::SavedDir && onRemoveDir)
            onRemoveDir(dirs[hit.index]);
        else if (hit.kind == RowKind::SavedSearch && onRemoveSavedSearch)
            onRemoveSavedSearch(hit.index);
        return;
    }

    syncNav(NavKind::Row, rowIdx);
    if (juce::isPositiveAndBelow(rowIdx, (int) rows.size()))
        activateNavItem({ NavKind::Row, rowIdx });
}

bool FavoritesSidebar::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (filterOpen)
        {
            setFilterOpen(false);
            return true;
        }
        if (searchQuery.isQueryFocused())
        {
            deactivateSearch(false);
            return true;
        }
        return false;
    }
    if (searchQuery.isQueryFocused())
        return false;

    if (navItems.empty())
        rebuildNavItems();

    if (key == juce::KeyPress::upKey || key == juce::KeyPress::leftKey)
    {
        moveNav(-1);
        return true;
    }
    if (key == juce::KeyPress::downKey || key == juce::KeyPress::rightKey)
    {
        moveNav(1);
        return true;
    }
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        if (juce::isPositiveAndBelow(navIndex, (int) navItems.size()))
            activateNavItem(navItems[(size_t) navIndex]);
        return true;
    }
    return false;
}

void FavoritesSidebar::mouseDrag(const juce::MouseEvent&)
{
    // Fixed sidebar width — no drag-resize.
}

void FavoritesSidebar::mouseUp(const juce::MouseEvent&)
{
    resizing = false;
}

void FavoritesSidebar::paintSectionLabel(juce::Graphics& g, const juce::String& title,
                                         juce::Rectangle<int> bounds)
{
    // Mockup: small caps, muted, open tracking, inset with the row gutter.
    if (bounds.isEmpty()) return;
    g.setColour(ds::inkMuted());
    auto font = monoFontFixed(kSectionLabelPt, true);
    font.setExtraKerningFactor(0.08f);
    g.setFont(font);
    g.drawText(title.toUpperCase(), bounds.withTrimmedLeft(sidebarRowPadX()),
               juce::Justification::centredLeft, false);
}

void FavoritesSidebar::paintOpenFolderButton(juce::Graphics& g, juce::Rectangle<int> r,
                                             bool hover, bool focus)
{
    if (r.isEmpty()) return;
    auto face = r.toFloat();
    // Mockup secondary: surface face, edge stroke, folder glyph (not plus).
    g.setColour(hover ? ds::surface300() : ds::surface200());
    g.fillRoundedRectangle(face, ds::radius::md);
    g.setColour(ds::edge());
    g.drawRoundedRectangle(face.reduced(0.5f), ds::radius::md, 1.0f);
    auto inner = r.reduced(metrics::scaled(12), 0);
    const float glyph = 13.0f;
    drawIcon(g, icons::folder, inner.removeFromLeft((int) glyph + 2).toFloat().withSizeKeepingCentre(glyph, glyph),
             ds::ink(), 1.45f);
    inner.removeFromLeft(6);
    g.setColour(ds::ink());
    float pt = 12.0f;
    auto font = uiFontFixed(pt, true);
    const juce::String label = "Open Folder";
    while (pt > 9.0f
           && juce::GlyphArrangement::getStringWidth(font, label) > (float) inner.getWidth())
    {
        pt -= 0.5f;
        font = uiFontFixed(pt, true);
    }
    g.setFont(font);
    g.drawText(label, inner, juce::Justification::centredLeft, true);
    if (focus)
        drawFocusRing(g, face, ds::radius::md);
}

void FavoritesSidebar::paintActionButton(juce::Graphics& g, juce::Rectangle<int> r,
                                         bool hover, bool active, bool disabled)
{
    if (r.isEmpty()) return;
    const auto p = sl();
    fillControlSurface(g, r.toFloat(), 6.0f, false);
    if (active)
    {
        g.setColour(p.accent.withAlpha(0.16f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
    }
    else if (hover && !disabled)
    {
        g.setColour(p.accent.withAlpha(0.08f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
    }
    if (disabled)
        g.setColour(p.text3.withAlpha(0.45f));
}

void FavoritesSidebar::setCurrentFolderStarred(bool on)
{
    if (currentFolderStarred == on) return;
    currentFolderStarred = on;
    repaint(folderCardBounds.isEmpty() ? getLocalBounds() : folderCardBounds);
}

void FavoritesSidebar::paintFolderCard(juce::Graphics& g)
{
    if (folderCardBounds.isEmpty()) return;
    const bool starred = currentFolderStarred;
    const juce::File dir(active);
    const auto name = dir.getFileName().isNotEmpty() ? dir.getFileName() : active;

    if (hoverChrome == ChromeHit::Star)
    {
        g.setColour(ds::surface200());
        g.fillRect(folderCardBounds);
    }

    auto iconSlot = starBounds.toFloat().withSizeKeepingCentre(14.0f, 14.0f);
    drawIcon(g, starred ? icons::star : icons::starOutline, iconSlot,
             starred ? favoriteGold() : ds::inkMuted(), starred ? 1.6f : 1.35f);

    auto nameR = folderCardBounds;
    nameR.removeFromLeft(juce::jmax(starBounds.getWidth(), metrics::scaled(22)) + 4);
    if (currentClipCount > 0)
    {
        // Mockup: plain muted count, not a pill.
        const auto txt = juce::String(currentClipCount);
        const auto bf = uiFontFixed(12.0f, false);
        const float tw = juce::GlyphArrangement::getStringWidth(bf, txt);
        auto countR = nameR.removeFromRight((int) std::ceil(tw) + 2);
        g.setColour(ds::inkMuted());
        g.setFont(bf);
        g.drawText(txt, countR, juce::Justification::centredRight, false);
        nameR.removeFromRight(6);
    }
    g.setColour(ds::ink());
    g.setFont(uiFontFixed(kSectionTitlePt, false));
    g.drawText(name, nameR, juce::Justification::centredLeft, true);
}

void FavoritesSidebar::paintRow(juce::Graphics& g, int rowIdx, const juce::Rectangle<int>& r,
                                bool hovered, bool removeZone, bool keyboardFocus)
{
    if (r.isEmpty() || !juce::isPositiveAndBelow(rowIdx, (int) rows.size()))
        return;

    const auto& row = rows[(size_t) rowIdx];
    const bool activeDir = row.kind == RowKind::SavedDir && dirs[row.index] == active;
    const bool activeStar = row.kind == RowKind::Starred && (starredFilter || browseMode == 1);
    const bool activeStarFolders = row.kind == RowKind::StarredFolders && browseMode == 3;
    const bool activeSearch = row.kind == RowKind::SavedSearch && row.index == activeSearchIdx;
    const bool isActive = activeDir || activeStar || activeStarFolders || activeSearch;

    enum class Lead { None, Dot, Star, Folder, Search };
    Lead lead = Lead::None;
    juce::String label;
    juce::Colour iconCol = ds::inkMuted();
    switch (row.kind)
    {
        case RowKind::Open:
            lead = Lead::Folder;
            label = "Open Folder";
            iconCol = ds::ink();
            break;
        case RowKind::SavedDir:
        {
            // Mockup LIBRARY: purple selection dot only when active; no idle icon.
            lead = isActive ? Lead::Dot : Lead::None;
            const juce::File dir(dirs[row.index]);
            label = dir.getFileName().isNotEmpty() ? dir.getFileName() : dirs[row.index];
            iconCol = ds::acc();
            break;
        }
        case RowKind::Starred:
            lead = Lead::Star;
            label = "Files";
            iconCol = favoriteGold();
            break;
        case RowKind::StarredFolders:
            lead = Lead::Folder;
            label = "Folders";
            iconCol = favoriteGold();
            break;
        case RowKind::SavedSearch:
        {
            lead = Lead::Search;
            const auto& entry = savedSearches[(size_t) row.index];
            label = entry.name.isNotEmpty() ? entry.name
                  : entry.search.query.isNotEmpty() ? entry.search.query : juce::String("Search");
            break;
        }
    }

    if (collapsed)
    {
        // Rail: icon only — active/hover tint the glyph, no square face.
        auto b = r.toFloat();
        const float glyph = (float) metrics::chromeIconGlyphSize();
        const char* railIcon = icons::folder;
        if (lead == Lead::Star) railIcon = icons::star;
        else if (lead == Lead::Search) railIcon = icons::search;
        else if (lead == Lead::Dot) railIcon = icons::folder;
        auto col = row.kind == RowKind::SavedDir ? ds::ink() : iconCol;
        if (isActive)
            col = (row.kind == RowKind::Starred || row.kind == RowKind::StarredFolders)
                      ? favoriteGold() : ds::ink();
        if (isActive)
        {
            g.setColour(ds::ink().withAlpha(0.07f));
            g.fillRoundedRectangle(b.reduced(4.0f, 2.0f), ds::radius::md);
        }
        else if (hovered)
            col = ds::ink();
        drawIcon(g, railIcon, b.withSizeKeepingCentre(glyph, glyph), col, kSidebarIconStroke);
        if (keyboardFocus)
            drawFocusRing(g, b, ds::radius::sm);
        return;
    }

    // Mockup selection: inset rounded selsoft wash — no leading accent bar.
    if (isActive)
    {
        auto wash = r.toFloat().reduced((float) sidebarPadH(), 1.0f);
        g.setColour(ds::selsoft());
        g.fillRoundedRectangle(wash, ds::radius::sm);
    }
    else if (hovered)
    {
        auto wash = r.toFloat().reduced((float) sidebarPadH(), 1.0f);
        g.setColour(ds::ink().withAlpha(0.04f));
        g.fillRoundedRectangle(wash, ds::radius::sm);
    }

    auto textArea = r.reduced(sidebarRowPadX(), 0);
    auto leadSlot = textArea.removeFromLeft(sidebarIconCol()).toFloat();
    if (lead == Lead::Dot)
    {
        const float d = kSelectDot;
        g.setColour(ds::acc());
        g.fillEllipse(leadSlot.withSizeKeepingCentre(d, d));
    }
    else if (lead == Lead::Star)
    {
        drawIcon(g, icons::star, leadSlot.withSizeKeepingCentre(12.0f, 12.0f),
                 iconCol, kSidebarIconStroke);
    }
    else if (lead == Lead::Folder)
    {
        drawIcon(g, icons::folder, leadSlot.withSizeKeepingCentre(12.0f, 12.0f),
                 iconCol, kSidebarIconStroke);
    }
    else if (lead == Lead::Search)
    {
        drawIcon(g, icons::search, leadSlot.withSizeKeepingCentre(12.0f, 12.0f),
                 iconCol, kSidebarIconStroke);
    }
    textArea.removeFromLeft(sidebarRowIconGap());

    if ((hovered || keyboardFocus) && (row.kind == RowKind::SavedDir || row.kind == RowKind::SavedSearch))
    {
        auto xArea = textArea.removeFromRight(14).toFloat().withSizeKeepingCentre(8.0f, 8.0f);
        drawIcon(g, icons::x, xArea, removeZone ? ds::ink() : ds::inkMuted(), 1.3f);
    }

    // Mockup: muted plain count on the trailing edge (no pill).
    int badgeN = 0;
    if (row.kind == RowKind::Starred && starredCount > 0)
        badgeN = starredCount;
    else if (row.kind == RowKind::SavedDir && dirs[row.index] == active && currentClipCount > 0)
        badgeN = currentClipCount;
    else if (row.kind == RowKind::SavedSearch)
        badgeN = (int) savedSearches[(size_t) row.index].resultPaths.size();
    if (badgeN > 0)
    {
        const auto txt = juce::String(badgeN);
        const auto bf = uiFontFixed(12.0f, false);
        const float tw = juce::GlyphArrangement::getStringWidth(bf, txt);
        auto countR = textArea.removeFromRight((int) std::ceil(tw) + 2);
        g.setColour(ds::inkMuted());
        g.setFont(bf);
        g.drawText(txt, countR, juce::Justification::centredRight, false);
        textArea.removeFromRight(6);
    }

    g.setColour(ds::ink());
    g.setFont(uiFontFixed(kSectionTitlePt, isActive));
    g.drawText(label, textArea, juce::Justification::centredLeft, true);

    if (keyboardFocus)
        drawFocusRing(g, r.toFloat().reduced((float) sidebarPadH(), 1.0f), ds::radius::sm);
}

void FavoritesSidebar::paint(juce::Graphics& g)
{
    // Levels sidebar: surface100 well (4% ink step).
    g.fillAll(ds::surface100());

    g.setColour(ds::line());
    g.fillRect(getWidth() - 1, 0, 1, getHeight());

    const bool kb = hasKeyboardFocus(true);
    const NavItem* focused = (kb && juce::isPositiveAndBelow(navIndex, (int) navItems.size()))
        ? &navItems[(size_t) navIndex] : nullptr;

    if (!collapsed)
    {
        paintOpenFolderButton(g, openFolderBounds, hoverChrome == ChromeHit::OpenFolder,
                              focused != nullptr && focused->kind == NavKind::OpenFolder);
        paintFolderCard(g);
        paintSectionLabel(g, "Library", libraryHeaderBounds);
        paintSectionLabel(g, "Favorites", searchHeaderBounds);
        paintSectionLabel(g, "Saved Searches", savedHeaderBounds);
    }

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        if (!collapsed && rows[(size_t) i].kind == RowKind::Open)
            continue;
        const auto r = rowBounds(i);
        const bool rowFocus = focused != nullptr && focused->kind == NavKind::Row && focused->rowIdx == i;
        paintRow(g, i, r, i == hoverRow, i == hoverRow && hoverRemove, rowFocus);
    }
}

// ── FileListPanel ────────────────────────────────────────────────────────────

FileListPanel::FileListPanel()
{
    setWantsKeyboardFocus(true);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(false, false);
    viewport.setScrollBarThickness(10);
    addAndMakeVisible(viewport);

    rowScroll.addListener(this);
    rowScroll.setAutoHide(false);
    addAndMakeVisible(rowScroll);

    scrollTopBtn.ghost = true;
    scrollTopBtn.iconScale = 1.1f;
    scrollTopBtn.setTooltip("Scroll to top");
    scrollTopBtn.onClick = [this] { setScrollRow(0); };
    addChildComponent(scrollTopBtn);
    scrollTopBtn.setVisible(false);

    startTimerHz(20);
}

void FileListPanel::updateScrollTopButton()
{
    const bool show = scrollRow > 0 && !searching;
    if (scrollTopBtn.isVisible() != show)
        scrollTopBtn.setVisible(show);
    if (show)
    {
        constexpr int kBtn = 28;
        constexpr int kPad = 10;
        const auto listArea = getLocalBounds().withTrimmedTop(metrics::listHeaderH());
        scrollTopBtn.setBounds(listArea.getRight() - kBtn - kPad,
                               listArea.getBottom() - kBtn - kPad,
                               kBtn, kBtn);
        scrollTopBtn.toFront(false);
    }
}

void FileListPanel::grabBrowseFocus()
{
    // Prefer the panel itself over combos/sliders/viewports so arrow keys
    // reach browse navigation after the user clicks a file row.
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (focused != this && !isParentOf(focused))
            focused->giveAwayKeyboardFocus();
    grabKeyboardFocus();
}

void FileListPanel::setFolderName(const juce::String& name)
{
    folderName = name;
    repaint();
    if (onFolderNameChanged) onFolderNameChanged(name);
}

void FileListPanel::setEmptyHint(const juce::String& hint)
{
    emptyHint = hint.isNotEmpty() ? hint : "No files";
    content.repaint();
}

void FileListPanel::setEntries(std::vector<FileListEntry> e)
{
    entries = std::move(e);
    selected = juce::jlimit(-1, (int) entries.size() - 1, selected);
    content.clearDragState();
    rebuildSortOrder();
    if (getWidth() > 0 && getHeight() > 0)
        resized();
    else
        updateContentSize();
    content.repaint();
    repaint();
    if (onEntriesChanged) onEntriesChanged();
}

void FileListPanel::updateEntry(int index, const FileListEntry& entry)
{
    if (!juce::isPositiveAndBelow(index, (int) entries.size()))
        return;
    entries[(size_t) index] = entry;
    rebuildSortOrder();
    content.repaint();
}

void FileListPanel::setSelectedIndex(int index, juce::NotificationType notify)
{
    selected = juce::jlimit(-1, (int) entries.size() - 1, index);
    ensureRowVisible(entryToDisplay(selected));
    content.repaint();
    // Callbacks use entry indices (parallel to MidiToolkitEditor::displayRows), not sort-order rows.
    if (notify != juce::dontSendNotification && onSelect && selected >= 0)
        onSelect(selected);
}

void FileListPanel::setPlaying(bool isPlaying)
{
    playing = isPlaying;
    content.repaint();
}

void FileListPanel::setScanProgress(int done, int total)
{
    scanDone = juce::jmax(0, done);
    scanTotal = juce::jmax(0, total);
    if (searching)
        repaint();
}

void FileListPanel::setSearching(bool isSearch)
{
    searching = isSearch;
    if (!searching)
    {
        scanDone = 0;
        scanTotal = 0;
    }
    // Hide the list under the scanning overlay so progress paints on top.
    viewport.setVisible(!searching);
    rowScroll.setVisible(false);
    scrollTopBtn.setVisible(false);
    // Timer also drives the scroll-to-top FAB visibility.
    startTimerHz(searching ? 12 : 20);
    repaint();
}

void FileListPanel::setSort(SortColumn column, bool ascending)
{
    sortColumn = column;
    sortAscending = ascending;
    rebuildSortOrder();
    content.repaint();
    repaint();
}

void FileListPanel::rebuildSortOrder()
{
    sortOrder.resize(entries.size());
    for (int i = 0; i < (int) entries.size(); ++i)
        sortOrder[(size_t) i] = i;

    std::stable_sort(sortOrder.begin(), sortOrder.end(), [this](int a, int b)
    {
        const auto& ea = entries[(size_t) a];
        const auto& eb = entries[(size_t) b];
        // Folders always first.
        if (ea.isDirectory != eb.isDirectory)
            return ea.isDirectory && !eb.isDirectory;

        int cmp = 0;
        switch (sortColumn)
        {
            case SortColumn::Name:
                cmp = ea.name.compareNatural(eb.name);
                break;
            case SortColumn::Key:
                cmp = ea.rootName.compareNatural(eb.rootName);
                break;
            case SortColumn::Tempo:
                cmp = (ea.bpm < eb.bpm) ? -1 : (ea.bpm > eb.bpm ? 1 : 0);
                break;
            case SortColumn::Bars:
                cmp = ea.bars - eb.bars;
                break;
            case SortColumn::Kind:
                cmp = juce::String(clipKindName(ea.kind))
                          .compareNatural(juce::String(clipKindName(eb.kind)));
                break;
            case SortColumn::Complexity:
                cmp = ea.complexity - eb.complexity;
                break;
            case SortColumn::DifNotes:
                cmp = ea.difNotes - eb.difNotes;
                break;
            case SortColumn::TimeSig:
                cmp = (ea.timeSigNum * 100 + ea.timeSigDen)
                    - (eb.timeSigNum * 100 + eb.timeSigDen);
                break;
            case SortColumn::Notes:
                cmp = ea.noteCount - eb.noteCount;
                break;
        }
        if (cmp == 0)
            cmp = ea.name.compareNatural(eb.name);
        return sortAscending ? cmp < 0 : cmp > 0;
    });

    displayOfEntry.assign(entries.size(), -1);
    for (int i = 0; i < (int) sortOrder.size(); ++i)
        displayOfEntry[(size_t) sortOrder[(size_t) i]] = i;
    setScrollRow(scrollRow);
}

int FileListPanel::displayToEntry(int displayIdx) const
{
    if (!juce::isPositiveAndBelow(displayIdx, (int) sortOrder.size()))
        return -1;
    return sortOrder[(size_t) displayIdx];
}

int FileListPanel::entryToDisplay(int entryIdx) const
{
    if (!juce::isPositiveAndBelow(entryIdx, (int) displayOfEntry.size()))
        return -1;
    return displayOfEntry[(size_t) entryIdx];
}

int FileListPanel::visibleRowCount() const
{
    const int rowH = juce::jmax(1, metrics::listRowH());
    const int h = viewport.getMaximumVisibleHeight();
    if (h <= 0)
        return juce::jmax(1, (int) sortOrder.size());
    return juce::jmax(1, h / rowH);
}

int FileListPanel::displayIndexAtContentY(int y) const
{
    const int rowH = metrics::listRowH();
    if (rowH <= 0 || y < 0)
        return -1;
    const int disp = scrollRow + y / rowH;
    if (!juce::isPositiveAndBelow(disp, (int) sortOrder.size()))
        return -1;
    return disp;
}

void FileListPanel::setScrollRow(int row)
{
    const int maxRow = juce::jmax(0, (int) sortOrder.size() - visibleRowCount());
    scrollRow = juce::jlimit(0, maxRow, row);
    syncRowScroll();
    content.repaint();
    updateScrollTopButton();
}

void FileListPanel::syncRowScroll()
{
    const int n = (int) sortOrder.size();
    const int vis = visibleRowCount();
    rowScroll.setRangeLimits(0.0, (double) juce::jmax(vis, n), juce::dontSendNotification);
    rowScroll.setCurrentRange((double) scrollRow, (double) vis, juce::dontSendNotification);
}

void FileListPanel::scrollBarMoved(juce::ScrollBar*, double newRangeStart)
{
    const int next = (int) std::lround(newRangeStart);
    if (next == scrollRow)
        return;
    const int maxRow = juce::jmax(0, (int) sortOrder.size() - visibleRowCount());
    scrollRow = juce::jlimit(0, maxRow, next);
    content.repaint();
    updateScrollTopButton();
}

void FileListPanel::selectAdjacent(int direction)
{
    if (sortOrder.empty()) return;
    int disp = entryToDisplay(selected);
    if (disp < 0) disp = direction > 0 ? -1 : (int) sortOrder.size();
    disp = juce::jlimit(0, (int) sortOrder.size() - 1, disp + direction);
    setSelectedIndex(displayToEntry(disp));
}

bool FileListPanel::keyPressed(const juce::KeyPress& key)
{
    const bool browseNav = key == juce::KeyPress::upKey || key == juce::KeyPress::downKey
        || key == juce::KeyPress::pageUpKey || key == juce::KeyPress::pageDownKey
        || key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey
        || key == juce::KeyPress::returnKey;

    // Browse navigation always reclaims focus from effects/combos/text fields.
    if (browseNav)
    {
        if (!hasKeyboardFocus(true))
            grabKeyboardFocus();

        if (key == juce::KeyPress::upKey)    { selectAdjacent(-1); return true; }
        if (key == juce::KeyPress::downKey)  { selectAdjacent(1); return true; }
        if (key == juce::KeyPress::leftKey)
        {
            if (onEnterParent) onEnterParent();
            return true;
        }
        if (key == juce::KeyPress::rightKey || key == juce::KeyPress::returnKey)
        {
            if (selected >= 0 && entries[(size_t) selected].isDirectory)
            {
                if (onEnterFolder)
                    onEnterFolder(selected);
            }
            return true;
        }

        const int pageRows = juce::jmax(1, viewport.getMaximumVisibleHeight() / metrics::listRowH());
        if (key == juce::KeyPress::pageUpKey)   { selectAdjacent(-pageRows); return true; }
        if (key == juce::KeyPress::pageDownKey) { selectAdjacent(pageRows); return true; }
        return true;
    }

    if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr
        || dynamic_cast<juce::ComboBox*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        return false;

    if (!hasKeyboardFocus(true))
        grabKeyboardFocus();

    return false;
}

void FileListPanel::ensureRowVisible(int displayIdx)
{
    if (displayIdx < 0) return;
    const int vis = visibleRowCount();
    if (displayIdx < scrollRow)
        setScrollRow(displayIdx);
    else if (displayIdx >= scrollRow + vis)
        setScrollRow(displayIdx - vis + 1);
}

void FileListPanel::updateContentSize()
{
    const int viewW = juce::jmax(1, viewport.getMaximumVisibleWidth());
    const int contentW = juce::jmax(viewW, totalContentWidth());
    const bool needHScroll = contentW > viewW + 1;
    viewport.setScrollBarsShown(false, needHScroll);
    // Windowed rows: the view is only as tall as the viewport, so leaving the
    // plugin does not snapshot a multi-million-pixel list.
    content.setSize(contentW, juce::jmax(1, viewport.getMaximumVisibleHeight()));
    syncRowScroll();
}

bool FileListPanel::isColumnVisible(SortColumn col) const
{
    switch (col)
    {
        case SortColumn::Name:       return true;
        case SortColumn::Key:        return columnsVisible.key;
        case SortColumn::Tempo:      return columnsVisible.tempo;
        case SortColumn::Bars:       return columnsVisible.bars;
        case SortColumn::Kind:       return columnsVisible.kind;
        case SortColumn::Complexity: return columnsVisible.complexity;
        case SortColumn::DifNotes:   return columnsVisible.difNotes;
        case SortColumn::TimeSig:    return columnsVisible.timeSig;
        case SortColumn::Notes:      return columnsVisible.notes;
    }
    return false;
}

int FileListPanel::totalContentWidth() const
{
    int w = metrics::scaled(8) * 2 + metrics::scaled(nameColumnW) + metrics::scaled(6);
    if (columnsVisible.key)        w += metrics::scaled(kKeyW);
    if (columnsVisible.tempo)      w += metrics::scaled(kTempoW);
    if (columnsVisible.bars)       w += metrics::scaled(kBarsW);
    if (columnsVisible.kind)       w += metrics::scaled(kKindW);
    if (columnsVisible.complexity) w += metrics::scaled(kComplexityW);
    if (columnsVisible.difNotes)   w += metrics::scaled(kDifNotesW);
    if (columnsVisible.timeSig)    w += metrics::scaled(kTimeSigW);
    if (columnsVisible.notes)      w += metrics::scaled(kNotesW);
    return w;
}

void FileListPanel::setNameColumnWidth(int logicalW)
{
    const int next = juce::jlimit(kNameWMin, kNameWMax, logicalW);
    if (next == nameColumnW)
        return;
    nameColumnW = next;
    updateContentSize();
    content.repaint();
    repaint();
}

void FileListPanel::fitNameColumnToContents()
{
    const float scale = juce::jmax(0.25f, contentScale());
    const auto font = ds::font(ds::Type::Body);
    float maxText = juce::GlyphArrangement::getStringWidth(font, "Name");
    // Huge folders: measuring every name stalls the UI; sample + keep a wide default.
    constexpr int kMeasureCap = 400;
    if ((int) entries.size() > kMeasureCap)
    {
        const int step = juce::jmax(1, (int) entries.size() / kMeasureCap);
        for (int i = 0; i < (int) entries.size(); i += step)
            if (entries[(size_t) i].name.isNotEmpty())
                maxText = juce::jmax(maxText,
                                     juce::GlyphArrangement::getStringWidth(font, entries[(size_t) i].name));
        maxText = juce::jmax(maxText, 220.0f);
    }
    else
    {
        for (const auto& e : entries)
            if (e.name.isNotEmpty())
                maxText = juce::jmax(maxText,
                                     juce::GlyphArrangement::getStringWidth(font, e.name));
    }

    // Star/folder icon (18) + gap (6) + trailing pad so glyphs aren't clipped.
    const int pixelW = 18 + 6 + (int) std::ceil(maxText) + 16;
    const int logical = juce::jlimit(kNameWMin, kNameWMax,
                                     juce::roundToInt((float) pixelW / scale));
    if (logical == nameColumnW)
        return;

    nameColumnW = logical;
    updateContentSize();
    content.repaint();
    repaint();
    if (onNameColumnWidthChanged)
        onNameColumnWidthChanged(nameColumnW);
}

void FileListPanel::setColumnVisibility(const BrowserColumnVisibility& v)
{
    columnsVisible = v;
    if (!isColumnVisible(sortColumn))
    {
        sortColumn = SortColumn::Name;
        sortAscending = true;
        rebuildSortOrder();
    }
    updateContentSize();
    content.repaint();
    repaint();
}

int FileListPanel::nameResizeHandleX() const
{
    return headerColumnBounds(SortColumn::Name).getRight();
}

int FileListPanel::nameColumnRightContentX() const
{
    const auto cols = splitRowColumns({ 0, 0, juce::jmax(getWidth(), totalContentWidth()), 1 });
    return cols.name.getRight();
}

bool FileListPanel::hitNameResizeHandle(juce::Point<int> pos) const
{
    if (pos.y < 0 || pos.y >= getHeight())
        return false;
    return std::abs(pos.x - nameResizeHandleX()) <= kResizeHitSlop;
}

bool FileListPanel::hitNameResizeHandleContent(juce::Point<int> pos) const
{
    return std::abs(pos.x - nameColumnRightContentX()) <= kResizeHitSlop;
}

void FileListPanel::beginNameColumnResize(int screenX)
{
    resizingNameColumn = true;
    nameResizeStartScreenX = screenX;
    nameResizeStartW = nameColumnW;
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    content.setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    repaint();
}

void FileListPanel::dragNameColumnResize(int screenX)
{
    if (!resizingNameColumn)
        return;
    const float scale = juce::jmax(0.25f, contentScale());
    const int deltaLogical = juce::roundToInt((float) (screenX - nameResizeStartScreenX) / scale);
    setNameColumnWidth(nameResizeStartW + deltaLogical);
}

void FileListPanel::endNameColumnResize()
{
    if (!resizingNameColumn)
        return;
    resizingNameColumn = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    content.setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
    if (onNameColumnWidthChanged)
        onNameColumnWidthChanged(nameColumnW);
}

void FileListPanel::paintNameColumnDivider(juce::Graphics& g, int x, int y, int h) const
{
    g.setColour(resizingNameColumn ? ds::acc() : ds::line());
    g.fillRect(x, y, 1, h);
    // Wider invisible-feeling rail so the edge reads as a splitter.
    if (resizingNameColumn)
    {
        g.setColour(ds::acc().withAlpha(0.12f));
        g.fillRect(x - 2, y, 5, h);
    }
}

void FileListPanel::showColumnVisibilityMenu()
{
    juce::PopupMenu m;
    auto addToggle = [&](int id, const juce::String& label, bool on)
    {
        m.addItem(id, label, true, on);
    };
    addToggle(1, "Key", columnsVisible.key);
    addToggle(2, "Tempo", columnsVisible.tempo);
    addToggle(3, "Bars", columnsVisible.bars);
    addToggle(4, "Kind", columnsVisible.kind);
    addToggle(5, "Complexity", columnsVisible.complexity);
    addToggle(6, "DifNotes", columnsVisible.difNotes);
    addToggle(7, "TimeSig", columnsVisible.timeSig);
    addToggle(8, "Notes", columnsVisible.notes);

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                         .withMousePosition(),
        [this](int result)
        {
            if (result == 0) return;
            auto v = columnsVisible;
            switch (result)
            {
                case 1: v.key = !v.key; break;
                case 2: v.tempo = !v.tempo; break;
                case 3: v.bars = !v.bars; break;
                case 4: v.kind = !v.kind; break;
                case 5: v.complexity = !v.complexity; break;
                case 6: v.difNotes = !v.difNotes; break;
                case 7: v.timeSig = !v.timeSig; break;
                case 8: v.notes = !v.notes; break;
                default: return;
            }
            setColumnVisibility(v);
            if (onColumnVisibilityChanged)
                onColumnVisibilityChanged(v);
        });
}

juce::Rectangle<int> FileListPanel::headerColumnBounds(SortColumn col) const
{
    auto row = getLocalBounds().removeFromTop(metrics::listHeaderH());
    row.setX(row.getX() - viewport.getViewPositionX());
    row.setWidth(juce::jmax(getWidth(), totalContentWidth()));
    const auto c = splitRowColumns(row);
    switch (col)
    {
        case SortColumn::Name:       return c.name;
        case SortColumn::Key:        return c.key;
        case SortColumn::Tempo:      return c.tempo;
        case SortColumn::Bars:       return c.bars;
        case SortColumn::Kind:       return c.kind;
        case SortColumn::Complexity: return c.complexity;
        case SortColumn::DifNotes:   return c.difNotes;
        case SortColumn::TimeSig:    return c.timeSig;
        case SortColumn::Notes:      return c.notes;
    }
    return {};
}

FileListPanel::ColumnRects FileListPanel::splitRowColumns(juce::Rectangle<int> row) const
{
    ColumnRects cols;
    row = row.reduced(metrics::scaled(8), 0);
    cols.name = row.removeFromLeft(metrics::scaled(nameColumnW));
    // Groove: a 6 px gutter keeps the key right beside the name.
    row.removeFromLeft(metrics::scaled(6));
    if (columnsVisible.key)
        cols.key = row.removeFromLeft(metrics::scaled(kKeyW));
    if (columnsVisible.tempo)
        cols.tempo = row.removeFromLeft(metrics::scaled(kTempoW));
    if (columnsVisible.bars)
        cols.bars = row.removeFromLeft(metrics::scaled(kBarsW));
    if (columnsVisible.kind)
        cols.kind = row.removeFromLeft(metrics::scaled(kKindW));
    if (columnsVisible.complexity)
        cols.complexity = row.removeFromLeft(metrics::scaled(kComplexityW));
    if (columnsVisible.difNotes)
        cols.difNotes = row.removeFromLeft(metrics::scaled(kDifNotesW));
    if (columnsVisible.timeSig)
        cols.timeSig = row.removeFromLeft(metrics::scaled(kTimeSigW));
    if (columnsVisible.notes)
        cols.notes = row.removeFromLeft(metrics::scaled(kNotesW));
    return cols;
}

void FileListPanel::timerCallback()
{
    updateScrollTopButton();

    // Keep column headers aligned with horizontally scrolled row content.
    const int viewX = viewport.getViewPositionX();
    if (viewX != lastViewX)
    {
        lastViewX = viewX;
        repaint(getLocalBounds().removeFromTop(metrics::listHeaderH()));
    }

    if (searching)
    {
        searchAnimT = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        repaint();
        return;
    }
    if (playing && selected >= 0)
    {
        const int local = entryToDisplay(selected) - scrollRow;
        if (local >= 0)
            content.repaint(0, local * metrics::listRowH(),
                            content.getWidth(), metrics::listRowH());
    }
}

void FileListPanel::resized()
{
    auto list = getLocalBounds().withTrimmedTop(metrics::listHeaderH());
    const int rowH = juce::jmax(1, metrics::listRowH());
    const int vis = juce::jmax(1, list.getHeight() / rowH);
    const bool needV = !searching && (int) sortOrder.size() > vis;
    if (needV)
        rowScroll.setBounds(list.removeFromRight(8));
    else
        rowScroll.setBounds({});
    rowScroll.setVisible(needV);
    viewport.setBounds(list);
    updateContentSize();
    setScrollRow(scrollRow);
    updateScrollTopButton();
}

void FileListPanel::paint(juce::Graphics& g)
{
    g.fillAll(ds::surface100());
    paintColumnHeader(g);

    if (searching)
    {
        auto area = getLocalBounds().withTrimmedTop(metrics::listHeaderH());
        g.setColour(colours::panel().withMultipliedBrightness(0.92f));
        g.fillRect(area);

        auto grouped = [](int n)
        {
            auto s = juce::String(juce::jmax(0, n));
            for (int i = s.length() - 3; i > 0; i -= 3)
                s = s.substring(0, i) + "," + s.substring(i);
            return s;
        };

        auto block = area.withSizeKeepingCentre(juce::jmin(320, area.getWidth() - 48), 88);
        // uiFontFixed avoids missing-glyph boxes from the scaled display face.
        g.setFont(uiFontFixed(11.0f, true));
        g.setColour(colours::text2());
        g.drawText("SEARCHING", block.removeFromTop(16), juce::Justification::centred, false);
        block.removeFromTop(4);

        juce::String count;
        if (scanTotal > 0)
            count = grouped(scanDone) + "  of  " + grouped(scanTotal);
        else
        {
            const int dots = 1 + (int) std::fmod(searchAnimT * 2.0, 3.0);
            count = "Reading files" + juce::String::repeatedString(".", dots);
        }
        g.setFont(uiFontFixed(20.0f, true));
        g.setColour(colours::text());
        g.drawText(count, block.removeFromTop(28), juce::Justification::centred, false);
        block.removeFromTop(10);

        if (scanTotal > 0)
        {
            const int barW = juce::jmin(220, block.getWidth());
            auto bar = juce::Rectangle<int>(barW, 3).withCentre(
                { area.getCentreX(), block.getY() + 2 });
            g.setColour(colours::line());
            g.fillRoundedRectangle(bar.toFloat(), 1.5f);
            const float frac = juce::jlimit(0.0f, 1.0f,
                                             (float) scanDone / (float) juce::jmax(1, scanTotal));
            auto fill = bar;
            fill.setWidth(juce::jmax(3, (int) std::lround((float) barW * frac)));
            g.setColour(colours::accent());
            g.fillRoundedRectangle(fill.toFloat(), 1.5f);
        }
        return;
    }

}

void FileListPanel::mouseDown(const juce::MouseEvent& e)
{
    grabBrowseFocus();
    if (onActivated)
        onActivated();

    if (e.y >= metrics::listHeaderH())
        return;

    if (e.mods.isPopupMenu())
    {
        showColumnVisibilityMenu();
        return;
    }

    if (hitNameResizeHandle(e.getPosition()))
    {
        beginNameColumnResize(e.getScreenX());
        return;
    }

    const SortColumn allCols[] = {
        SortColumn::Name, SortColumn::Key, SortColumn::Tempo, SortColumn::Bars,
        SortColumn::Kind, SortColumn::Complexity, SortColumn::DifNotes,
        SortColumn::TimeSig, SortColumn::Notes
    };
    for (auto col : allCols)
    {
        if (!isColumnVisible(col))
            continue;
        if (headerColumnBounds(col).contains(e.getPosition()))
        {
            if (sortColumn == col)
                sortAscending = !sortAscending;
            else
            {
                sortColumn = col;
                sortAscending = true;
            }
            rebuildSortOrder();
            content.repaint();
            repaint();
            return;
        }
    }
}

void FileListPanel::mouseDrag(const juce::MouseEvent& e)
{
    dragNameColumnResize(e.getScreenX());
}

void FileListPanel::mouseUp(const juce::MouseEvent&)
{
    endNameColumnResize();
}

void FileListPanel::mouseMove(const juce::MouseEvent& e)
{
    if (resizingNameColumn)
        return;
    setMouseCursor(hitNameResizeHandle(e.getPosition())
                       ? juce::MouseCursor::LeftRightResizeCursor
                       : juce::MouseCursor::NormalCursor);
}

void FileListPanel::mouseExit(const juce::MouseEvent&)
{
    if (!resizingNameColumn)
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void FileListPanel::paintColumnHeader(juce::Graphics& g)
{
    auto headerClip = getLocalBounds().removeFromTop(metrics::listHeaderH());
    g.setColour(colours::line());
    g.fillRect(headerClip.removeFromBottom(1));

    // Paint headers in content coordinates, shifted by the viewport's X scroll
    // so titles stay locked to their columns while rows scroll sideways.
    const int viewX = viewport.getViewPositionX();
    g.saveState();
    g.reduceClipRegion(getLocalBounds().removeFromTop(metrics::listHeaderH()));
    g.addTransform(juce::AffineTransform::translation((float) -viewX, 0.0f));

    auto drawCol = [&](SortColumn col, const juce::String& label)
    {
        if (!isColumnVisible(col))
            return;
        // Bounds in panel space already account for viewX; undo that for content paint.
        auto r = headerColumnBounds(col).translated(viewX, 0);
        const bool active = sortColumn == col;
        g.setColour(ds::colour(ds::Type::TableHeader));
        g.setFont(ds::font(ds::Type::TableHeader));
        const auto font = g.getCurrentFont();
        const auto caps = label.toUpperCase(); // Levels table header: mono caps
        const int labelW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(font, caps));
        auto labelR = r.removeFromLeft(labelW);
        g.drawText(caps, labelR, juce::Justification::centredLeft, false);
        if (active)
        {
            auto arrowR = r.removeFromLeft(metrics::scaled(kSortArrowW)).toFloat()
                              .withSizeKeepingCentre(8.0f, 8.0f);
            juce::Path p;
            const float cx = arrowR.getCentreX();
            const float cy = arrowR.getCentreY();
            if (sortAscending)
                p.addTriangle(cx - 3.5f, cy + 2.0f, cx + 3.5f, cy + 2.0f, cx, cy - 2.5f);
            else
                p.addTriangle(cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
            g.setColour(ds::inkMuted());
            g.fillPath(p);
        }
    };

    drawCol(SortColumn::Name, "Name");
    drawCol(SortColumn::Key, "Key");
    drawCol(SortColumn::Tempo, "BPM");
    drawCol(SortColumn::Bars, "Bars");
    drawCol(SortColumn::Kind, "Kind");
    drawCol(SortColumn::Complexity, "Cx");
    drawCol(SortColumn::DifNotes, "DifNotes");
    drawCol(SortColumn::TimeSig, "TimeSig");
    drawCol(SortColumn::Notes, "Notes");

    // Name-column resize rail (content coordinates; transform already applied).
    const int divX = splitRowColumns({ 0, 0, juce::jmax(getWidth(), totalContentWidth()),
                                       metrics::listHeaderH() }).name.getRight();
    paintNameColumnDivider(g, divX, 0, metrics::listHeaderH());
    g.restoreState();
}

void FileListPanel::paintRow(juce::Graphics& g, int displayIdx, juce::Rectangle<int> r,
                             bool hovered, bool hoverStar)
{
    const int entryIdx = displayToEntry(displayIdx);
    if (entryIdx < 0) return;
    const auto& e = entries[(size_t) entryIdx];
    const bool isSelected = entryIdx == selected;

    // Groove list row: edge to edge, 1px line beneath. Selected = accent fill, on-accent text.
    // Folder rows get a slightly cooler wash so they read as containers.
    if (isSelected)
    {
        // Levels selected row (LvApp): inset rounded violet tint; a 3x12 success
        // tick marks the clip being previewed. Text stays ink (600).
        auto sel = r.toFloat().reduced(4.0f, 1.0f);
        g.setColour(ds::selsoft());
        g.fillRoundedRectangle(sel, ds::radius::sm);
        g.setColour(ds::play());
        g.fillRoundedRectangle(sel.getX() + 2.0f, sel.getCentreY() - 6.0f, 3.0f, 12.0f, 1.5f);
    }
    else if (e.isDirectory)
    {
        g.setColour(hovered ? ds::surface300() : ds::surface200());
        g.fillRect(r);
    }
    else if (hovered)
    {
        g.setColour(ds::surface200());
        g.fillRect(r);
    }
    else if (tweaks().alternateRows.load() != 0 && (displayIdx % 2) == 1)
    {
        // Stronger zebra so rows read clearly at a glance.
        g.setColour(ds::surface300());
        g.fillRect(r);
    }
    g.setColour(ds::line());
    g.fillRect(r.getX(), r.getBottom() - 1, r.getWidth(), 1);

    const auto textCol = ds::ink();
    const auto metaCol = isSelected ? ds::ink() : ds::inkMuted();
    const auto mutedCol = ds::inkMuted();

    auto cols = splitRowColumns(r);
    auto row = cols.name;

    // Larger star so filled vs outline reads clearly at a glance.
    constexpr float kStarGlyph = 14.0f;
    auto iconArea = row.removeFromLeft(metrics::scaled(kIconW)).toFloat()
                        .withSizeKeepingCentre(kStarGlyph, kStarGlyph);
    // Favorite star for files and folders — filled stars are gold.
    {
        const auto starCol = e.starred ? favoriteGold()
                                       : (hoverStar ? textCol : ds::inkMuted());
        drawIcon(g, e.starred ? icons::star : icons::starOutline, iconArea, starCol,
                 e.starred ? 1.6f : 1.35f);
    }

    const auto nameFont = uiFontFixed(12.0f, isSelected); // Levels 12, 600 when selected
    if (e.isDirectory)
    {
        auto folderArea = row.removeFromLeft(metrics::scaled(kIconW)).toFloat()
                              .withSizeKeepingCentre(13.0f, 13.0f);
        drawIcon(g, icons::folder, folderArea, ds::inkMuted(), 1.35f);
        g.setColour(textCol);
        g.setFont(nameFont);
        g.drawText(e.name, row, juce::Justification::centredLeft, true);
        return;
    }

    g.setFont(ds::font(ds::Type::Metadata));
    g.setColour(metaCol);
    if (columnsVisible.key && e.rootName.isNotEmpty())
        g.drawText(e.rootName, cols.key, juce::Justification::centredLeft);
    if (columnsVisible.tempo && e.bpm > 0.0)
        g.drawText(juce::String((int) std::lround(e.bpm)), cols.tempo, juce::Justification::centredLeft);
    if (columnsVisible.bars && e.bars > 0)
        g.drawText(juce::String(e.bars), cols.bars, juce::Justification::centredLeft);
    if (columnsVisible.complexity && e.complexity > 0)
        g.drawText(juce::String(e.complexity), cols.complexity, juce::Justification::centredLeft);
    if (columnsVisible.difNotes && e.difNotes > 0)
        g.drawText(juce::String(e.difNotes), cols.difNotes, juce::Justification::centredLeft);
    if (columnsVisible.timeSig)
        g.drawText(juce::String(e.timeSigNum) + "/" + juce::String(e.timeSigDen),
                   cols.timeSig, juce::Justification::centredLeft);
    if (columnsVisible.notes && e.noteCount > 0)
        g.drawText(juce::String(e.noteCount), cols.notes, juce::Justification::centredLeft);
    if (columnsVisible.kind)
    {
        // Kind is a plain word in its own column.
        g.setColour(mutedCol);
        g.setFont(uiFontFixed(12.0f, false));
        g.drawText(clipKindName(e.kind), cols.kind, juce::Justification::centredLeft, true);
    }

    g.setColour(textCol);
    g.setFont(nameFont);
    g.drawText(e.name, row, juce::Justification::centredLeft, true);
}

void FileListPanel::ListContent::paint(juce::Graphics& g)
{
    if (owner.entries.empty())
    {
        g.setColour(ds::inkMuted());
        g.setFont(uiFontFixed(12.0f, false));
        g.drawText(owner.emptyHint, getLocalBounds().reduced(24, 32),
                   juce::Justification::centred);
        return;
    }

    const int rowH = metrics::listRowH();
    const int count = (int) owner.sortOrder.size();
    int y = 0;
    for (int i = owner.scrollRow; i < count && y < getHeight(); ++i, y += rowH)
        owner.paintRow(g, i, { 0, y, getWidth(), rowH }, i == hoverRow,
                       i == hoverRow && hoverStar);

    if (owner.resizingNameColumn)
        owner.paintNameColumnDivider(g, owner.nameColumnRightContentX(), 0, getHeight());
}

void FileListPanel::ListContent::mouseMove(const juce::MouseEvent& e)
{
    const auto local = getLocalPoint(nullptr, e.getScreenPosition());
    if (owner.hitNameResizeHandleContent(local))
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        if (hoverRow >= 0 || hoverStar)
        {
            hoverRow = -1;
            hoverStar = false;
            repaint();
        }
        return;
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);

    const int rowH = metrics::listRowH();
    // Screen→local avoids AffineTransform/content-scale drift in nested Viewport coords.
    const int disp = owner.displayIndexAtContentY(local.y);
    const bool valid = disp >= 0;
    const int entryIdx = valid ? owner.displayToEntry(disp) : -1;
    const bool isDir = entryIdx >= 0 && owner.entries[(size_t) entryIdx].isDirectory;
    // Star sits in the leading icon slot of the name column.
    const auto cols = owner.splitRowColumns({ 0, 0, getWidth(), rowH });
    const int starLeft = cols.name.getX();
    const int x = local.x;
    const int starHit = metrics::scaled(FileListPanel::kIconW) + 4;
    juce::ignoreUnused(isDir);
    const bool newHoverStar = valid && x >= starLeft && x < starLeft + starHit;

    if (disp != hoverRow || newHoverStar != hoverStar)
    {
        hoverRow = valid ? disp : -1;
        hoverStar = newHoverStar;
        repaint();
    }
}

void FileListPanel::ListContent::mouseExit(const juce::MouseEvent&)
{
    hoverRow = -1;
    hoverStar = false;
    if (!owner.resizingNameColumn)
        setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void FileListPanel::ListContent::mouseDown(const juce::MouseEvent& e)
{
    owner.grabBrowseFocus();
    if (owner.onActivated)
        owner.onActivated();

    const auto local = getLocalPoint(nullptr, e.getScreenPosition());
    if (owner.hitNameResizeHandleContent(local))
    {
        owner.beginNameColumnResize(e.getScreenX());
        return;
    }

    if (owner.entries.empty())
    {
        if (owner.onEmptyOpenFolder)
            owner.onEmptyOpenFolder();
        return;
    }

    // Column header clicks are handled by the parent panel.
    if (local.y < 0) return;

    const int rowH = metrics::listRowH();
    const int disp = owner.displayIndexAtContentY(local.y);
    if (disp < 0)
        return;

    const int entryIdx = owner.displayToEntry(disp);
    if (entryIdx < 0) return;
    const auto& entry = owner.entries[(size_t) entryIdx];
    const auto cols = owner.splitRowColumns({ 0, 0, getWidth(), rowH });
    const int starLeft = cols.name.getX();

    if (e.mods.isPopupMenu() && !entry.isDirectory && entry.file.existsAsFile())
    {
        juce::PopupMenu m;
        juce::StringArray locs = entry.locations;
        if (locs.isEmpty())
            locs.add(entry.file.getFullPathName());

        if (locs.size() <= 1)
        {
            m.addItem(1, "Show in Finder");
        }
        else
        {
            for (int i = 0; i < locs.size(); ++i)
                m.addItem(100 + i, "Show in Finder Location " + juce::String(i + 1));
        }
        m.addItem(2, "Copy to Folder...");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                             .withMousePosition(),
            [this, locs, file = entry.file](int result)
            {
                if (result == 1 && owner.onRevealFile)
                    owner.onRevealFile(file);
                else if (result >= 100 && result < 100 + locs.size() && owner.onRevealFile)
                    owner.onRevealFile(juce::File(locs[result - 100]));
                else if (result == 2 && owner.onCopyFileToFolder)
                    owner.onCopyFileToFolder(file);
            });
        return;
    }

    // Pass entry indices so the editor can index displayRows / clips correctly
    // even when the list is re-ordered by column sort.
    const int starHit = metrics::scaled(FileListPanel::kIconW) + 4;
    if (local.x >= starLeft && local.x < starLeft + starHit)
    {
        if (owner.onToggleStar) owner.onToggleStar(entryIdx);
        return;
    }

    owner.setSelectedIndex(entryIdx);
    if (!entry.isDirectory)
    {
        dragSourcePath = entry.file.getFullPathName();
        if (dragSourcePath.isEmpty())
            clearDragState();
    }
    else
        clearDragState();

    if (entry.isDirectory && e.getNumberOfClicks() > 1 && owner.onEnterFolder)
        owner.onEnterFolder(entryIdx);
    else if (!entry.isDirectory && e.getNumberOfClicks() > 1 && owner.onOpenInPianoRoll)
        owner.onOpenInPianoRoll(entryIdx);
}

void FileListPanel::ListContent::mouseWheelMove(const juce::MouseEvent&,
                                                const juce::MouseWheelDetails& wheel)
{
    if (std::abs(wheel.deltaX) > std::abs(wheel.deltaY) + 0.01f)
    {
        const int dx = (int) std::lround(-wheel.deltaX * 48.0);
        owner.viewport.setViewPosition(owner.viewport.getViewPositionX() + dx, 0);
        return;
    }
    const int rows = (int) std::lround(-wheel.deltaY * (wheel.isSmooth ? 10.0 : 4.0));
    if (rows != 0)
        owner.setScrollRow(owner.scrollRow + rows);
}

void FileListPanel::ListContent::clearDragState()
{
    dragSourcePath.clear();
}

void FileListPanel::ListContent::mouseUp(const juce::MouseEvent&)
{
    if (owner.resizingNameColumn)
    {
        owner.endNameColumnResize();
        return;
    }
    clearDragState();
}

void FileListPanel::ListContent::mouseDrag(const juce::MouseEvent& e)
{
    if (owner.resizingNameColumn)
    {
        owner.dragNameColumnResize(e.getScreenX());
        return;
    }

    if (dragSourcePath.isEmpty() || e.getDistanceFromDragStart() < 8)
        return;

    const juce::File file(dragSourcePath);
    clearDragState();

    if (!file.getFullPathName().isNotEmpty() || !file.existsAsFile())
        return;

    if (owner.onDragFile)
        owner.onDragFile(file);
}

// ── BrowserToolbar ───────────────────────────────────────────────────────────

BrowserToolbar::BrowserToolbar()
{
    auto prep = [](IconBtn& b)
    {
        b.ghost = false;
        b.setWantsKeyboardFocus(true);
    };
    btnSubfolders.setComponentID("btnSubfolders");
    prep(btnSubfolders);
    btnSubfolders.onClick = [this] { if (onToggleSubfolders) onToggleSubfolders(); };
    addAndMakeVisible(btnSubfolders);

    btnGrid.setComponentID("btnGrid");
    prep(btnGrid);
    btnGrid.onClick = [this] { if (onToggleGrid) onToggleGrid(); };
    addAndMakeVisible(btnGrid);

    btnFilters.setComponentID("btnFilters");
    prep(btnFilters);
    // Levels (LvApp): outlined Filters button; active = violet border + 12 % tint,
    // ink label, primary count badge.
    btnFilters.caption = "Filters";
    btnFilters.solid = false;
    btnFilters.accentSolid = false;
    btnFilters.outlined = true;
    btnFilters.ghost = false;
    btnFilters.onClick = [this] { if (onToggleFilters) onToggleFilters(); };
    addAndMakeVisible(btnFilters);
}

int BrowserToolbar::idealHeight() const
{
    // Mockup: title row + gap + slim search row + matching bottom pad
    // (+ filter summary strip when active).
    const int titleH = titleText.isNotEmpty() ? metrics::scaled(28) : 0;
    const int searchH = metrics::scaled(28);
    const int gap = metrics::scaled(12); // title↔search and below search
    const int stripH = filtersActiveCount > 0 ? metrics::scaled(22) : 0;
    const int titleGap = titleText.isNotEmpty() ? gap : 0;
    return titleH + titleGap + searchH + gap + stripH;
}

juce::String BrowserToolbar::filterStripText() const
{
    auto s = filtersSummary;
    if (s.startsWithIgnoreCase("Filters"))
        s = s.fromFirstOccurrenceOf("\n", false, false).trim();
    juce::StringArray parts;
    parts.addTokens(s, "\n", "");
    parts.trim();
    parts.removeEmptyStrings();
    return parts.joinIntoString(" · ");
}

void BrowserToolbar::setSearchComponent(juce::Component* c)
{
    search = c;
    if (search != nullptr)
        addAndMakeVisible(*search);
    resized();
}

void BrowserToolbar::setTitle(const juce::String& t)
{
    if (titleText == t) return;
    titleText = t;
    resized();
    repaint();
}

void BrowserToolbar::setCount(int shown, int total)
{
    if (shownCount == shown && totalCount == total) return;
    shownCount = shown;
    totalCount = total;
    resized();
    repaint();
}

void BrowserToolbar::setSaved(bool, bool) {}

void BrowserToolbar::setGrid(bool on)
{
    btnGrid.active = on;
    btnGrid.repaint();
}

void BrowserToolbar::setSubfolders(bool on)
{
    btnSubfolders.active = on;
    btnSubfolders.repaint();
}

void BrowserToolbar::setFiltersActive(bool open, int activeCount, const juce::String& summary)
{
    filtersOpen = open;
    filtersActiveCount = activeCount;
    filtersSummary = summary;
    // Levels: violet active chip whenever filters are on or the popover is open.
    btnFilters.active = open || activeCount > 0;
    btnFilters.outlined = !btnFilters.active;
    btnFilters.caption = "Filters";
    if (!open && activeCount > 0 && summary.isNotEmpty())
        btnFilters.setTooltip(summary);
    else if (activeCount > 0)
        btnFilters.setTooltip("Filters (" + juce::String(activeCount) + " active)");
    else
        btnFilters.setTooltip("Filters");
    btnFilters.repaint();
    resized();
    repaint();
}

void BrowserToolbar::resized()
{
    auto area = getLocalBounds();
    const int ctl = metrics::scaled(28); // Levels search/Filters height
    const int gap = metrics::scaled(6);
    const int pad = metrics::scaled(12);
    btnSubfolders.setVisible(false);
    btnGrid.setVisible(false);
    btnSubfolders.setBounds({});
    btnGrid.setBounds({});
    titleBounds = countBounds = filtersBadgeBounds = filterStripBounds = resetBounds = {};

    if (titleText.isNotEmpty())
    {
        titleBounds = area.removeFromTop(metrics::scaled(28)).reduced(pad, 0);
        area.removeFromTop(pad); // same gap as bottom pad under search
    }

    auto searchRow = area.removeFromTop(metrics::scaled(28)).reduced(pad, 0);
    int filtersW = btnFilters.captionWidth();
    int badgeW = 0;
    if (filtersActiveCount > 0)
    {
        const juce::String n = juce::String(filtersActiveCount);
        const auto f = monoFontFixed(11.0f, true);
        badgeW = juce::jmax(16, (int) std::ceil(juce::GlyphArrangement::getStringWidth(f, n)) + 6);
        filtersW += badgeW + metrics::scaled(4); // room for white count pill inside chip
    }
    btnFilters.setBounds(searchRow.removeFromRight(filtersW).withSizeKeepingCentre(filtersW, ctl));
    if (badgeW > 0)
        filtersBadgeBounds = { btnFilters.getRight() - badgeW - 4,
                               btnFilters.getY() + (btnFilters.getHeight() - 16) / 2, badgeW, 16 };
    searchRow.removeFromRight(gap);
    if (search != nullptr)
        search->setBounds(searchRow.withSizeKeepingCentre(searchRow.getWidth(), ctl));

    area.removeFromTop(pad); // bottom pad under search (= title↔search gap)

    if (filtersActiveCount > 0)
    {
        auto strip = area.removeFromTop(metrics::scaled(22)).reduced(pad, 0);
        const auto resetFont = uiFontFixed(11.0f, false);
        const int resetW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(resetFont, "Reset")) + 4;
        resetBounds = strip.removeFromRight(resetW);
        strip.removeFromRight(metrics::scaled(8));
        filterStripBounds = strip;
    }
}

void BrowserToolbar::mouseDown(const juce::MouseEvent& e)
{
    if (resetBounds.contains(e.getPosition()) && onResetFilters)
        onResetFilters();
}

void BrowserToolbar::mouseMove(const juce::MouseEvent& e)
{
    const bool over = resetBounds.contains(e.getPosition());
    if (over != resetHover)
    {
        resetHover = over;
        setMouseCursor(over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint(resetBounds.expanded(2));
    }
}

void BrowserToolbar::mouseExit(const juce::MouseEvent&)
{
    if (!resetHover) return;
    resetHover = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint(resetBounds.expanded(2));
}

void BrowserToolbar::paint(juce::Graphics& g)
{
    g.setColour(ds::bg());
    g.fillRect(getLocalBounds());
    g.setColour(ds::line());
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    if (!titleBounds.isEmpty() && titleText.isNotEmpty())
    {
        // Mockup: 16 pt title, muted "N of M" immediately after (no pill).
        const auto titleFont = uiFontFixed(14.0f, true); // Levels: 14 / 600
        g.setFont(titleFont);
        g.setColour(ds::ink());
        const float titleW = juce::GlyphArrangement::getStringWidth(titleFont, titleText);
        auto titleDraw = titleBounds;
        g.drawText(titleText, titleDraw.removeFromLeft((int) std::ceil(titleW) + 2),
                   juce::Justification::centredLeft, false);

        juce::String countTxt;
        if (totalCount > 0)
            countTxt = shownCount == totalCount ? juce::String(totalCount)
                                                : juce::String(shownCount) + " of " + juce::String(totalCount);
        else if (shownCount > 0)
            countTxt = juce::String(shownCount);
        if (countTxt.isNotEmpty())
        {
            titleDraw.removeFromLeft(metrics::scaled(8));
            g.setColour(ds::inkMuted());
            g.setFont(monoFontFixed(12.0f));
            g.drawText(countTxt, titleDraw, juce::Justification::centredLeft, true);
        }
    }

    if (!filtersBadgeBounds.isEmpty() && filtersActiveCount > 0)
    {
        auto r = filtersBadgeBounds.toFloat();
        g.setColour(ds::primary());
        g.fillEllipse(r);
        g.setColour(ds::onPrimary());
        g.setFont(monoFontFixed(11.0f, true));
        g.drawText(juce::String(filtersActiveCount), filtersBadgeBounds, juce::Justification::centred, false);
    }

    if (filtersActiveCount > 0)
    {
        const auto strip = filterStripText();
        if (strip.isNotEmpty() && !filterStripBounds.isEmpty())
        {
            g.setColour(ds::inkMuted());
            g.setFont(uiFontFixed(11.0f, false));
            g.drawText(strip, filterStripBounds, juce::Justification::centredLeft, true);
        }
        if (!resetBounds.isEmpty())
        {
            g.setColour(resetHover ? ds::ink() : ds::inkMuted());
            g.setFont(uiFontFixed(11.0f, false));
            g.drawText("Reset", resetBounds, juce::Justification::centredRight, false);
            // Underline like the mockup text link.
            const float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), "Reset");
            const float x1 = (float) resetBounds.getRight() - tw;
            const float y = (float) resetBounds.getCentreY() + 6.0f;
            g.drawLine(x1, y, (float) resetBounds.getRight(), y, 1.0f);
        }
    }
}

// ── FilterPopover ────────────────────────────────────────────────────────────

FilterPopover::FilterPopover()
{
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::NormalCursor);
    bodyViewport.setScrollBarsShown(true, false);
    bodyViewport.setScrollBarThickness(metrics::scaled(8));
    addAndMakeVisible(bodyViewport);
}

void FilterPopover::setContent(juce::Component* p, std::function<int()> idealPanelHeight)
{
    panel = p;
    panelHeight = std::move(idealPanelHeight);
    if (panel != nullptr)
        bodyViewport.setViewedComponent(panel, false);
    resized();
}

void FilterPopover::setCountText(const juce::String& t)
{
    if (countText == t) return;
    countText = t;
    repaint();
}

void FilterPopover::setMaxBodyHeight(int h)
{
    maxBodyH = juce::jmax(0, h);
}

int FilterPopover::idealHeight() const
{
    // Levels: 40 header + 12 body pad + panel + 12 pad + 40 footer (count + Apply).
    const int header = metrics::scaled(40);
    const int footer = metrics::scaled(40);
    const int pad = metrics::scaled(12);
    const int bodyIdeal = panelHeight ? panelHeight() : 0;
    const int body = maxBodyH > 0 ? juce::jmin(bodyIdeal, maxBodyH) : bodyIdeal;
    const int inset = themeDepth() == ThemeDepth::Brutal
                          ? 2 * (int) std::ceil(juce::jmax(1.5f, themeBorderPx()) + 1.0f)
                          : 0;
    return header + pad + body + pad + footer + inset;
}

void FilterPopover::resized()
{
    const int inset = themeDepth() == ThemeDepth::Brutal
                          ? (int) std::ceil(juce::jmax(1.5f, themeBorderPx()) + 1.0f)
                          : 0;
    auto r = getLocalBounds().reduced(inset);
    auto header = r.removeFromTop(metrics::scaled(40)).reduced(metrics::scaled(12), 0);
    const int resetH = metrics::scaled(24);
    const int resetW = metrics::scaled(50);
    resetBounds = header.removeFromRight(resetW).withSizeKeepingCentre(resetW, resetH);

    auto footer = r.removeFromBottom(metrics::scaled(40)).reduced(metrics::scaled(12), 0);
    const int applyW = metrics::scaled(64);
    const int applyH = metrics::scaled(24);
    applyBounds = footer.removeFromRight(applyW).withSizeKeepingCentre(applyW, applyH);

    const int pad = metrics::scaled(12);
    auto body = r.reduced(pad);
    bodyViewport.setBounds(body);
    if (panel != nullptr)
    {
        const int contentH = panelHeight ? panelHeight() : body.getHeight();
        panel->setSize(juce::jmax(1, bodyViewport.getMaximumVisibleWidth()),
                       juce::jmax(1, contentH));
        panel->resized();
    }
}

void FilterPopover::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat();
    // Inset face so brutal offset shadow / thick borders never clip.
    const float inset = themeDepth() == ThemeDepth::Brutal
                            ? juce::jmax(1.5f, themeBorderPx()) + 1.0f
                            : 0.0f;
    auto b = outer.reduced(inset);
    ds::shadow::popover(g, b, ds::radius::md);
    {
        juce::Path clip;
        clip.addRoundedRectangle(b, ds::radius::md);
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(clip);
        g.setColour(ds::surface100());
        g.fillRect(b);
    }

    auto header = b.toNearestInt().removeFromTop(metrics::scaled(40));
    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clip;
        clip.addRoundedRectangle(b, ds::radius::md);
        g.reduceClipRegion(clip);
        g.setColour(ds::line());
        g.fillRect(header.getX(), header.getBottom() - 1, header.getWidth(), 1);
        auto footerLine = b.toNearestInt().removeFromBottom(metrics::scaled(40));
        g.fillRect(footerLine.removeFromTop(1));
    }

    auto titleR = header.reduced(metrics::scaled(12), 0);
    titleR.setRight(resetBounds.getX() - 8);
    g.setColour(ds::ink());
    g.setFont(uiFontFixed(14.0f, true));
    g.drawText("Filters", titleR, juce::Justification::centredLeft, false);

    auto paintChip = [&](juce::Rectangle<int> bounds, const juce::String& label, bool hover, bool solid)
    {
        if (bounds.isEmpty()) return;
        auto rb = bounds.toFloat();
        juce::Path p;
        p.addRoundedRectangle(rb, ds::radius::sm);
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(p);
            g.setColour(solid ? ds::primary() : (hover ? ds::surface300() : ds::surface100()));
            g.fillRect(rb);
        }
        if (!solid)
        {
            g.setColour(ds::edge());
            g.drawRoundedRectangle(rb.reduced(0.5f), ds::radius::sm, 1.0f);
        }
        g.setColour(solid ? ds::onPrimary() : ds::ink());
        g.setFont(uiFontFixed(12.0f, true));
        g.drawText(label, bounds, juce::Justification::centred, false);
    };

    paintChip(resetBounds, "Reset", resetHover, false);
    paintChip(applyBounds, "Apply", applyHover, true);

    auto footer = b.toNearestInt().removeFromBottom(metrics::scaled(40)).reduced(metrics::scaled(12), 0);
    footer.setRight(applyBounds.getX() - 8);
    g.setColour(ds::inkMuted());
    g.setFont(monoFontFixed(12.0f));
    g.drawText(countText, footer, juce::Justification::centredLeft, true);
}

void FilterPopover::mouseDown(const juce::MouseEvent& e)
{
    if (applyBounds.contains(e.getPosition()) && onApply)
    {
        onApply();
        return;
    }
    if (resetBounds.contains(e.getPosition()) && onReset)
        onReset();
}

void FilterPopover::mouseMove(const juce::MouseEvent& e)
{
    const bool resetHot = resetBounds.contains(e.getPosition());
    const bool applyHot = applyBounds.contains(e.getPosition());
    if (resetHot != resetHover)
    {
        resetHover = resetHot;
        repaint(resetBounds.expanded(2));
    }
    if (applyHot != applyHover)
    {
        applyHover = applyHot;
        repaint(applyBounds.expanded(2));
    }
    setMouseCursor((resetHot || applyHot) ? juce::MouseCursor::PointingHandCursor
                                          : juce::MouseCursor::NormalCursor);
}

void FilterPopover::mouseExit(const juce::MouseEvent&)
{
    if (resetHover)
    {
        resetHover = false;
        repaint(resetBounds.expanded(2));
    }
    if (applyHover)
    {
        applyHover = false;
        repaint(applyBounds.expanded(2));
    }
}

bool FilterPopover::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onDone) onDone();
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        if (onApply) onApply();
        return true;
    }
    return false;
}

// ── KindGridView ─────────────────────────────────────────────────────────────

namespace {
constexpr int kGridCols = kNumClipKinds;
constexpr int kGridHeadH = 22;
constexpr int kGridCellH = 22;   // clip-height
constexpr int kGridColW = 96;    // track-width
}

KindGridView::KindGridView()
{
    setWantsKeyboardFocus(true);
    for (int i = 0; i < kGridCols; ++i)
        cols.push_back({ (ClipKind) i, {} });
    vScroll.addListener(this);
    vScroll.setAutoHide(false);
    addAndMakeVisible(vScroll);
}

int KindGridView::idealWidth()
{
    return metrics::scaled(kGridColW) * kGridCols + (kGridCols - 1);
}

void KindGridView::setSource(const FileListPanel* list)
{
    source = list;
    refresh();
}

void KindGridView::setKindMask(uint8_t mask)
{
    if (kindMask == mask) return;
    kindMask = mask;
    refresh();
}

int KindGridView::cellH() const
{
    return metrics::scaled(kGridCellH);
}

int KindGridView::defaultColWidth() const
{
    const int n = juce::jmax(1, numVisibleCols());
    return juce::jmax(1, (gridBodyWidth() - (n - 1)) / n);
}

void KindGridView::refresh()
{
    cols.clear();
    for (int i = 0; i < kGridCols; ++i)
        if (kindMask == 0 || (kindMask & (uint8_t) (1u << i)) != 0)
            cols.push_back({ (ClipKind) i, {} });
    if (cols.empty())
        for (int i = 0; i < kGridCols; ++i)
            cols.push_back({ (ClipKind) i, {} });

    if (source != nullptr)
        for (int i = 0; i < source->getNumEntries(); ++i)
            if (const auto* e = source->entryAt(i); e != nullptr && !e->isDirectory)
            {
                const int k = juce::jlimit(0, kGridCols - 1, (int) e->kind);
                for (auto& c : cols)
                    if ((int) c.kind == k)
                    {
                        c.entries.push_back(i);
                        break;
                    }
            }
    scrollY = juce::jlimit(0, maxScroll(), scrollY);
    ensureSelectedVisible();
    syncScrollBar();
    repaint();
}

void KindGridView::setPlayhead(bool isPlaying, float loopPhase)
{
    if (playing == isPlaying && std::abs(phase - loopPhase) < 0.002f)
        return;
    playing = isPlaying;
    phase = loopPhase;
    repaint();
}

ClipKind KindGridView::selectedColumnKind() const
{
    if (source == nullptr) return ClipKind::Lead;
    const int sel = source->getSelectedIndex();
    if (const auto* e = source->entryAt(sel); e != nullptr)
        return e->kind;
    return ClipKind::Lead;
}

int KindGridView::columnIndexForEntry(int entryIdx) const
{
    for (int c = 0; c < numVisibleCols(); ++c)
        for (int e : cols[(size_t) c].entries)
            if (e == entryIdx)
                return c;
    return -1;
}

int KindGridView::rowOfEntry(int col, int entryIdx) const
{
    if (!juce::isPositiveAndBelow(col, numVisibleCols())) return -1;
    const auto& list = cols[(size_t) col].entries;
    for (int r = 0; r < (int) list.size(); ++r)
        if (list[(size_t) r] == entryIdx)
            return r;
    return -1;
}

void KindGridView::ensureSelectedVisible()
{
    if (source == nullptr) return;
    const int sel = source->getSelectedIndex();
    const int col = columnIndexForEntry(sel);
    if (col < 0) return;
    const int row = rowOfEntry(col, sel);
    if (row < 0) return;

    const int headH = metrics::scaled(kGridHeadH);
    const int h = cellH();
    const int viewH = juce::jmax(0, getHeight() - headH);
    const int y = row * h;
    if (y < scrollY)
        scrollY = y;
    else if (y + h > scrollY + viewH)
        scrollY = juce::jmax(0, y + h - viewH);
    scrollY = juce::jlimit(0, maxScroll(), scrollY);
    syncScrollBar();
}

int KindGridView::gridBodyWidth() const
{
    const int sb = vScroll.isVisible() ? vScroll.getWidth() : 0;
    return juce::jmax(1, getWidth() - sb);
}

juce::Rectangle<int> KindGridView::columnBounds(int c) const
{
    const int n = juce::jmax(1, numVisibleCols());
    const int gaps = n - 1;
    const int bodyW = gridBodyWidth();
    const int avail = juce::jmax(0, bodyW - gaps);

    int fixedSum = 0, autoCount = 0;
    for (int i = 0; i < n; ++i)
    {
        const int k = (int) cols[(size_t) i].kind;
        if (colWidthOverride[k] > 0) fixedSum += colWidthOverride[k];
        else ++autoCount;
    }
    const int rem = juce::jmax(0, avail - fixedSum);
    const int autoW = autoCount > 0 ? rem / autoCount : 0;

    int x = 0;
    for (int i = 0; i < n; ++i)
    {
        const int k = (int) cols[(size_t) i].kind;
        int w = colWidthOverride[k] > 0 ? colWidthOverride[k] : autoW;
        if (i == n - 1)
            w = juce::jmax(1, bodyW - x);
        if (i == c)
            return { x, 0, w, getHeight() };
        x += w + 1;
    }
    return {};
}

int KindGridView::maxScroll() const
{
    size_t rows = 0;
    for (const auto& c : cols)
        rows = std::max(rows, c.entries.size());
    const int contentH = (int) rows * cellH();
    return juce::jmax(0, contentH - (getHeight() - metrics::scaled(kGridHeadH)));
}

void KindGridView::resized()
{
    const int sbW = juce::jmax(10, metrics::scaled(10));
    const int headH = metrics::scaled(kGridHeadH);
    vScroll.setBounds(getWidth() - sbW, headH, sbW, juce::jmax(0, getHeight() - headH));
    scrollY = juce::jlimit(0, maxScroll(), scrollY);
    syncScrollBar();
}

void KindGridView::syncScrollBar()
{
    const int headH = metrics::scaled(kGridHeadH);
    const int viewH = juce::jmax(1, getHeight() - headH);
    size_t rows = 0;
    for (const auto& c : cols)
        rows = std::max(rows, c.entries.size());
    const int contentH = juce::jmax(viewH, (int) rows * cellH());
    // Always show the thumb track so users can jump quickly.
    vScroll.setVisible(true);
    vScroll.setRangeLimits(0.0, (double) contentH);
    vScroll.setCurrentRange((double) scrollY, (double) viewH, juce::dontSendNotification);
    vScroll.setSingleStepSize((double) cellH());
}

void KindGridView::scrollBarMoved(juce::ScrollBar*, double newRangeStart)
{
    const int next = juce::jlimit(0, maxScroll(), (int) std::lround(newRangeStart));
    if (next == scrollY) return;
    scrollY = next;
    repaint();
}

void KindGridView::selectAt(int col, int row)
{
    if (!juce::isPositiveAndBelow(col, numVisibleCols())) return;
    const auto& list = cols[(size_t) col].entries;
    if (!juce::isPositiveAndBelow(row, (int) list.size())) return;
    const int entry = list[(size_t) row];
    if (onSelect) onSelect(entry);
    ensureSelectedVisible();
    repaint();
}

void KindGridView::moveSelection(int dCol, int dRow)
{
    if (source == nullptr || numVisibleCols() <= 0) return;
    int col = columnIndexForEntry(source->getSelectedIndex());
    int row = col >= 0 ? rowOfEntry(col, source->getSelectedIndex()) : -1;
    if (col < 0 || row < 0)
    {
        // Nothing selected — land on the first clip that exists.
        for (int c = 0; c < numVisibleCols(); ++c)
            if (!cols[(size_t) c].entries.empty())
            {
                selectAt(c, 0);
                return;
            }
        return;
    }

    if (dCol != 0)
    {
        int c = col;
        while (true)
        {
            c += dCol > 0 ? 1 : -1;
            if (!juce::isPositiveAndBelow(c, numVisibleCols()))
                break;
            const auto& list = cols[(size_t) c].entries;
            if (list.empty()) continue;
            selectAt(c, juce::jmin(row, (int) list.size() - 1));
            return;
        }
        return;
    }

    if (dRow != 0)
    {
        const auto& list = cols[(size_t) col].entries;
        const int next = row + dRow;
        if (juce::isPositiveAndBelow(next, (int) list.size()))
            selectAt(col, next);
    }
}

int KindGridView::columnDividerAt(juce::Point<int> p) const
{
    // Only the selected column's right edge is draggable.
    const ClipKind selKind = selectedColumnKind();
    const int near = juce::jmax(3, metrics::scaled(3));
    for (int c = 0; c < numVisibleCols(); ++c)
    {
        if (cols[(size_t) c].kind != selKind) continue;
        const auto r = columnBounds(c);
        if (std::abs(p.x - r.getRight()) <= near && p.y >= 0 && p.y < getHeight())
            return c;
    }
    return -1;
}

int KindGridView::entryAt(juce::Point<int> p) const
{
    const int headH = metrics::scaled(kGridHeadH);
    const int h = cellH();
    if (p.y < headH || h <= 0) return -1;
    for (int c = 0; c < numVisibleCols(); ++c)
    {
        if (!columnBounds(c).contains(p)) continue;
        const int row = (p.y - headH + scrollY) / h;
        const auto& list = cols[(size_t) c].entries;
        return juce::isPositiveAndBelow(row, (int) list.size()) ? list[(size_t) row] : -1;
    }
    return -1;
}

void KindGridView::paint(juce::Graphics& g)
{
    g.setColour(ds::line());
    g.fillAll();

    const int headH = metrics::scaled(kGridHeadH);
    const int h = cellH();
    const int selected = source != nullptr ? source->getSelectedIndex() : -1;
    const int nCols = numVisibleCols();

    for (int c = 0; c < nCols; ++c)
    {
        const auto colR = columnBounds(c);
        const auto& col = cols[(size_t) c];
        const auto clipCol = kindClipColour(col.kind);

        g.setColour(ds::surface100());
        g.fillRect(colR);

        // Cells (clipped below the header so they scroll under it).
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(colR.withTrimmedTop(headH));
            const int rowsVisible = h > 0 ? (colR.getHeight() - headH) / h + 2 : 0;
            const int first = h > 0 ? scrollY / h : 0;
            for (int r = first; r < first + rowsVisible; ++r)
            {
                auto cell = juce::Rectangle<int>(colR.getX(), headH + r * h - scrollY, colR.getWidth(), h);
                const bool has = r < (int) col.entries.size();
                auto inner = cell.withTrimmedBottom(1);
                if (!has)
                {
                    g.setColour(ds::surface200());
                    g.fillRect(inner);
                    auto sq = juce::Rectangle<float>((float) inner.getX() + 6.0f, (float) inner.getCentreY() - 3.0f, 6.0f, 6.0f);
                    g.setColour(ds::inkFaint());
                    g.drawRect(sq, 1.0f);
                }
                else
                {
                    const int entryIdx = col.entries[(size_t) r];
                    const auto* e = source->entryAt(entryIdx);
                    const bool isSel = entryIdx == selected;
                    const bool isPlayingSel = isSel && playing;

                    // Levels: kind-tinted cell (kind hue at 10% over surface), ink text.
                    // Selected = violet tint + 1.5px violet border; playing = success bar.
                    if (isSel)
                        g.setColour(ds::selsoft());
                    else
                        g.setColour(ds::surface200().overlaidWith(clipCol.withAlpha(0.10f)));
                    g.fillRect(inner);

                    if (isPlayingSel)
                    {
                        const float prog = juce::jlimit(0.0f, 1.0f, phase);
                        g.setColour(ds::play());
                        g.fillRect(inner.getX(), inner.getBottom() - 3,
                                   juce::roundToInt((float) inner.getWidth() * prog), 3);
                    }

                    const bool starred = e != nullptr && e->starred;
                    auto starR = starBoundsForCell(inner).toFloat();
                    const auto starCol = starred ? favoriteGold() : ds::inkMuted().withAlpha(0.7f);
                    drawIcon(g, starred ? icons::star : icons::starOutline, starR, starCol,
                             starred ? 1.6f : 1.35f);

                    g.setColour(ds::ink());
                    g.setFont(uiFontFixed(11.0f, isSel));
                    g.drawText(e != nullptr ? e->name : juce::String(),
                               inner.withTrimmedLeft(20).withTrimmedRight(4), juce::Justification::centredLeft, true);

                    if (isSel)
                    {
                        g.setColour(ds::acc());
                        g.drawRect(inner.toFloat().reduced(0.75f), 1.5f);
                    }
                }
                g.setColour(ds::line());
                g.fillRect(cell.getX(), cell.getBottom() - 1, cell.getWidth(), 1);
            }
        }

        auto head = colR.withHeight(headH);
        g.setColour(ds::surface200());
        g.fillRect(head);
        g.setColour(clipCol);
        g.fillRect(head.removeFromTop(3));
        g.setColour(ds::line());
        g.fillRect(head.getX(), head.getBottom() - 1, head.getWidth(), 1);
        auto label = head.reduced(6, 0);
        g.setColour(ds::inkMuted());
        g.setFont(monoFontFixed(12.0f));
        const auto count = juce::String((int) col.entries.size());
        g.drawText(count, label.removeFromRight(24), juce::Justification::centredRight, false);
        g.setColour(ds::ink());
        g.setFont(uiFontFixed(11.0f, true));
        g.drawText(clipKindName(col.kind), label, juce::Justification::centredLeft, true);
    }

    if (hasKeyboardFocus(false))
        drawFocusRing(g, getLocalBounds().toFloat().reduced(1.5f), 0.0f);
}

juce::Rectangle<int> KindGridView::starBoundsForCell(juce::Rectangle<int> inner) const
{
    return { inner.getX() + 4, inner.getCentreY() - 7, 14, 14 };
}

void KindGridView::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (onActivated) onActivated();
    dragStarted = false;
    resizingCol = false;
    resizeColIndex = columnDividerAt(e.getPosition());
    if (resizeColIndex >= 0)
    {
        resizingCol = true;
        resizeStartX = e.getScreenX();
        resizeStartW = columnBounds(resizeColIndex).getWidth();
        return;
    }
    pressedEntry = entryAt(e.getPosition());
    if (pressedEntry >= 0)
    {
        // Star hit → favorite; otherwise select/play.
        const int headH = metrics::scaled(kGridHeadH);
        const int h = cellH();
        const int col = columnIndexForEntry(pressedEntry);
        const int row = rowOfEntry(col, pressedEntry);
        if (col >= 0 && row >= 0)
        {
            const auto colR = columnBounds(col);
            auto cell = juce::Rectangle<int>(colR.getX(), headH + row * h - scrollY, colR.getWidth(), h);
            if (starBoundsForCell(cell.withTrimmedBottom(1)).contains(e.getPosition()))
            {
                if (onToggleStar) onToggleStar(pressedEntry);
                pressedEntry = -1;
                repaint();
                return;
            }
        }
        if (onSelect) onSelect(pressedEntry);
    }
    ensureSelectedVisible();
    repaint();
}

void KindGridView::mouseDrag(const juce::MouseEvent& e)
{
    if (resizingCol && juce::isPositiveAndBelow(resizeColIndex, numVisibleCols()))
    {
        const int kind = (int) cols[(size_t) resizeColIndex].kind;
        const int minW = metrics::scaled(56);
        const int maxW = juce::jmax(minW, gridBodyWidth() - (numVisibleCols() - 1) * minW);
        const int next = juce::jlimit(minW, maxW, resizeStartW + (e.getScreenX() - resizeStartX));
        if (colWidthOverride[kind] != next)
        {
            colWidthOverride[kind] = next;
            repaint();
        }
        return;
    }
    if (dragStarted || pressedEntry < 0 || e.getDistanceFromDragStart() < 4 || source == nullptr)
        return;
    if (const auto* entry = source->entryAt(pressedEntry); entry != nullptr && onDragFile)
    {
        dragStarted = true;
        onDragFile(entry->file);
    }
}

void KindGridView::mouseUp(const juce::MouseEvent&)
{
    pressedEntry = -1;
    dragStarted = false;
    resizingCol = false;
    resizeColIndex = -1;
}

void KindGridView::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(columnDividerAt(e.getPosition()) >= 0
                       ? juce::MouseCursor::LeftRightResizeCursor
                       : juce::MouseCursor::NormalCursor);
}

void KindGridView::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void KindGridView::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int idx = entryAt(e.getPosition());
    if (idx >= 0 && onOpen)
        onOpen(idx);
}

void KindGridView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int next = juce::jlimit(0, maxScroll(), scrollY - juce::roundToInt(w.deltaY * 240.0f));
    if (next != scrollY)
    {
        scrollY = next;
        syncScrollBar();
        repaint();
    }
}

bool KindGridView::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey)  { moveSelection(-1, 0); return true; }
    if (key == juce::KeyPress::rightKey) { moveSelection(1, 0);  return true; }
    if (key == juce::KeyPress::upKey)    { moveSelection(0, -1); return true; }
    if (key == juce::KeyPress::downKey)  { moveSelection(0, 1);  return true; }
    return false;
}

} // namespace pflow
