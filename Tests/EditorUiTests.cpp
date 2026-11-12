#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EditModel.h"
#include "BrowserPanels.h"
#include "EffectsInspector.h"
#include "EffectsInspectorWidgets.h"
#include "TransportBar.h"
#include "TestHelpers.h"
#include <catch2/catch_approx.hpp>
#include <CoreFoundation/CoreFoundation.h>

using namespace pflow;

namespace {

juce::Component* findById(juce::Component* root, const juce::String& id)
{
    if (root->getComponentID() == id)
        return root;
    for (int i = 0; i < root->getNumChildComponents(); ++i)
        if (auto* found = findById(root->getChildComponent(i), id))
            return found;
    return nullptr;
}

void openAncestorSection(juce::Component* c)
{
    for (auto* p = c; p != nullptr; p = p->getParentComponent())
        if (auto* sec = dynamic_cast<fx::Section*>(p))
        {
            sec->setOpen(true);
            if (auto* parent = sec->getParentComponent())
                parent->resized();
            return;
        }
}

juce::File makeLeadBarFolder(const juce::String& name)
{
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name);
    dir.deleteRecursively();
    dir.createDirectory();
    // First bar empty: notes start at beat 4 (bar 2 of 2).
    auto clip = test::makeClipWithNotes({ { 60, 100, 4.0, 1.0, 1 },
                                          { 64, 100, 6.0, 1.5, 1 } }, 8.0);
    writeMidiFile(clip, dir.getChildFile("lead.mid"), 120.0);
    return dir;
}

} // namespace

TEST_CASE("Trim button trims and restores through the real editor UI", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    // Keep content scale at 1.0 so hit-testing matches component bounds.
    tweaks().size.store((int) ContentSize::Medium);
    tweaks().appearance.store((int) Appearance::Light);
    tweaks().density.store((int) Density::Compact);

    const auto dir = makeLeadBarFolder("MidiToolkitUiTest");
    const auto path = dir.getChildFile("lead.mid").getFullPathName();

    MidiToolkitProcessor proc;
    proc.effectsOpen = true;
    proc.passthrough = false;
    proc.passthroughActive.store(false);

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    REQUIRE(ed != nullptr);
    auto* browser = dynamic_cast<MidiToolkitEditor*>(ed.get());
    REQUIRE(browser != nullptr);
    browser->ensureAndSelectFile(dir.getChildFile("lead.mid"));
    ed->setVisible(true);   // hit-testing requires a visible component tree
    ed->setSize(1100, 620);

    auto* trimComp = findById(ed.get(), "btnTrim");
    REQUIRE(trimComp != nullptr);
    openAncestorSection(trimComp);
    ed->resized();

    auto* trim = dynamic_cast<juce::Button*>(trimComp);
    REQUIRE(trim != nullptr);
    REQUIRE(trim->isVisible());
    REQUIRE(trim->isEnabled());   // the clip has an empty lead bar

    // Nothing overlaps the button: a click at its centre must reach it.
    const auto centre = ed->getLocalPoint(trim, trim->getLocalBounds().getCentre());
    auto* hit = ed->getComponentAt(centre.x, centre.y);
    INFO("component at trim centre: " << (hit != nullptr ? hit->getComponentID() + " / "
        + juce::String(typeid(*hit).name()) : juce::String("null")));
    REQUIRE((hit == trim || trim->isParentOf(hit)));

    // Button::triggerClick is asynchronous (needs a message pump), so invoke
    // the wired handler directly; enabled + hit-test above cover reachability.
    REQUIRE(trim->onClick != nullptr);

    // Click: trims the empty lead bar.
    trim->onClick();
    REQUIRE(proc.clipEdits.count(path) == 1);
    CHECK(proc.clipEdits[path].removedBars == std::vector<int>{ 0 });
    CHECK(trim->isEnabled());

    // Click again: restores.
    trim->onClick();
    CHECK(proc.clipEdits[path].removedBars.empty());

    // And a third time: trims again (it toggles indefinitely).
    trim->onClick();
    CHECK(proc.clipEdits[path].removedBars == std::vector<int>{ 0 });

    ed = nullptr;
    dir.deleteRecursively();
}

TEST_CASE("Fold and pitch-lock buttons are reachable; lock persists a template", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto dir = makeLeadBarFolder("MidiToolkitUiTest2");

    MidiToolkitProcessor proc;
    proc.effectsOpen = true;
    proc.passthrough = false;
    proc.passthroughActive.store(false);

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* browser = dynamic_cast<MidiToolkitEditor*>(ed.get());
    REQUIRE(browser != nullptr);
    browser->ensureAndSelectFile(dir.getChildFile("lead.mid"));
    browser->toggleEditorFold(); // startup leaves the piano roll folded
    ed->setVisible(true);
    ed->setSize(1300, 620);

    auto* fold = dynamic_cast<juce::Button*>(findById(ed.get(), "btnFold"));
    REQUIRE(fold != nullptr);
    REQUIRE(fold->isEnabled());
    fold->onClick();   // no crash, toggles fold state

    // Effects lock lives in the Effects inspector header.
    auto* lock = dynamic_cast<juce::Button*>(findById(ed.get(), "btnLock"));
    REQUIRE(lock != nullptr);
    REQUIRE(proc.sectionLocks == 0);
    lock->onClick();
    CHECK(proc.sectionLocks == toolkitLock::All);
    CHECK(proc.effectsLock);
    lock->onClick();
    CHECK(proc.sectionLocks == 0);
    CHECK_FALSE(proc.effectsLock);

    ed = nullptr;
    dir.deleteRecursively();
}

TEST_CASE("Locked toolkit sections keep their settings on the next file", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    tweaks().size.store((int) ContentSize::Medium);

    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("MidiToolkitLockBrowse");
    dir.deleteRecursively();
    dir.createDirectory();

    const auto sparseFile = dir.getChildFile("sparse.mid");
    const auto denseFile = dir.getChildFile("dense.mid");
    writeMidiFile(test::makeClipWithNotes({ { 60, 100, 0.0, 1.0, 1 } }, 4.0), sparseFile, 120.0);
    {
        std::vector<NoteEvent> notes;
        for (int i = 0; i < 32; ++i)
            notes.push_back({ 48 + (i % 12), 40 + (i % 50), i * 0.25, 0.2, 1 });
        MidiClip dense;
        dense.name = "Dense";
        dense.notes = notes;
        dense.lengthBeats = 8.0;
        writeMidiFile(dense, denseFile, 120.0);
    }

    const auto sparse = makeStepClip(parseMidiFile(sparseFile));
    const auto dense = makeStepClip(parseMidiFile(denseFile));
    REQUIRE(sparse.complexity != dense.complexity);

    MidiToolkitProcessor proc;
    proc.effectsOpen = true;
    proc.passthrough = false;
    proc.passthroughActive.store(false);

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* browser = dynamic_cast<MidiToolkitEditor*>(ed.get());
    REQUIRE(browser != nullptr);
    browser->ensureAndSelectFile(sparseFile);

    const auto sparsePath = sparseFile.getFullPathName();
    auto& groove = proc.grooveFor(sparsePath);
    groove.delayAmount = 60;
    groove.complexityTarget = -1;

    auto* lock = dynamic_cast<juce::Button*>(findById(ed.get(), "btnLock"));
    REQUIRE(lock != nullptr);
    lock->onClick();

    CHECK(proc.sectionLocks == toolkitLock::All);
    CHECK(proc.lockedGroove.delayAmount == 60);
    CHECK(proc.lockedGroove.complexityTarget == sparse.complexity);

    browser->ensureAndSelectFile(denseFile);
    const auto densePath = denseFile.getFullPathName();
    CHECK(proc.sectionLocks == toolkitLock::All);
    REQUIRE(proc.clipGrooves.count(densePath) == 1);
    CHECK(proc.clipGrooves[densePath].delayAmount == 60);
    CHECK(proc.clipGrooves[densePath].complexityTarget == sparse.complexity);
    CHECK(proc.clipGrooves[densePath].complexityTarget != dense.complexity);

    ed = nullptr;
    dir.deleteRecursively();
}

TEST_CASE("Reopening the editor restores the scanned folder", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    tweaks().size.store((int) ContentSize::Medium);

    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("MidiToolkitRestoreFolder");
    dir.deleteRecursively();
    dir.createDirectory();
    const auto midi = dir.getChildFile("phrase.mid");
    writeMidiFile(test::makeClipWithNotes({ { 60, 100, 0.0, 1.0, 1 } }, 4.0), midi, 120.0);

    MidiToolkitProcessor proc;
    proc.browseMode = 0;
    proc.includeSubdirs = false;
    proc.browserSessionSearch = {};
    proc.lastBrowserDir = dir.getFullPathName();
    proc.selectedClipPath = midi.getFullPathName();

    {
        std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
        const auto deadline = juce::Time::getMillisecondCounter() + 4000;
        while (juce::Time::getMillisecondCounter() < deadline)
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
    }

    CHECK(proc.lastBrowserDir == dir.getFullPathName());
    CHECK(proc.selectedClipPath == midi.getFullPathName());
    dir.deleteRecursively();
}

TEST_CASE("Opening a folder lists its MIDI files", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    tweaks().size.store((int) ContentSize::Medium);

    auto parent = juce::File::getSpecialLocation(juce::File::tempDirectory)
                      .getChildFile("MidiToolkitOpenFolder");
    parent.deleteRecursively();
    parent.createDirectory();
    const auto top = parent.getChildFile("top.mid");
    const auto childDir = parent.getChildFile("inner");
    childDir.createDirectory();
    const auto nested = childDir.getChildFile("nested.mid");
    writeMidiFile(test::makeClipWithNotes({ { 60, 100, 0.0, 1.0, 1 } }, 4.0), top, 120.0);
    writeMidiFile(test::makeClipWithNotes({ { 64, 100, 0.0, 1.0, 1 } }, 4.0), nested, 120.0);

    MidiToolkitProcessor proc;
    proc.browseMode = 0;
    proc.includeSubdirs = false;
    proc.browserSessionSearch = {};
    proc.lastBrowserDir = {};
    proc.selectedClipPath = {};

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* browser = dynamic_cast<MidiToolkitEditor*>(ed.get());
    REQUIRE(browser != nullptr);

    auto pumpUntil = [](auto pred)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + 4000;
        while (juce::Time::getMillisecondCounter() < deadline && !pred())
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
    };

    browser->setRootDirectory(parent, false, false);
    pumpUntil([&] { return browser->browserEntryCount() >= 2; });
    CHECK(browser->browserEntryCount() >= 2);

    browser->setRootDirectory(childDir, false, false);
    pumpUntil([&] { return proc.selectedClipPath == nested.getFullPathName(); });
    CHECK(browser->browserEntryCount() >= 1);
    CHECK(proc.selectedClipPath == nested.getFullPathName());

    ed = nullptr;
    parent.deleteRecursively();
}

TEST_CASE("Locked trim stays on for a clip with no empty bars", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    tweaks().size.store((int) ContentSize::Medium);

    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("MidiToolkitTrimLock");
    dir.deleteRecursively();
    dir.createDirectory();
    const auto gapped = dir.getChildFile("gapped.mid");
    const auto full = dir.getChildFile("full.mid");
    writeMidiFile(test::makeClipWithNotes({ { 60, 100, 4.0, 1.0, 1 } }, 8.0), gapped, 120.0);
    writeMidiFile(test::makeClipWithNotes({ { 60, 100, 0.0, 1.0, 1 } }, 4.0), full, 120.0);

    MidiToolkitProcessor proc;
    proc.effectsOpen = true;
    proc.passthrough = false;
    proc.passthroughActive.store(false);

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* browser = dynamic_cast<MidiToolkitEditor*>(ed.get());
    REQUIRE(browser != nullptr);
    browser->ensureAndSelectFile(gapped);

    auto* trim = dynamic_cast<juce::Button*>(findById(ed.get(), "btnTrim"));
    REQUIRE(trim != nullptr);
    trim->onClick();
    CHECK(trim->getToggleState());
    CHECK(proc.editFor(gapped.getFullPathName()).hasTrim());

    auto* lock = dynamic_cast<juce::Button*>(findById(ed.get(), "btnLock"));
    REQUIRE(lock != nullptr);
    lock->onClick();
    CHECK(proc.lockAutoTrim);

    browser->ensureAndSelectFile(full);
    CHECK(proc.lockAutoTrim);
    CHECK(trim->getToggleState());
    CHECK_FALSE(proc.editFor(full.getFullPathName()).hasTrim());

    trim->onClick();
    CHECK_FALSE(proc.lockAutoTrim);
    CHECK_FALSE(trim->getToggleState());

    ed = nullptr;
    dir.deleteRecursively();
}

TEST_CASE("Passthrough keeps Playback and Form controls visible", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    EffectsInspector fx;
    fx.setSize(320, 800);
    fx.playbackSection().setOpen(true);
    fx.formSection().setOpen(true);
    fx.effectsSection().setOpen(true);
    const int formH = fx.formSection().idealHeight();
    REQUIRE(fx.playbackSection().isVisible());
    REQUIRE(formH > 0);

    fx.setLiveStreamMode(true);
    // Form stays open with its controls even before a clip is loaded.
    CHECK(fx.playbackSection().isVisible());
    CHECK(fx.formSection().idealHeight() == formH);
    CHECK(fx.contentHeight() > 0);

    fx.setHasClip(true);
    CHECK(fx.formSection().idealHeight() == formH);

    fx.setLiveStreamMode(false);
    CHECK(fx.playbackSection().isVisible());
    CHECK(fx.formSection().idealHeight() == formH);
}

TEST_CASE("File table keyboard nav follows sorted display order", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    FileListPanel panel;
    panel.setSize(400, 300);

    std::vector<FileListEntry> entries(3);
    entries[0].name = "zebra";
    entries[0].bars = 8;
    entries[0].bpm = 120;
    entries[1].name = "alpha";
    entries[1].bars = 2;
    entries[1].bpm = 90;
    entries[2].name = "middle";
    entries[2].bars = 4;
    entries[2].bpm = 100;
    panel.setEntries(std::move(entries));

    // Default sort: Name ascending → alpha, middle, zebra
    std::vector<int> visited;
    panel.onSelect = [&](int idx) { visited.push_back(idx); };

    panel.setSelectedIndex(1); // alpha (entry 1)
    REQUIRE(panel.getSelectedIndex() == 1);

    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 2); // middle
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 0); // zebra

    // Sort by Bars ascending → alpha(2), middle(4), zebra(8)
    panel.setSort(FileListPanel::SortColumn::Bars, true);
    panel.setSelectedIndex(1); // alpha
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 2); // middle
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 0); // zebra

    // Bars descending → zebra, middle, alpha
    panel.setSort(FileListPanel::SortColumn::Bars, false);
    panel.setSelectedIndex(0); // zebra
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 2); // middle
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 1); // alpha
}

TEST_CASE("A large file list does not build a giant view", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    FileListPanel panel;
    panel.setSize(420, 320);

    std::vector<FileListEntry> entries(4000);
    for (int i = 0; i < (int) entries.size(); ++i)
        entries[(size_t) i].name = "clip " + juce::String(i);
    panel.setEntries(std::move(entries));

    juce::Viewport* viewport = nullptr;
    for (int i = 0; i < panel.getNumChildComponents(); ++i)
        if (auto* view = dynamic_cast<juce::Viewport*>(panel.getChildComponent(i)))
            viewport = view;
    REQUIRE(viewport != nullptr);
    REQUIRE(viewport->getViewedComponent() != nullptr);
    CHECK(viewport->getViewedComponent()->getHeight() < 800);
}

TEST_CASE("Toolkit fold keeps dirty rows and collapses clean sections", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    EffectsInspector fx;
    fx.setSize(metrics::effectsPaneWidth(), 800);

    GrooveParams g;
    g.swing = 35;
    fx.setGroove(g, juce::dontSendNotification);
    // Playback/Timing/… start open for discoverability; fold keeps only dirty rows.
    fx.timingSection().setOpen(false);
    REQUIRE(fx.timingSection().isVisible());
    REQUIRE(fx.timingSection().visibleRowCount() == 1);
    REQUIRE(fx.timingSection().modifiedCount() == 1);

    EffectsInspector clean;
    clean.setSize(metrics::effectsPaneWidth(), 800);
    for (fx::Section* sec : { &clean.playbackSection(), &clean.timingSection(),
                              &clean.performanceSection(), &clean.pitchSection(),
                              &clean.effectsSection() })
    {
        sec->setOpen(false);
        REQUIRE(sec->getHeight() == fx::kSectionHeaderH);
    }
}

TEST_CASE("Toolkit paired delay rows and disabled dependents", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    EffectsInspector fx;
    fx.setSize(metrics::effectsPaneWidth(), 800);

    GrooveParams g;
    g.delayAmount = 25;
    g.delayFeedback = 40;
    fx.setGroove(g, juce::dontSendNotification);
    REQUIRE(fx.delayPairVisible());
    REQUIRE_FALSE(fx.delayFeedbackVisible());

    REQUIRE_FALSE(fx.articulationStrengthEnabled());
    g = GrooveParams{};
    g.articulationIndex = (int) Articulation::Legato;
    fx.setGroove(g, juce::dontSendNotification);
    REQUIRE(fx.articulationStrengthEnabled());
}

TEST_CASE("Note filter legend follows key and mode", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    EffectsInspector fx;
    ClipEdit edit;
    edit.root = 9; // A
    edit.mode = Mode::Aeolian;
    fx.setEdit(edit, juce::dontSendNotification);
    REQUIRE(fx.noteFilterLegendLead() == "In A aeolian");
    REQUIRE(metrics::effectsPaneWidth() == metrics::scaled(ds::region::toolkitW));
}

TEST_CASE("Header wordmark tracks the traffic-light zone and drops Toolkit before MIDI", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    tweaks().size.store((int) ContentSize::Medium);

    TransportBar bar;
    bar.setSize(1600, 40);
    bar.setLightsZoneWidth(88);
    REQUIRE(bar.getTitleBounds().getX() == 88);
    REQUIRE(bar.logoWordmarkLevel() == 2);

    bar.setLightsZoneWidth(0);
    REQUIRE(bar.getTitleBounds().getX() == 12);

    const auto font = ds::font(ds::Type::Logo);
    auto measure = [](const juce::Font& f, const juce::String& text)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(f, text, 0.0f, 0.0f);
        if (glyphs.getNumGlyphs() <= 0)
            return 0.0f;
        return glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true).getWidth();
    };
    const int textW = (int) std::ceil(measure(font, "MIDI") + measure(font, " ") + measure(font, "Toolkit"));
    REQUIRE(std::abs(bar.logoIdealWidth() - textW) <= 1);

    bool sawFull = false, sawMidi = false, sawHidden = false;
    int previous = 3;
    for (int w = 1600; w >= 220; w -= 8)
    {
        bar.setSize(w, 40);
        const int level = bar.logoWordmarkLevel();
        REQUIRE(level <= previous);
        if (level == 2) sawFull = true;
        if (level == 1) sawMidi = true;
        if (level == 0) sawHidden = true;
        previous = level;
    }
    REQUIRE(sawFull);
    REQUIRE(sawMidi);
    REQUIRE(sawHidden);
}
