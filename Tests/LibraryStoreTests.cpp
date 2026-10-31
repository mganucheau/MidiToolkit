#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "LibraryStore.h"
#include "PluginProcessor.h"
#include "Theme.h"

using namespace pflow;
using Catch::Approx;

TEST_CASE("LibraryStore persists stars and search cache to disk", "[Library][qa]")
{
    const auto file = LibraryStore::libraryFile();
    const auto backup = file.getSiblingFile("library.xml.bak-test");
    if (file.existsAsFile())
        file.copyFileTo(backup);
    file.deleteFile();

    {
        LibraryStore store;
        store.toggleStarred("/tmp/a.mid");
        store.toggleStarred("/tmp/b.mid");

        BrowserSearch s;
        s.query = "groove";
        s.bpmMin = 120.0;
        s.bpmMax = 120.0;
        s.subdirs = true;
        store.putSearchCache(juce::File("/tmp/midi"), s, { "/tmp/midi/a.mid", "/tmp/midi/b.mid" });

        SavedSearchEntry entry;
        entry.name = "groove 120";
        entry.search = s;
        entry.rootPath = "/tmp/midi";
        entry.resultPaths.add("/tmp/midi/a.mid");
        entry.resultPaths.add("/tmp/midi/b.mid");
        store.addSavedSearch(entry);
        store.save(); // flush async writes before reloading
    }

    {
        LibraryStore loaded;
        loaded.load();
        REQUIRE(loaded.isStarred("/tmp/a.mid"));
        REQUIRE(loaded.isStarred("/tmp/b.mid"));
        REQUIRE(loaded.savedSearches.size() == 1);
        REQUIRE(loaded.savedSearches[0].name == "groove 120");
        REQUIRE(loaded.savedSearches[0].rootPath == "/tmp/midi");
        REQUIRE(loaded.savedSearches[0].resultPaths.size() == 2);
        REQUIRE(loaded.savedSearches[0].resultPaths[0] == "/tmp/midi/a.mid");
        REQUIRE(loaded.savedSearches[0].resultPaths[1] == "/tmp/midi/b.mid");

        BrowserSearch s;
        s.query = "groove";
        s.bpmMin = 120.0;
        s.bpmMax = 120.0;
        s.subdirs = true;
        REQUIRE(LibraryStore::searchesEqual(loaded.savedSearches[0].search, s));
        const auto* cached = loaded.findSearchCache(juce::File("/tmp/midi"), s);
        REQUIRE(cached != nullptr);
        REQUIRE(cached->resultPaths.size() == 2);
        REQUIRE(cached->rootPath == "/tmp/midi");
    }

    file.deleteFile();
    if (backup.existsAsFile())
    {
        backup.moveFileTo(file);
    }
}

TEST_CASE("Processor loads library stars on construction", "[Library][qa]")
{
    const auto file = LibraryStore::libraryFile();
    const auto backup = file.getSiblingFile("library.xml.bak-test2");
    if (file.existsAsFile())
        file.copyFileTo(backup);
    file.deleteFile();

    {
        LibraryStore store;
        store.toggleStarred("/tmp/starred-clip.mid");
        store.save();
    }

    MidiToolkitProcessor processor;
    REQUIRE(processor.isStarred("/tmp/starred-clip.mid"));

    file.deleteFile();
    if (backup.existsAsFile())
        backup.moveFileTo(file);
}

TEST_CASE("Shared UI session reloads on a new processor instance", "[Library][qa]")
{
    const auto file = LibraryStore::libraryFile();
    const auto backup = file.getSiblingFile("library.xml.bak-session");
    if (file.existsAsFile())
        file.copyFileTo(backup);
    file.deleteFile();

    {
        MidiToolkitProcessor a;
        a.lastBrowserDir = "/tmp/shared-midi-lib";
        a.browseMode = 2;
        a.includeSubdirs = true;
        a.browserSessionSearch.query = "groove";
        a.browserSessionSearch.bpmMin = 120.0;
        a.browserSessionSearch.bpmMax = 128.0;
        a.selectedClipPath = "/tmp/shared-midi-lib/a.mid";
        a.editorOpen = true;
        a.previewOpen = false;
        a.passthrough = true;
        a.nameColumnWidth = 360;
        a.columnVisibility.complexity = true;
        tweaks().appearance.store((int) Appearance::Light);
        a.persistSharedUiSession();
        a.library().flush();
    }

    {
        MidiToolkitProcessor b;
        // No host state — must still restore workspace from the app library.
        b.setStateInformation(nullptr, 0);
        REQUIRE(b.lastBrowserDir == "/tmp/shared-midi-lib");
        REQUIRE(b.browseMode == 2);
        REQUIRE(b.includeSubdirs);
        REQUIRE(b.browserSessionSearch.query == "groove");
        REQUIRE(b.browserSessionSearch.bpmMin == Approx(120.0));
        REQUIRE(b.selectedClipPath == "/tmp/shared-midi-lib/a.mid");
        REQUIRE(b.editorOpen);
        REQUIRE_FALSE(b.previewOpen);
        REQUIRE(b.passthrough);
        REQUIRE(b.passthroughActive.load());
        REQUIRE(b.nameColumnWidth == 360);
        REQUIRE(b.columnVisibility.complexity);
        REQUIRE(tweaks().appearance.load() == (int) Appearance::Light);
    }

    // Restore prior library and bump the save generation so any late async
    // writes from destroyed processors cannot clobber the restore.
    file.deleteFile();
    if (backup.existsAsFile())
        backup.moveFileTo(file);
    {
        LibraryStore drain;
        drain.load();
        drain.flush();
    }
}

TEST_CASE("Favorites accrue across sessions and host state loads", "[Library][qa]")
{
    const auto file = LibraryStore::libraryFile();
    const auto backup = file.getSiblingFile("library.xml.bak-test3");
    if (file.existsAsFile())
        file.copyFileTo(backup);
    file.deleteFile();

    {
        LibraryStore store;
        store.toggleStarred("/tmp/fav-a.mid");
        store.toggleStarred("/tmp/fav-b.mid");
        REQUIRE(store.starredFiles.size() == 2);
    }

    // New store (new session) keeps both, then adds a third.
    {
        LibraryStore store;
        store.load();
        REQUIRE(store.isStarred("/tmp/fav-a.mid"));
        REQUIRE(store.isStarred("/tmp/fav-b.mid"));
        store.toggleStarred("/tmp/fav-c.mid");
        REQUIRE(store.starredFiles.size() == 3);
    }

    // Host state with a subset must not wipe the accrued library.
    {
        MidiToolkitProcessor processor;
        REQUIRE(processor.isStarred("/tmp/fav-a.mid"));
        REQUIRE(processor.isStarred("/tmp/fav-b.mid"));
        REQUIRE(processor.isStarred("/tmp/fav-c.mid"));

        juce::MemoryBlock state;
        // Simulate a project that only remembered one favorite.
        {
            juce::XmlElement xml("MidiToolkitState");
            auto* star = xml.createNewChildElement("StarredFile");
            star->setAttribute("path", "/tmp/fav-a.mid");
            juce::AudioProcessor::copyXmlToBinary(xml, state);
        }
        // Use public setStateInformation path.
        processor.setStateInformation(state.getData(), (int) state.getSize());
        REQUIRE(processor.isStarred("/tmp/fav-a.mid"));
        REQUIRE(processor.isStarred("/tmp/fav-b.mid"));
        REQUIRE(processor.isStarred("/tmp/fav-c.mid"));
        REQUIRE(processor.starredFiles.size() >= 3);
    }

    file.deleteFile();
    if (backup.existsAsFile())
        backup.moveFileTo(file);
}
