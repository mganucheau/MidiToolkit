// Renders the editor in its main states to build/snapshots/*.png so the
// Groove Panel redesign can be checked visually without a DAW.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TestHelpers.h"
#include <CoreFoundation/CoreFoundation.h>

using namespace pflow;

namespace {

juce::File snapshotDir()
{
    return juce::File(__FILE__).getParentDirectory().getParentDirectory()
        .getChildFile("build").getChildFile("snapshots");
}

void pumpFor(int ms)
{
    const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < deadline)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, false);
}

void save(juce::Component& c, const juce::String& name)
{
    auto img = c.createComponentSnapshot(c.getLocalBounds(), true, 2.0f);
    auto dir = snapshotDir();
    dir.createDirectory();
    auto f = dir.getChildFile(name + ".png");
    f.deleteFile();
    juce::FileOutputStream out(f);
    if (out.openedOk())
        juce::PNGImageFormat().writeImageToStream(img, out);
}

juce::File makeLibrary()
{
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("House Pack 124");
    dir.deleteRecursively();
    dir.createDirectory();

    auto bass = [](int root, int seed)
    {
        std::vector<NoteEvent> n;
        for (int i = 0; i < 16; ++i)
            if ((i * 7 + seed) % 3 != 0)
                n.push_back({ 36 + root + ((i + seed) % 4 == 0 ? 12 : 0), 70 + (i * 13 % 50), i * 0.5, 0.4, 1 });
        return n;
    };
    auto chords = [](int root)
    {
        std::vector<NoteEvent> n;
        for (int b = 0; b < 4; ++b)
            for (int k : { 0, 3, 7, 10 })
                n.push_back({ 60 + root + k + (b == 2 ? 5 : 0), 80 + b * 8, b * 4.0, 3.5, 1 });
        return n;
    };
    auto lead = [](int root, int seed)
    {
        std::vector<NoteEvent> n;
        const int sc[] = { 0, 2, 3, 5, 7, 8, 10 };
        for (int i = 0; i < 24; ++i)
            n.push_back({ 72 + root + sc[(i * 3 + seed) % 7], 60 + (i * 17 % 60), i * 0.5 + ((i % 3) ? 0.0 : 0.25), 0.3, 1 });
        return n;
    };
    auto drums = [](int seed)
    {
        std::vector<NoteEvent> n;
        for (int i = 0; i < 32; ++i)
        {
            if (i % 4 == 0) n.push_back({ 36, 120, i * 0.25 * 2, 0.1, 10 });
            if (i % 8 == 4) n.push_back({ 38, 110, i * 0.25 * 2, 0.1, 10 });
            if ((i + seed) % 2 == 0) n.push_back({ 42, 50 + (i * 11 % 60), i * 0.25 * 2, 0.1, 10 });
        }
        return n;
    };
    struct F { const char* name; std::vector<NoteEvent> notes; double len; double bpm; };
    std::vector<F> files = {
        { "Rolling Sub", bass(5, 1), 8.0, 124 }, { "Offbeat Pluck", bass(5, 2), 8.0, 124 },
        { "Late Night Stabs", chords(5), 16.0, 124 }, { "Bass 124 F min", bass(5, 3), 16.0, 124 },
        { "Glass Arp", lead(0, 1), 8.0, 126 }, { "Warehouse Hats", drums(1), 4.0, 124 },
        { "Reese Hold", bass(5, 4), 32.0, 124 }, { "Soft Ninths", chords(8), 32.0, 122 },
        { "Hook Line", lead(5, 2), 16.0, 124 }, { "Skippy Garage Kit", drums(2), 8.0, 130 },
        { "Organ Chops", chords(10), 16.0, 122 }, { "Bell Motif", lead(5, 3), 16.0, 124 },
        { "Rim Shuffle", drums(3), 8.0, 124 }, { "Slide Run", bass(0, 5), 8.0, 124 },
        { "Chord Memory", chords(0), 16.0, 124 }, { "Saw Lead", lead(5, 4), 16.0, 124 },
    };
    for (auto& f : files)
    {
        MidiClip clip;
        clip.name = f.name;
        clip.notes = f.notes;
        clip.lengthBeats = f.len;
        writeMidiFile(clip, dir.getChildFile(juce::String(f.name) + ".mid"), f.bpm);
    }
    return dir;
}

} // namespace

TEST_CASE("Snapshot the Groove Panel editor states", "[snapshot]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    tweaks().size.store((int) ContentSize::Medium);
    tweaks().appearance.store((int) Appearance::Dark);

    const auto dir = makeLibrary();

    // The processor persists its UI session to the user's real library.xml.
    // Remove leftovers from earlier runs, back the file up, and restore it at the end.
    const auto libFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("Midi Toolkit").getChildFile("library.xml");
    {
        MidiToolkitProcessor cleanup;
        cleanup.removeSavedBrowserDir(dir.getFullPathName());
    }
    juce::MemoryBlock libBackup;
    const bool hadLib = libFile.existsAsFile() && libFile.loadFileAsData(libBackup);

    {
    MidiToolkitProcessor proc;
    proc.passthrough = false;
    proc.passthroughActive.store(false);
    proc.effectsOpen = false;
    proc.editorOpen = false;
    proc.browseMode = 0;
    proc.addSavedBrowserDir(dir.getFullPathName());

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* browser = dynamic_cast<MidiToolkitEditor*>(ed.get());
    REQUIRE(browser != nullptr);
    ed->setVisible(true);

    browser->setRootDirectory(dir, false, true);
    pumpFor(2500);
    browser->ensureAndSelectFile(dir.getChildFile("Bass 124 F min.mid"));
    pumpFor(600);

    ed->setSize(560, 720);
    pumpFor(300);
    save(*ed, "01-slim");

    browser->setFilterOpenForTesting(true);
    pumpFor(300);
    save(*ed, "02-filters");
    browser->setFilterOpenForTesting(false);

    browser->setGridViewForTesting(true);
    ed->setSize(680, 720);
    pumpFor(300);
    save(*ed, "03-grid");
    browser->setGridViewForTesting(false);

    browser->toggleEffectsForTesting();
    browser->toggleEditorFold();
    ed->setSize(1500, 760);
    pumpFor(400);
    save(*ed, "04-roll-toolkit");
    browser->toggleEditorFold();
    browser->toggleEffectsForTesting();

    browser->setSidebarCollapsedForTesting(true);
    ed->setSize(460, 720);
    pumpFor(300);
    save(*ed, "05-rail");
    browser->setSidebarCollapsedForTesting(false);

    tweaks().appearance.store((int) Appearance::Light);
    ed->sendLookAndFeelChange();
    ed->setSize(560, 720);
    pumpFor(300);
    save(*ed, "06-light");
    tweaks().appearance.store((int) Appearance::Dark);

    CHECK(snapshotDir().getChildFile("01-slim.png").existsAsFile());
    ed = nullptr;
    }

    if (hadLib)
        libFile.replaceWithData(libBackup.getData(), libBackup.getSize());
    dir.deleteRecursively();
}
