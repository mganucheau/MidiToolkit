#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "ClipNoteCache.h"
#include "EditModel.h"
#include "FolderIndex.h"
#include "MidiFileData.h"
#include <atomic>
#include <chrono>

using namespace pflow;

namespace {

juce::File writeMidi(const juce::String& name, int noteCount, bool stackedChord)
{
    juce::MidiMessageSequence seq;
    const int tpq = 960;
    for (int i = 0; i < noteCount; ++i)
    {
        const int pitch = 48 + (i % 14);
        const double start = (i / 4) * 0.5;
        const double end = start + 0.45;
        seq.addEvent(juce::MidiMessage::noteOn(1, pitch, (juce::uint8) 96), start * tpq);
        seq.addEvent(juce::MidiMessage::noteOff(1, pitch), end * tpq);
    }
    if (stackedChord)
    {
        for (int p = 60; p < 67; ++p)
        {
            seq.addEvent(juce::MidiMessage::noteOn(1, p, (juce::uint8) 100), 0.0);
            seq.addEvent(juce::MidiMessage::noteOff(1, p), tpq);
        }
    }
    seq.addEvent(juce::MidiMessage::tempoMetaEvent(500000), 0.0);
    seq.updateMatchedPairs();

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote(tpq);
    midi.addTrack(seq);

    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile(name);
    juce::FileOutputStream out(file);
    REQUIRE(out.openedOk());
    REQUIRE(midi.writeTo(out));
    out.flush();
    return file;
}

template <typename Fn>
double seconds(Fn&& fn)
{
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

TEST_CASE("Scan progress publishes at a steady cadence, not once per file", "[scan][efficiency]")
{
    std::atomic<juce::int64> last { 0 };
    REQUIRE(claimScanProgressPublish(1, 1000, last, 1000, 100));
    REQUIRE_FALSE(claimScanProgressPublish(2, 1000, last, 1050, 100));
    REQUIRE(claimScanProgressPublish(3, 1000, last, 1200, 100));
    REQUIRE(claimScanProgressPublish(1000, 1000, last, 1210, 100));
}

TEST_CASE("Browse scan matches full parse and does not keep notes", "[scan][efficiency]")
{
    const auto file = writeMidi("midi-toolkit-scan-meta.mid", 240, true);
    const auto full = makeStepClip(parseMidiFile(file));
    const auto meta = scanMidiBrowseMeta(file);

    REQUIRE(full.notesResident);
    REQUIRE(full.noteCount > 200);
    REQUIRE_FALSE(meta.notesResident);
    REQUIRE(meta.notes.empty());
    REQUIRE(meta.noteCount == full.noteCount);
    REQUIRE(meta.difNotes == full.difNotes);
    REQUIRE(meta.bars == full.bars);
    REQUIRE(meta.bpm == Catch::Approx(full.bpm));
    REQUIRE(meta.complexity == full.complexity);
    REQUIRE(meta.kind == full.kind);
    REQUIRE(meta.root == full.root);
    file.deleteFile();
}

TEST_CASE("Classifying a dense clip stays off the quadratic path", "[scan][efficiency]")
{
    MidiClip clip;
    clip.bpm = 120.0;
    clip.lengthBeats = 64.0;
    clip.notes.reserve(4000);
    for (int i = 0; i < 4000; ++i)
    {
        NoteEvent n;
        n.noteNumber = 40 + (i % 20);
        n.startBeat = (i % 128) * 0.25;
        n.lengthBeats = 0.5;
        n.velocity = 100;
        n.channel = 1;
        clip.notes.push_back(n);
    }

    StepClip step;
    const double s = seconds([&] { step = makeStepClip(clip); });
    REQUIRE(step.noteCount == 4000);
    REQUIRE(step.notesResident);
    REQUIRE(s < 0.5);
}

TEST_CASE("Note cache keeps the last few clips and drops the oldest", "[scan][efficiency]")
{
    ClipNoteCache cache;
    for (int i = 0; i < ClipNoteCache::kCapacity + 3; ++i)
    {
        StepClip c;
        c.filePath = "clip-" + juce::String(i);
        c.notesResident = true;
        RollNote n;
        n.pitch = 60 + i;
        c.notes.push_back(n);
        cache.store(std::move(c));
    }

    REQUIRE(cache.size() == ClipNoteCache::kCapacity);
    REQUIRE_FALSE(cache.contains("clip-0"));
    REQUIRE(cache.contains("clip-" + juce::String(ClipNoteCache::kCapacity + 2)));

    auto hit = cache.find("clip-" + juce::String(ClipNoteCache::kCapacity + 2));
    REQUIRE(hit.has_value());
    REQUIRE(hit->notes.size() == 1);
    REQUIRE(hit->notes.front().pitch == 60 + ClipNoteCache::kCapacity + 2);
}
