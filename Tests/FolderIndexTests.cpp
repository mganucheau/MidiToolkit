#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "FolderIndex.h"
#include "EditModel.h"
#include "MidiFileData.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace pflow;

namespace {
juce::File midiParsedRoot()
{
    return juce::File("/Users/ganucheau/Music/Ableton/- Midi Project/Midi_Parsed");
}

template <typename Fn>
double seconds(Fn&& fn)
{
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}
} // namespace

TEST_CASE("Midi_Parsed root lists instantly", "[perf][midi_parsed]")
{
    const auto root = midiParsedRoot();
    if (!root.isDirectory()) SKIP("Midi_Parsed missing");

    QuickDirListing listing;
    const double s = seconds([&]{ listing = listDirectoryQuick(root, FolderIndex::kMaxBrowseNames); });
    WARN("root listDirectoryQuick: " << s << "s dirs=" << listing.directories.size()
         << " midis=" << listing.totalMidiFiles);
    REQUIRE(listing.directories.size() >= 5);
    REQUIRE(listing.totalMidiFiles == 0);
    REQUIRE(s < 0.5);
}

TEST_CASE("Midi_Parsed Keys browse is fast with native listing", "[perf][midi_parsed]")
{
    const auto keys = midiParsedRoot().getChildFile("Keys");
    if (!keys.isDirectory()) SKIP("Keys missing");

    QuickDirListing listing;
    const double s = seconds([&]{ listing = listDirectoryQuick(keys, FolderIndex::kMaxBrowseNames); });
    WARN("Keys listDirectoryQuick: " << s << "s total=" << listing.totalMidiFiles
         << " kept=" << listing.midiFiles.size());
    REQUIRE(listing.totalMidiFiles > 50000);
    REQUIRE((int) listing.midiFiles.size() == FolderIndex::kMaxBrowseNames);
    REQUIRE(s < 2.0);
}

TEST_CASE("FolderIndex binary roundtrip", "[folder_index]")
{
    auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("MidiToolkitIndexTest");
    tmp.deleteRecursively();
    REQUIRE(tmp.createDirectory());
    for (int i = 0; i < 3; ++i)
        tmp.getChildFile("clip" + juce::String(i) + ".mid").replaceWithText("MThd");

    std::vector<StepClip> clips;
    for (int i = 0; i < 3; ++i)
    {
        StepClip c;
        c.filePath = tmp.getChildFile("clip" + juce::String(i) + ".mid").getFullPathName();
        c.name = "clip" + juce::String(i);
        c.bpm = 100.0 + i;
        c.bars = 2 + i;
        clips.push_back(c);
    }
    REQUIRE(FolderIndex::fromClips(tmp, false, clips).save());
    auto loaded = FolderIndex::loadValidClips(tmp, false);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->size() == 3);
    REQUIRE(loaded->at(0).bpm == Catch::Approx(100.0));
    REQUIRE(loaded->at(0).notes.empty());
    tmp.deleteRecursively();
}

TEST_CASE("Parallel parse sample from Drums vs serial", "[perf][midi_parsed]")
{
    const auto drums = midiParsedRoot().getChildFile("Drums");
    if (!drums.isDirectory()) SKIP("Drums missing");

    auto files = collectMidiFiles(drums, false);
    if (files.size() < 200) SKIP("not enough drums");

    constexpr int N = 200;
    std::vector<StepClip> serial((size_t) N);
    const double serialSec = seconds([&]{
        for (int i = 0; i < N; ++i)
            serial[(size_t) i] = makeStepClip(parseMidiFile(files.getReference(i)));
    });

    std::vector<StepClip> parallel((size_t) N);
    const double parallelSec = seconds([&]{
        std::atomic<int> done{0};
        juce::ThreadPool pool(juce::jmax(2, (int) std::thread::hardware_concurrency()));
        for (int i = 0; i < N; ++i)
            pool.addJob([&files, &parallel, &done, i]{
                parallel[(size_t) i] = makeStepClip(parseMidiFile(files.getReference(i)));
                ++done;
            });
        while (done.load() < N) juce::Thread::sleep(1);
    });

    WARN("parse 200 drums serial=" << serialSec << "s parallel=" << parallelSec << "s");
    REQUIRE(serial[0].filePath.isNotEmpty());
    REQUIRE(parallelSec < serialSec * 0.9);
}

TEST_CASE("Fingerprint of Midi_Parsed root is cheap", "[perf][midi_parsed]")
{
    const auto root = midiParsedRoot();
    if (!root.isDirectory()) SKIP("Midi_Parsed missing");
    FolderFingerprint fp;
    const double s = seconds([&]{ fp = FolderIndex::computeFingerprint(root, false); });
    WARN("root fingerprint: " << s << "s count=" << fp.fileCount);
    REQUIRE(fp.fileCount == 0);
    REQUIRE(s < 0.5);
}
