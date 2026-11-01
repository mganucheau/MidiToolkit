#include "BrowserPanels.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace pflow {

namespace {

// 3c source-list metrics (Cupertino sidebar).
constexpr int kSidebarHeaderH = 28;
constexpr int kSidebarSectionH = 18;
constexpr int kSidebarRowH = 28;
constexpr int kOpenFolderH = 34;
constexpr int kCardHeaderH = 22;
constexpr int kSectionGap = 12;
constexpr int kHeaderToRows = 4;
constexpr int kBodyPadT = 8;
constexpr int kBodyPadB = 10;
constexpr int kPadH = 8;
constexpr int kPadV = 12;
constexpr int kRowPadX = 6;
constexpr int kIconCol = 16;
constexpr int kRowIconGap = 6;
constexpr int kCollapsedBtn = 28;
constexpr float kCapPt = 11.0f;
constexpr float kTitlePt = 12.5f;
constexpr float kCtrlPt = 13.0f;
constexpr float kSidebarIconStroke = 1.5f;

inline int sidebarHeaderH() { return metrics::scaled(kSidebarHeaderH); }
inline int sidebarSectionH() { return metrics::scaled(kSidebarSectionH); }
inline int sidebarRowH() { return metrics::scaled(kSidebarRowH); }
inline int sidebarRowGap() { return metrics::scaled(2); }
inline int sidebarSectionGap() { return metrics::scaled(kSectionGap); }
inline int sidebarHeaderToRows() { return metrics::scaled(kHeaderToRows); }
inline int sidebarBodyPadT() { return metrics::scaled(kBodyPadT); }
inline int sidebarPadH() { return metrics::scaled(kPadH); }
inline int sidebarPadV() { return metrics::scaled(kPadV); }
inline int sidebarRowPadX() { return metrics::scaled(kRowPadX); }
inline int sidebarIconCol() { return metrics::scaled(kIconCol); }
inline int sidebarRowIconGap() { return metrics::scaled(kRowIconGap); }
inline int collapsedBtnSize() { return metrics::scaled(kCollapsedBtn); }
inline int openFolderH() { return metrics::scaled(kOpenFolderH); }

struct SourceListPalette
{
    juce::Colour tint, card, control, text, text2, text3, accent, selectWash, border, gold, shadow;
};

inline SourceListPalette sl()
{
    // All colours from ds:: tokens (§2) — no local hex.
    return {
        ds::side(),
        ds::panel(),
        ds::ctl(),
        ds::tx(),
        ds::tx2(),
        ds::tx3(),
        ds::acc(),
        ds::selsoft(),
        ds::ctlb(),
        ds::stron(),
        juce::Colours::black.withAlpha(usesDarkAppearance() ? 0.30f : 0.14f),
    };
}

inline void fillControlSurface(juce::Graphics& g, juce::Rectangle<float> r, float radius,
                                bool drawShadow = true)
{
    const auto p = sl();
    g.setColour(p.control);
    g.fillRoundedRectangle(r, radius);
    g.setColour(p.border);
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
    if (drawShadow)
        ds::shadow::control(g, r, radius);
}

} // namespace

// ── FilterPanel ──────────────────────────────────────────────────────────────

FavoritesSidebar::FilterPanel::FilterPanel()
{
    juce::StringArray keys;
    for (int i = 0; i < 12; ++i)
        keys.add(kNoteNames[(size_t) i]);
    keyGrid.title = "KEY";
    keyGrid.columns = 6;
    keyGrid.multiSelect = true;
    keyGrid.sidebarLabelStyle = true;
    keyGrid.collapsible = true;
    keyGrid.collapsed = true;
    keyGrid.onCollapsedChanged = [this] { if (onLayoutChanged) onLayoutChanged(); };
    keyGrid.setItems(keys);
    keyGrid.onChange = [this]
    {
        if (onCriteriaChanged) onCriteriaChanged();
    };
    addAndMakeVisible(keyGrid);

    juce::StringArray kinds;
    for (int i = 0; i < 5; ++i)
        kinds.add(clipKindName((ClipKind) i));
    kindGrid.title = "KIND";
    kindGrid.columns = 3;
    kindGrid.multiSelect = true;
    kindGrid.sidebarLabelStyle = true;
    kindGrid.collapsible = true;
    kindGrid.collapsed = true;
    kindGrid.onCollapsedChanged = [this] { if (onLayoutChanged) onLayoutChanged(); };
    kindGrid.setItems(kinds);
    kindGrid.onChange = [this]
    {
        if (onCriteriaChanged) onCriteriaChanged();
    };
    addAndMakeVisible(kindGrid);

    juce::StringArray timeSigs;
    for (int i = 0; i < kNumFilterTimeSigs; ++i)
        timeSigs.add(kFilterTimeSigs[i].label);
    timeSigGrid.title = "TIME";
    timeSigGrid.columns = 4;
    timeSigGrid.multiSelect = true;
    timeSigGrid.sidebarLabelStyle = true;
    timeSigGrid.collapsible = true;
    timeSigGrid.collapsed = true;
    timeSigGrid.onCollapsedChanged = [this] { if (onLayoutChanged) onLayoutChanged(); };
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
        range->onCollapsedChanged = [this] { if (onLayoutChanged) onLayoutChanged(); };
        range->onChange = [this](int, int)
        {
            if (onCriteriaChanged) onCriteriaChanged();
        };
        addAndMakeVisible(*range);
    }
    bpmRange.setTooltip("Filter by tempo (BPM)");
    barsRange.setTooltip("Filter by length in bars");
    complexityRange.setTooltip("Filter by complexity score");
}

void FavoritesSidebar::FilterPanel::lookAndFeelChanged()
{
    repaint();
}

int FavoritesSidebar::FilterPanel::idealHeight() const
{
    const int gap = metrics::scaled(4);
    const int sub = metrics::scaled(28);
    return sub + metrics::scaled(8)
         + keyGrid.idealHeight() + gap
         + kindGrid.idealHeight() + gap
         + timeSigGrid.idealHeight() + gap
         + bpmRange.idealHeight() + gap
         + barsRange.idealHeight() + gap
         + complexityRange.idealHeight() + gap
         + metrics::scaled(4)
         + 24;
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
    s.kindMask = (uint8_t) kindGrid.getSelectedMask();
    s.timeSigMask = timeSigGrid.getSelectedMask();
    s.removeDuplicates = hideDuplicates;
}

int FavoritesSidebar::FilterPanel::activeGroupCount() const
{
    int n = 0;
    if (keyGrid.getSelectedMask() != 0) ++n;
    if (kindGrid.getSelectedMask() != 0) ++n;
    if (timeSigGrid.getSelectedMask() != 0) ++n;
    if (!bpmRange.isFullRange()) ++n;
    if (!barsRange.isFullRange()) ++n;
    if (!complexityRange.isFullRange()) ++n;
    if (!hideDuplicates) ++n;
    return n;
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
    kindGrid.setSelectedMask(s.kindMask, juce::dontSendNotification);
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
    kindGrid.setSelectedMask(0, juce::dontSendNotification);
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
    if (!subdirRowBounds.isEmpty())
    {
        g.setFont(uiFontFixed(kCtrlPt));
        g.setColour(scanning ? ds::tx3() : ds::tx());
        auto label = subdirRowBounds;
        if (!subdirSwitchBounds.isEmpty())
            label.setRight(subdirSwitchBounds.getX() - 8);
        g.drawText("Subdirectories", label, juce::Justification::centredLeft, true);

        auto track = subdirSwitchBounds.toFloat();
        if (!track.isEmpty())
        {
            const float radius = track.getHeight() * 0.5f;
            g.setColour(includeSubdirs ? t.accent : t.tallWell);
            g.fillRoundedRectangle(track, radius);
            if (!includeSubdirs)
            {
                g.setColour(t.controlHairline);
                g.drawRoundedRectangle(track.reduced(0.5f), radius, 1.0f);
            }
            const float knob = juce::jmax(8.0f, track.getHeight() - 4.0f);
            const float kx = includeSubdirs ? track.getRight() - knob - 2.0f
                                             : track.getX() + 2.0f;
            g.setColour(includeSubdirs ? fx::onAccentColour() : t.headerText);
            g.fillEllipse(kx, track.getCentreY() - knob * 0.5f, knob, knob);
        }
    }

    auto paintChip = [&](juce::Rectangle<int> r, const juce::String& label, bool on)
    {
        if (r.isEmpty()) return;
        auto cell = r.toFloat();
        g.setColour(on ? fx::accentFillColour() : t.tallWell);
        g.fillRoundedRectangle(cell, 6.0f);
        if (!on)
        {
            g.setColour(t.controlHairline);
            g.drawRoundedRectangle(cell.reduced(0.5f), 6.0f, 1.0f);
        }
        g.setFont(uiFontFixed(fx::kAnnotPt, on));
        g.setColour(on ? fx::onAccentColour() : t.headerText);
        g.drawText(label, r, juce::Justification::centred, false);
    };
    paintChip(hideDupesBounds, "Hide duplicates", hideDuplicates);
    paintChip(resetBounds, "Reset", false);
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
    const int gap = metrics::scaled(4);

    subdirRowBounds = r.removeFromTop(metrics::scaled(28));
    {
        constexpr int kSwitchW = 28;
        constexpr int kSwitchH = 16;
        subdirSwitchBounds = juce::Rectangle<int>(
            subdirRowBounds.getRight() - kSwitchW,
            subdirRowBounds.getY() + (subdirRowBounds.getHeight() - kSwitchH) / 2,
            kSwitchW, kSwitchH);
    }
    r.removeFromTop(metrics::scaled(8));

    keyGrid.setBounds(r.removeFromTop(keyGrid.idealHeight()));
    r.removeFromTop(gap);
    kindGrid.setBounds(r.removeFromTop(kindGrid.idealHeight()));
    r.removeFromTop(gap);
    timeSigGrid.setBounds(r.removeFromTop(timeSigGrid.idealHeight()));
    r.removeFromTop(gap);
    bpmRange.setBounds(r.removeFromTop(bpmRange.idealHeight()));
    r.removeFromTop(gap);
    barsRange.setBounds(r.removeFromTop(barsRange.idealHeight()));
    r.removeFromTop(gap);
    complexityRange.setBounds(r.removeFromTop(complexityRange.idealHeight()));
    r.removeFromTop(gap);
    r.removeFromTop(metrics::scaled(4));
    auto row = r.removeFromTop(24);
    const auto font = uiFontFixed(fx::kAnnotPt, false);
    auto chipW = [&](const juce::String& label)
    {
        const int textW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(font, label));
        return textW + 20;
    };
    constexpr int chipGap = 4;
    hideDupesBounds = row.removeFromLeft(juce::jmin(chipW("Hide duplicates"), row.getWidth()));
    if (row.getWidth() > chipGap)
        row.removeFromLeft(chipGap);
    resetBounds = row.removeFromLeft(juce::jmin(chipW("Reset"), row.getWidth()));
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
    const auto p = sl();
    queryField.setTextToShowWhenEmpty("Search", p.text3);
    queryField.setFont(uiFontFixed(13.0f));
    queryField.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    queryField.setColour(juce::TextEditor::textColourId, p.text);
    queryField.setColour(juce::TextEditor::highlightedTextColourId, colours::accentInk());
    queryField.setColour(juce::TextEditor::highlightColourId, p.accent.withAlpha(0.35f));
    queryField.setColour(juce::CaretComponent::caretColourId, p.accent);
    queryField.setIndents(0, 2);
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
    const auto p = sl();
    const int iconCol = sidebarIconCol();
    const bool active = queryField.hasKeyboardFocus(true);

    if (!queryWell.isEmpty())
    {
        auto well = queryWell.toFloat();
        g.setColour(p.control);
        g.fillRoundedRectangle(well, 6.0f);
        if (filterActive && !filterCapBounds.isEmpty())
        {
            juce::Path clip;
            clip.addRoundedRectangle(well, 6.0f);
            g.saveState();
            g.reduceClipRegion(clip);
            g.setColour(p.accent);
            g.fillRect(filterCapBounds);
            g.restoreState();
        }
        g.setColour(p.border);
        g.drawRoundedRectangle(well.reduced(0.5f), 6.0f, 1.0f);
        if (!filterCapBounds.isEmpty())
        {
            g.setColour(p.border);
            g.fillRect((float) filterCapBounds.getX(), well.getY() + 6.0f, 1.0f, well.getHeight() - 12.0f);
        }
        if (active)
            drawFocusRing(g, well, 6.0f);
    }

    auto iconArea = juce::Rectangle<float>((float) queryWell.getX() + 4.0f,
                                           (float) queryWell.getY(),
                                           (float) iconCol,
                                           (float) queryWell.getHeight())
                        .withSizeKeepingCentre(14.0f, 14.0f);
    drawIcon(g, icons::search, iconArea, active ? p.accent : p.text3, kSidebarIconStroke);
    if (!filterCapBounds.isEmpty())
    {
        drawIcon(g, icons::sliders,
                 filterCapBounds.toFloat().withSizeKeepingCentre(14.0f, 14.0f),
                 filterActive ? colours::accentInk() : p.text3, kSidebarIconStroke);
    }

    if (saveVisible && !saveChipBounds.isEmpty())
    {
        g.setColour(p.accent);
        g.fillRoundedRectangle(saveChipBounds.toFloat(), (float) saveChipBounds.getHeight() * 0.5f);
        g.setColour(colours::accentInk());
        g.setFont(uiFontFixed(11.0f, true));
        g.drawText("Save", saveChipBounds, juce::Justification::centred, false);
    }
}

void FavoritesSidebar::SearchQueryPanel::resized()
{
    auto r = getLocalBounds();
    const int rowH = idealHeight();
    const int iconCol = sidebarIconCol();
    const int iconGap = sidebarRowIconGap();

    auto querySlot = r.removeFromTop(rowH);
    queryWell = querySlot;
    filterCapBounds = querySlot.removeFromRight(rowH);

    saveChipBounds = {};
    if (saveVisible)
    {
        constexpr int kChipW = 46;
        constexpr int kChipH = 20;
        saveChipBounds = querySlot.removeFromRight(kChipW + 6)
                             .withTrimmedRight(5)
                             .withSizeKeepingCentre(kChipW, juce::jmin(kChipH, rowH - 8));
    }

    queryField.setBounds(querySlot.withTrimmedLeft(iconCol + iconGap + 4).reduced(0, 3));
}

// ── FavoritesSidebar (3c compact action row) ─────────────────────────────────

FavoritesSidebar::FavoritesSidebar()
{
    btnToggle.ghost = true;
    btnToggle.iconScale = 1.0f;
    btnToggle.setWantsKeyboardFocus(true);
    btnToggle.setTooltip(collapsed ? "Expand sidebar" : "Collapse sidebar");
    btnToggle.onClick = [this] { setCollapsed(!collapsed); };
    addAndMakeVisible(btnToggle);

    btnSettings.ghost = true;
    btnSettings.iconScale = 1.0f;
    btnSettings.setWantsKeyboardFocus(true);
    btnSettings.setTooltip("Settings");
    btnSettings.onClick = [this] { if (onOpenSettings) onOpenSettings(); };
    addAndMakeVisible(btnSettings);

    expandedWidth = metrics::sidebarExpandedWidth();
    setWantsKeyboardFocus(true);

    filterPanel.onCriteriaChanged = [this]
    {
        scheduleFilterApply();
        searchQuery.setFilterActive(filterOpen || activeFilterGroupCount() > 0);
        repaint();
    };
    filterPanel.onLayoutChanged = [this] { notifyHeightChanged(); };
    filterPanel.onSubdirsChanged = [this](bool on)
    {
        includeSubdirs = on;
        if (onScanSubdirs)
            onScanSubdirs();
    };
    addChildComponent(filterPanel);

    searchQuery.onActivate = [this]
    {
        searchOpen = true;
        if (collapsed)
            setCollapsed(false);
    };
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
    searchQuery.onToggleFilter = [this] { toggleFilterPanel(); };
    addChildComponent(searchQuery);

    rebuildRows();
}

void FavoritesSidebar::notifyHeightChanged()
{
    layoutChildren();
    resized();
    repaint();
    if (onContentHeightChanged) onContentHeightChanged();
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
    return sidebarHeaderH() + sidebarBodyPadT();
}

int FavoritesSidebar::footerPad() const
{
    return sidebarBodyPadT();
}

int FavoritesSidebar::footerHeight() const
{
    return footerPad() + sidebarRowH() + metrics::scaled(kBodyPadB);
}

std::vector<FavoritesSidebar::LayoutItem> FavoritesSidebar::collectLayout() const
{
    std::vector<LayoutItem> out;
    const int rowH = collapsed ? collapsedBtnSize() : sidebarRowH();
    const int gap = sidebarRowGap();
    const int padH = sidebarPadH();
    const int W = getWidth();
    int y = contentTopY();
    const int itemInset = padH + sidebarRowPadX();

    auto placeRow = [&](int rowIdx)
    {
        const auto r = collapsed
            ? juce::Rectangle<int>(0, y, W, rowH).withSizeKeepingCentre(collapsedBtnSize(), collapsedBtnSize())
            : juce::Rectangle<int>(padH, y, W - padH * 2, rowH);
        out.push_back({ LayoutKind::Row, rowIdx, r });
        y += rowH + gap;
    };

    auto placeSection = [&](SectionId id)
    {
        if (collapsed) return;
        const auto header = juce::Rectangle<int>(itemInset, y, W - itemInset * 2, sidebarSectionH());
        out.push_back({ LayoutKind::Section, (int) id, header });
        y += sidebarSectionH() + sidebarHeaderToRows();
    };

    if (!collapsed)
    {
        placeSection(SectionId::Library);
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::SavedDir
                || rows[(size_t) i].kind == RowKind::Starred)
                placeRow(i);

        y += sidebarSectionGap() - gap;
        const int contentW = juce::jmax(0, W - padH * 2);
        const int qh = searchQuery.idealHeight();
        out.push_back({ LayoutKind::Query, -1,
                        juce::Rectangle<int>(padH, y, contentW, qh) });
        y += qh;

        if (filterOpen)
        {
            y += metrics::scaled(8);
            const int fh = filterPanel.idealHeight();
            out.push_back({ LayoutKind::Filter, -1,
                            juce::Rectangle<int>(padH, y, contentW, fh) });
            y += fh;
        }
    }
    else
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::Open)
                placeRow(i);
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::Starred)
                placeRow(i);
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::SavedDir)
                placeRow(i);
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
    {
        btnToggle.setTooltip(collapsed ? "Expand sidebar" : "Collapse sidebar");
        return;
    }
    collapsed = shouldCollapse;
    btnToggle.setTooltip(collapsed ? "Expand sidebar" : "Collapse sidebar");
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
    for (int i = 0; i < (int) savedSearches.size(); ++i)
        rows.push_back({ RowKind::SavedSearch, i });
    rebuildNavItems();
}

void FavoritesSidebar::rebuildNavItems()
{
    navItems.clear();
    if (!collapsed)
    {
        navItems.push_back({ NavKind::Filters, -1 });
        navItems.push_back({ NavKind::OpenFolder, -1 });
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind == RowKind::SavedDir
                || rows[(size_t) i].kind == RowKind::Starred)
                navItems.push_back({ NavKind::Row, i });
    }
    else
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].kind != RowKind::SavedSearch)
                navItems.push_back({ NavKind::Row, i });
    }
    navItems.push_back({ NavKind::Settings, -1 });
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
            if (onAddCurrent) onAddCurrent();
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
        setTooltip(isCurrentFolderFavorited() ? "Remove from Library" : "Save folder and filters");
        return;
    }
    if (refreshBtnBounds.contains(pos))
    {
        setTooltip(scanning
                       ? (scanProgressTotal > 0
                              ? ("Scanning " + juce::String(scanProgressDone) + "/"
                                 + juce::String(scanProgressTotal))
                              : juce::String("Scanning…"))
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

    filterPanel.setVisible(false);
    if (collapsed)
    {
        searchQuery.setVisible(false);
        return;
    }

    for (const auto& item : collectLayout())
    {
        switch (item.kind)
        {
            case LayoutKind::OpenBtn:
                openFolderBounds = item.bounds;
                break;
            case LayoutKind::FolderCard:
            {
                folderCardBounds = item.bounds;
                auto inner = item.bounds.reduced(metrics::scaled(10), metrics::scaled(10));
                auto header = inner.removeFromTop(metrics::scaled(kCardHeaderH));
                starBounds = header.removeFromLeft(22).withSizeKeepingCentre(18, 18);
                inner.removeFromTop(metrics::scaled(6));
                auto action = inner.removeFromTop(24);
                constexpr int chipGap = 4;
                refreshBtnBounds = action.removeFromLeft(24);
                action.removeFromLeft(juce::jmin(chipGap, action.getWidth()));
                const auto font = uiFontFixed(fx::kAnnotPt, false);
                const int filtersText = (int) std::ceil(
                    juce::GlyphArrangement::getStringWidth(font, "Filters"));
                const int filtersW = 10 + 14 + 4 + filtersText + 10;
                filtersBtnBounds = action.removeFromRight(juce::jmin(filtersW, action.getWidth()));
                if (action.getWidth() > chipGap)
                    action.removeFromRight(chipGap);
                scanBtnBounds = action;
                if (filterOpen)
                {
                    inner.removeFromTop(metrics::scaled(12));
                    const int fh = filterPanel.idealHeight();
                    filterPanel.setBounds(inner.removeFromTop(fh));
                    filterPanel.setVisible(true);
                }
                break;
            }
            case LayoutKind::Section:
                switch ((SectionId) item.id)
                {
                    case SectionId::Library:
                    {
                        auto header = item.bounds;
                        const int plus = sidebarSectionH();
                        libraryPlusBounds = header.removeFromRight(plus)
                                                .withSizeKeepingCentre(plus, plus);
                        libraryHeaderBounds = header;
                        break;
                    }
                    case SectionId::Search:  searchHeaderBounds = item.bounds; break;
                    case SectionId::Saved:   savedHeaderBounds = item.bounds; break;
                }
                break;
            case LayoutKind::Filter:
            {
                auto b = item.bounds;
                if (searchQuery.getWidth() > 0)
                {
                    b.setX(searchQuery.getX());
                    b.setWidth(searchQuery.getWidth());
                }
                filterPanel.setBounds(b);
                filterPanel.setVisible(true);
                break;
            }
            case LayoutKind::Query:
                searchQuery.setBounds(item.bounds);
                searchQuery.setVisible(true);
                break;
            default:
                break;
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
    if (open && collapsed)
        setCollapsed(false);
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
    filterOpen = !filterOpen;
    if (filterOpen)
        filterPanel.setHistograms(histogramClips);
    notifyHeightChanged();
}

void FavoritesSidebar::resized()
{
    const int railW = metrics::sidebarRailWidth();
    const int headerH = sidebarHeaderH();
    const int padH = sidebarPadH();
    const int iconBtn = metrics::chromeIconButton();
    const int iconColX = padH + sidebarRowPadX();
    btnToggle.iconScale = 0.9f;
    if (collapsed)
        btnToggle.setBounds(juce::Rectangle<int>(0, 0, railW, headerH)
                                .withSizeKeepingCentre(iconBtn, iconBtn));
    else
        btnToggle.setBounds(juce::Rectangle<int>(getWidth() - padH - iconBtn, 0, iconBtn, headerH)
                                .withSizeKeepingCentre(iconBtn, iconBtn));

    const int footerH = footerHeight();
    const int contentBottom = contentBottomY();
    const int pad = juce::jmax(footerPad(), metrics::scaled(10));
    const int settingsTop = juce::jmax(contentBottom + pad,
                                       getHeight() - footerH + pad);
    if (collapsed)
    {
        settingsRowBounds = juce::Rectangle<int>(0, settingsTop, getWidth(), sidebarRowH())
                                .withSizeKeepingCentre(collapsedBtnSize(), collapsedBtnSize());
    }
    else
    {
        settingsRowBounds = juce::Rectangle<int>(padH, settingsTop,
                                                 getWidth() - padH * 2,
                                                 sidebarRowH());
    }
    {
        if (collapsed)
            btnSettings.setBounds(settingsRowBounds);
        else
        {
            auto iconSlot = juce::Rectangle<int>(iconColX, settingsTop,
                                                 sidebarIconCol(), sidebarRowH());
            btnSettings.setBounds(iconSlot.withSizeKeepingCentre(iconBtn, iconBtn));
        }
        btnSettings.setVisible(true);
    }
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
    if (searchQuery.isVisible() && searchQuery.getBounds().contains(e.getPosition()))
        return;
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
            if (onAddCurrent) onAddCurrent();
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
            filterOpen = false;
            notifyHeightChanged();
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
    if (bounds.isEmpty()) return;
    const auto p = sl();
    g.setColour(p.text3);
    g.setFont(uiFontFixed(kCapPt, true));
    // Letter-spaced uppercase via drawText (JUCE lacks tracking; use spaced string).
    juce::String spaced;
    for (int i = 0; i < title.length(); ++i)
    {
        spaced += title[i];
        if (i + 1 < title.length()) spaced += " ";
    }
    g.drawText(spaced, bounds, juce::Justification::centredLeft, false);
}

void FavoritesSidebar::paintOpenFolderButton(juce::Graphics& g, juce::Rectangle<int> r,
                                             bool hover, bool focus)
{
    if (r.isEmpty()) return;
    const auto p = sl();
    fillControlSurface(g, r.toFloat(), 7.0f);
    if (hover)
    {
        g.setColour(p.accent.withAlpha(0.08f));
        g.fillRoundedRectangle(r.toFloat(), 7.0f);
    }
    // Center the plus + label as one group (mock: "+ Open Folder...").
    const juce::String label = "Open Folder...";
    auto font = uiFontFixed(13.0f, true);
    g.setFont(font);
    const int textW = (int) std::ceil(font.getStringWidthFloat(label));
    const int iconW = 14;
    const int gap = 6;
    auto group = r.withSizeKeepingCentre(iconW + gap + textW, r.getHeight());
    auto icon = group.removeFromLeft(iconW).toFloat().withSizeKeepingCentre(12.0f, 12.0f);
    drawIcon(g, icons::plus, icon, p.accent, 1.8f);
    group.removeFromLeft(gap);
    g.setColour(p.text);
    g.drawText(label, group, juce::Justification::centredLeft, false);
    if (focus)
        drawFocusRing(g, r.toFloat(), 7.0f);
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

void FavoritesSidebar::paintFolderCard(juce::Graphics& g)
{
    if (folderCardBounds.isEmpty()) return;
    const auto p = sl();
    auto card = folderCardBounds.toFloat();
    // Same control face + border recipe as Open Folder / search field.
    g.setColour(p.control);
    g.fillRoundedRectangle(card, 9.0f);
    g.setColour(p.border);
    g.drawRoundedRectangle(card.reduced(0.5f), 9.0f, 1.0f);

    const bool saved = isCurrentFolderFavorited();
    const juce::File dir(active);
    const auto name = dir.getFileName().isNotEmpty() ? dir.getFileName() : active;
    {
        auto iconSlot = starBounds.toFloat().withSizeKeepingCentre(14.0f, 14.0f);
        drawIcon(g, saved ? icons::star : icons::starOutline, iconSlot,
                 saved ? p.gold : p.accent, kSidebarIconStroke);

        auto nameR = folderCardBounds.reduced(metrics::scaled(10), metrics::scaled(10))
                         .removeFromTop(metrics::scaled(kCardHeaderH));
        nameR.removeFromLeft(starBounds.getWidth() + 4);
        auto countArea = nameR.removeFromRight(44);
        g.setColour(p.text2);
        g.setFont(monoFont(12.0f));
        g.drawText(juce::String(currentClipCount), countArea,
                   juce::Justification::centredRight, false);
        g.setColour(p.text);
        g.setFont(uiFontFixed(13.0f, true));
        g.drawText(name, nameR, juce::Justification::centredLeft, true);
    }

    const bool kb = hasKeyboardFocus(true);
    const NavItem* focused = (kb && juce::isPositiveAndBelow(navIndex, (int) navItems.size()))
        ? &navItems[(size_t) navIndex] : nullptr;
    auto focusedKind = [&](NavKind k)
    {
        return focused != nullptr && focused->kind == k;
    };
    const auto& t = inspectorTokens();

    // Chip metrics match KEY / TIME / KIND: 24px tall, 6px corner, 11pt label, icon+text only.
    auto drawChip = [&](juce::Rectangle<int> r, const char* iconName, const juce::String& label,
                        bool hover, bool on, bool spin, bool focus)
    {
        if (r.isEmpty()) return;
        auto cell = r.toFloat();
        if (on)
            g.setColour(fx::accentFillColour());
        else if (hover)
            g.setColour(t.accent.withAlpha(0.12f));
        else
            g.setColour(t.tallWell);
        g.fillRoundedRectangle(cell, 6.0f);
        if (!on)
        {
            g.setColour(t.controlHairline);
            g.drawRoundedRectangle(cell.reduced(0.5f), 6.0f, 1.0f);
        }
        const auto iconCol = on ? fx::onAccentColour() : p.accent;
        const auto textCol = on ? fx::onAccentColour() : t.headerText;
        // Same font whether the chip is on or off, so the label does not shift.
        auto font = uiFontFixed(fx::kAnnotPt, false);
        g.setFont(font);
        constexpr int iconW = 14;
        constexpr int gap = 4;
        if (label.isEmpty())
        {
            auto iconR = r.toFloat().withSizeKeepingCentre(12.0f, 12.0f);
            if (spin)
            {
                g.saveState();
                g.addTransform(juce::AffineTransform::rotation(scanAngle, iconR.getCentreX(),
                                                               iconR.getCentreY()));
                drawIcon(g, iconName, iconR, iconCol, kSidebarIconStroke);
                g.restoreState();
            }
            else
            {
                drawIcon(g, iconName, iconR, iconCol, kSidebarIconStroke);
            }
        }
        else
        {
            const int textW = (int) std::ceil(font.getStringWidthFloat(label));
            const int groupW = juce::jmin(iconW + gap + textW, juce::jmax(1, r.getWidth() - 8));
            auto group = r.withSizeKeepingCentre(groupW, r.getHeight());
            auto iconR = group.removeFromLeft(iconW).toFloat().withSizeKeepingCentre(12.0f, 12.0f);
            drawIcon(g, iconName, iconR, iconCol, kSidebarIconStroke);
            group.removeFromLeft(juce::jmin(gap, group.getWidth()));
            g.setColour(textCol);
            g.drawText(label, group, juce::Justification::centredLeft, true);
        }
        if (focus)
            drawFocusRing(g, cell, 6.0f);
    };

    drawChip(refreshBtnBounds, icons::refresh, {},
             hoverChrome == ChromeHit::Refresh || focusedKind(NavKind::Refresh),
             false, scanning, focusedKind(NavKind::Refresh));
    drawChip(scanBtnBounds, icons::folder, "Scan Subdirectories",
             hoverChrome == ChromeHit::ScanSubdirs || focusedKind(NavKind::ScanSubdirs),
             includeSubdirs, false, focusedKind(NavKind::ScanSubdirs));
    drawChip(filtersBtnBounds, icons::sliders, "Filters",
             hoverChrome == ChromeHit::Filters || focusedKind(NavKind::Filters),
             filterOpen || activeFilterGroupCount() > 0, false, focusedKind(NavKind::Filters));
}

void FavoritesSidebar::paintRow(juce::Graphics& g, int rowIdx, const juce::Rectangle<int>& r,
                                bool hovered, bool removeZone, bool keyboardFocus)
{
    if (r.isEmpty() || !juce::isPositiveAndBelow(rowIdx, (int) rows.size()))
        return;

    const auto& row = rows[(size_t) rowIdx];
    const auto p = sl();
    const bool activeDir = row.kind == RowKind::SavedDir && dirs[row.index] == active;
    const bool activeStar = row.kind == RowKind::Starred
        && (starredFilter || browseMode == 1);
    const bool activeSearch = row.kind == RowKind::SavedSearch && row.index == activeSearchIdx;
    const bool isActive = activeDir || activeStar || activeSearch;
    const bool lit = isActive || hovered || keyboardFocus;

    const char* icon = icons::folder;
    juce::String label;
    juce::String countText;
    juce::Colour iconCol = p.accent;

    switch (row.kind)
    {
        case RowKind::Open:
            icon = icons::folderOpen;
            label = "Open";
            iconCol = lit ? p.accent : p.text3;
            break;
        case RowKind::SavedDir:
        {
            const juce::File dir(dirs[row.index]);
            label = dir.getFileName().isNotEmpty() ? dir.getFileName() : dirs[row.index];
            // Never enumerate disk for badge counts — that freezes on large libraries.
            // Active folder shows the scanned clip count only.
            if (dirs[row.index] == active && currentClipCount > 0)
                countText = juce::String(currentClipCount);
            break;
        }
        case RowKind::Starred:
            icon = icons::star;
            label = "Favorite Files";
            iconCol = p.gold;
            if (starredCount > 0) countText = juce::String(starredCount);
            break;
        case RowKind::SavedSearch:
        {
            icon = icons::search;
            const auto& entry = savedSearches[(size_t) row.index];
            label = entry.search.query.isNotEmpty() ? entry.search.query : "Search";
            iconCol = lit ? p.accent : p.text3;
            break;
        }
    }

    if (collapsed)
    {
        if (hovered || keyboardFocus)
        {
            g.setColour(p.selectWash);
            g.fillRoundedRectangle(r.toFloat(), 6.0f);
        }
        drawIcon(g, icon, r.toFloat().withSizeKeepingCentre(16.0f, 16.0f),
                 iconCol, kSidebarIconStroke);
        if (keyboardFocus)
            drawFocusRing(g, r.toFloat(), 6.0f);
        return;
    }

    if (isActive || hovered)
    {
        g.setColour(isActive ? p.selectWash : p.selectWash.withAlpha(0.5f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
    }

    auto textArea = r.reduced(sidebarRowPadX(), 0);
    auto iconSlot = textArea.removeFromLeft(sidebarIconCol()).toFloat();
    drawIcon(g, icon, iconSlot.withSizeKeepingCentre(16.0f, 16.0f), iconCol, kSidebarIconStroke);
    textArea.removeFromLeft(sidebarRowIconGap());

    if ((hovered || keyboardFocus)
        && (row.kind == RowKind::SavedDir || row.kind == RowKind::SavedSearch))
    {
        auto xArea = textArea.removeFromRight(18).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
        drawIcon(g, icons::x, xArea,
                 removeZone ? p.text : p.text3, kSidebarIconStroke);
    }
    else if (countText.isNotEmpty())
    {
        auto countArea = textArea.removeFromRight(28);
        g.setColour(p.text3);
        g.setFont(monoFont(11.0f));
        g.drawText(countText, countArea, juce::Justification::centredRight, false);
    }

    g.setColour(lit ? p.text : p.text2);
    g.setFont(uiFontFixed(kCtrlPt, isActive || keyboardFocus));
    g.drawText(label, textArea, juce::Justification::centredLeft, true);

    if (keyboardFocus)
        drawFocusRing(g, r.toFloat(), 6.0f);
}

void FavoritesSidebar::paint(juce::Graphics& g)
{
    const auto p = sl();
    auto b = getLocalBounds().toFloat();
    g.setColour(p.tint);
    g.fillRect(b);
    g.setColour(p.border);
    g.fillRect(getLocalBounds().removeFromRight(1));

    const bool kb = hasKeyboardFocus(true);
    const NavItem* focused = (kb && juce::isPositiveAndBelow(navIndex, (int) navItems.size()))
        ? &navItems[(size_t) navIndex] : nullptr;

    if (!collapsed)
    {
        paintSectionLabel(g, "LIBRARY", libraryHeaderBounds);
        if (!libraryPlusBounds.isEmpty())
        {
            const bool plusHot = hoverChrome == ChromeHit::OpenFolder
                || (focused && focused->kind == NavKind::OpenFolder);
            if (plusHot)
            {
                g.setColour(p.accent.withAlpha(0.12f));
                g.fillRoundedRectangle(libraryPlusBounds.toFloat(), 4.0f);
            }
            drawIcon(g, icons::plus,
                     libraryPlusBounds.toFloat().withSizeKeepingCentre(12.0f, 12.0f),
                     p.accent, 1.6f);
            if (focused && focused->kind == NavKind::OpenFolder)
                drawFocusRing(g, libraryPlusBounds.toFloat(), 4.0f);
        }
    }

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        // Expanded: Open is the top button, not a list row.
        if (!collapsed && rows[(size_t) i].kind == RowKind::Open)
            continue;
        const auto r = rowBounds(i);
        const bool rowFocus = focused != nullptr && focused->kind == NavKind::Row
            && focused->rowIdx == i;
        paintRow(g, i, r, i == hoverRow, i == hoverRow && hoverRemove, rowFocus);
    }

    if (!settingsRowBounds.isEmpty() && !collapsed)
    {
        const bool lit = hoverSettings
            || (focused != nullptr && focused->kind == NavKind::Settings);
        auto area = settingsRowBounds;
        area.removeFromLeft(sidebarRowPadX() + sidebarIconCol() + sidebarRowIconGap());
        g.setColour(lit ? p.text : p.text2);
        g.setFont(uiFontFixed(kCtrlPt, lit));
        g.drawText("Settings", area, juce::Justification::centredLeft, true);
        if (focused != nullptr && focused->kind == NavKind::Settings)
            drawFocusRing(g, settingsRowBounds.toFloat(), 6.0f);
    }
    else if (!settingsRowBounds.isEmpty() && collapsed
             && focused != nullptr && focused->kind == NavKind::Settings)
    {
        drawFocusRing(g, settingsRowBounds.toFloat(), 6.0f);
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
    int w = metrics::scaled(8) * 2 + metrics::scaled(nameColumnW) + metrics::scaled(14);
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
    g.setColour(resizingNameColumn ? ds::acc().withAlpha(0.65f) : ds::ctlb());
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
    // Extra gap so the name never crowds the first meta column.
    row.removeFromLeft(metrics::scaled(14));
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
    g.fillAll(colours::panel());
    paintColumnHeader(g);
    // Match search-field / control border (--ctlb), not the darker hairline.
    g.setColour(ds::ctlb());
    g.fillRect(getLocalBounds().removeFromRight(1));

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
        g.setFont(uiFont(11.0f, true));
        g.setColour(colours::text2());
        g.drawText("SCANNING", block.removeFromTop(16), juce::Justification::centred, false);
        block.removeFromTop(4);

        juce::String count;
        if (scanTotal > 0)
            count = grouped(scanDone) + "  of  " + grouped(scanTotal);
        else
        {
            const int dots = 1 + (int) std::fmod(searchAnimT * 2.0, 3.0);
            count = "Reading files" + juce::String::repeatedString(".", dots);
        }
        g.setFont(uiFont(20.0f, true));
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
        const int labelW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(font, label));
        auto labelR = r.removeFromLeft(labelW);
        g.drawText(label, labelR, juce::Justification::centredLeft, false);
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
            g.setColour(ds::acc());
            g.fillPath(p);
        }
    };

    drawCol(SortColumn::Name, "Name");
    drawCol(SortColumn::Key, "Key");
    drawCol(SortColumn::Tempo, "Tempo");
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
    auto rowR = r.toFloat().reduced(2.0f, 1.0f);

    if (isSelected)
    {
        g.setColour(ds::acc());
        g.fillRoundedRectangle(rowR, 5.0f);
    }
    else if (hovered)
    {
        g.setColour(ds::hoverWash());
        g.fillRoundedRectangle(rowR, 5.0f);
    }
    else if ((displayIdx % 2) == 1)
    {
        g.setColour(ds::rowalt());
        g.fillRoundedRectangle(rowR, 5.0f);
    }

    // Selection text: on-accent ink (dark in dark mode so blue highlight stays readable).
    const auto ink = colours::accentInk();
    const auto textCol = isSelected ? ink : ds::tx();
    const auto metaCol = isSelected ? ink.withAlpha(0.80f) : ds::tx2();

    auto cols = splitRowColumns(r);
    auto row = cols.name;

    const float iconS = 13.0f;
    auto iconArea = row.removeFromLeft(18).toFloat().withSizeKeepingCentre(iconS, iconS);
    if (e.isDirectory)
    {
        drawIcon(g, icons::folder, iconArea,
                 isSelected ? ink : ds::acc(), 1.35f);
    }
    else
    {
        // Outline star (--trk tint) → --stron when favorited; on-accent when selected.
        const auto starCol = e.starred
            ? (isSelected ? ink : ds::stron())
            : (hoverStar ? textCol : (isSelected ? ink.withAlpha(0.55f)
                                                 : ds::trk()));
        drawIcon(g, e.starred ? icons::star : icons::starOutline, iconArea, starCol, 1.35f);
    }
    row.removeFromLeft(6);

    if (e.isDirectory)
    {
        g.setColour(textCol);
        g.setFont(ds::font(ds::Type::Body));
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
    if (columnsVisible.kind)
    {
        // §2 kind chip: 7×7 r2 at 75% opacity + 11px label.
        juce::Colour chip = ds::kindKeys();
        switch (e.kind)
        {
            case ClipKind::Drums:  chip = ds::kindDrums(); break;
            case ClipKind::Bass:   chip = ds::kindBass(); break;
            case ClipKind::Piano:  chip = ds::kindKeys(); break;
            case ClipKind::Lead:   chip = ds::kindKeys(); break;
            case ClipKind::Single: chip = ds::kindPerc(); break;
        }
        auto kindR = cols.kind;
        auto dot = kindR.removeFromLeft(10).toFloat().withSizeKeepingCentre(7.0f, 7.0f);
        g.setColour(isSelected ? ink.withAlpha(0.75f) : chip.withAlpha(0.75f));
        g.fillRoundedRectangle(dot, 2.0f);
        g.setColour(metaCol);
        g.setFont(ds::font(ds::Type::Metadata));
        g.drawText(clipKindName(e.kind), kindR, juce::Justification::centredLeft, true);
    }
    if (columnsVisible.complexity && e.complexity > 0)
        g.drawText(juce::String(e.complexity), cols.complexity, juce::Justification::centredLeft);
    if (columnsVisible.difNotes && e.difNotes > 0)
        g.drawText(juce::String(e.difNotes), cols.difNotes, juce::Justification::centredLeft);
    if (columnsVisible.timeSig)
        g.drawText(juce::String(e.timeSigNum) + "/" + juce::String(e.timeSigDen),
                   cols.timeSig, juce::Justification::centredLeft);
    if (columnsVisible.notes && e.noteCount > 0)
        g.drawText(juce::String(e.noteCount), cols.notes, juce::Justification::centredLeft);

    g.setColour(textCol);
    g.setFont(ds::font(ds::Type::Body));
    g.drawText(e.name, row, juce::Justification::centredLeft, true);
}

void FileListPanel::ListContent::paint(juce::Graphics& g)
{
    if (owner.entries.empty())
    {
        g.setColour(colours::text2());
        g.setFont(uiFont(13.0f, false));
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
    const int starHit = metrics::chromeIconGlyphSize() + 6;
    const bool newHoverStar = valid && !isDir && x >= starLeft && x < starLeft + starHit;

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
    const int starHit = metrics::chromeIconGlyphSize() + 6;
    if (!entry.isDirectory && local.x >= starLeft && local.x < starLeft + starHit)
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

} // namespace pflow
