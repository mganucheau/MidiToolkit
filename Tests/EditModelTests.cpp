#include "EditModel.h"
#include "TestHelpers.h"
#include <catch2/catch_approx.hpp>

using namespace pflow;
using Catch::Approx;

namespace {

StepClip makeClip(std::initializer_list<RollNote> notes, int bars, int root = 0)
{
    StepClip c;
    c.name = "Test";
    c.root = root;
    c.bars = bars;
    c.notes = notes;
    return c;
}

} // namespace

TEST_CASE("pitchName maps MIDI 60 to C4", "[editmodel]")
{
    CHECK(pitchName(60) == "C4");
    CHECK(pitchName(61) == "C#4");
    CHECK(pitchName(59) == "B3");
    CHECK(pitchName(0) == "C-1");
    CHECK(pitchName(127) == "G9");
}

TEST_CASE("isBlackKeyPitch matches {1,3,6,8,10} pitch classes", "[editmodel]")
{
    CHECK_FALSE(isBlackKeyPitch(60));  // C
    CHECK(isBlackKeyPitch(61));        // C#
    CHECK(isBlackKeyPitch(63));        // D#
    CHECK_FALSE(isBlackKeyPitch(64));  // E
    CHECK(isBlackKeyPitch(66));        // F#
    CHECK(isBlackKeyPitch(68));        // G#
    CHECK(isBlackKeyPitch(70));        // A#
    CHECK_FALSE(isBlackKeyPitch(71));  // B
}

TEST_CASE("modeIntervals match the seven-mode table", "[editmodel]")
{
    CHECK(modeIntervals(Mode::Ionian)     == std::array<int, 7> { 0, 2, 4, 5, 7, 9, 11 });
    CHECK(modeIntervals(Mode::Dorian)     == std::array<int, 7> { 0, 2, 3, 5, 7, 9, 10 });
    CHECK(modeIntervals(Mode::Phrygian)   == std::array<int, 7> { 0, 1, 3, 5, 7, 8, 10 });
    CHECK(modeIntervals(Mode::Lydian)     == std::array<int, 7> { 0, 2, 4, 6, 7, 9, 11 });
    CHECK(modeIntervals(Mode::Mixolydian) == std::array<int, 7> { 0, 2, 4, 5, 7, 9, 10 });
    CHECK(modeIntervals(Mode::Aeolian)    == std::array<int, 7> { 0, 2, 3, 5, 7, 8, 10 });
    CHECK(modeIntervals(Mode::Locrian)    == std::array<int, 7> { 0, 1, 3, 5, 6, 8, 10 });
}

TEST_CASE("fitToScale matches prototype golden values", "[editmodel]")
{
    // Golden values computed by running the verbatim Prototype/mb/data.jsx
    // fitToScale in Node (ties at equal |d| resolve to the lower candidate).
    CHECK(fitToScale(61, 0, Mode::Ionian) == 60);    // C# -> C
    CHECK(fitToScale(63, 0, Mode::Ionian) == 62);    // D# -> D
    CHECK(fitToScale(66, 0, Mode::Ionian) == 65);    // F# -> F
    CHECK(fitToScale(68, 0, Mode::Ionian) == 67);    // G# -> G
    CHECK(fitToScale(70, 0, Mode::Ionian) == 69);    // A# -> A
    CHECK(fitToScale(64, 0, Mode::Aeolian) == 63);   // E  -> D# in C minor
    CHECK(fitToScale(61, 9, Mode::Dorian) == 60);    // C# -> C in A Dorian
    CHECK(fitToScale(37, 6, Mode::Locrian) == 36);
    CHECK(fitToScale(0, 0, Mode::Phrygian) == 0);    // in scale, unchanged
    CHECK(fitToScale(127, 11, Mode::Lydian) == 126);
}

TEST_CASE("analyseClipScale detects viable keys/modes and octave span", "[editmodel]")
{
    // C major triad C4-E4-G4 → covers C Ionian and related modes/roots.
    auto clip = makeClip({ { 0, 60, 0.0, 2.0 }, { 1, 64, 0.0, 2.0 }, { 2, 67, 0.0, 2.0 } }, 1, 0);
    const auto a = analyseClipScale(clip);
    CHECK((a.keyMask & (uint16_t) (1u << 0)) != 0); // C
    CHECK((a.modeMask & (uint16_t) (1u << (int) Mode::Ionian)) != 0);
    CHECK(a.primaryRoot == 0);
    CHECK(a.primaryMode == Mode::Ionian);
    CHECK(a.octaveRange == 0); // single octave

    // Notes spanning C3..C5 → octaveRange at least 2.
    auto wide = makeClip({ { 0, 48, 0.0, 1.0 }, { 1, 72, 1.0, 1.0 } }, 1, 0);
    CHECK(analyseClipScale(wide).octaveRange >= 2);
}

TEST_CASE("analyseClipScale seeds Major when Lydian covers F#", "[editmodel]")
{
    // C Lydian content (C E F# G) — Lydian covers it, but audition defaults to
    // Major so Fit-to-Scale in C snaps F# rather than treating it as in-key.
    auto clip = makeClip({
        { 0, 60, 0.0, 1.0 },  // C
        { 1, 64, 1.0, 1.0 },  // E
        { 2, 66, 2.0, 1.0 },  // F#
        { 3, 67, 3.0, 1.0 },  // G
    }, 1, 0);
    const auto a = analyseClipScale(clip);
    CHECK((a.modeMask & (uint16_t) (1u << (int) Mode::Lydian)) != 0);
    CHECK(a.primaryRoot == 0);
    CHECK(a.primaryMode == Mode::Ionian);
    CHECK(auditionMode(Mode::Lydian) == Mode::Ionian);
    CHECK(auditionMode(Mode::Mixolydian) == Mode::Ionian);
    CHECK(auditionMode(Mode::Dorian) == Mode::Dorian);
    CHECK(auditionMode(Mode::Aeolian) == Mode::Aeolian);

    ClipEdit e;
    e.fitScale = true;
    e.root = a.primaryRoot;
    e.mode = a.primaryMode;
    const auto r = resolveClip(clip, e);
    REQUIRE(r.notes.size() == 4);
    for (const auto& n : r.notes)
        CHECK(pitchInScale(n.pitch, 0, Mode::Ionian));
    CHECK(r.notes[2].pitch == 65); // F# -> F
}

TEST_CASE("analyseClipScale does not collapse Dorian to same-root Minor", "[editmodel]")
{
    // D Dorian (D E F G A B C) with clip.root=D. Must not seed D Aeolian
    // (which has Bb) — that would snap B→Bb under Fit.
    auto clip = makeClip({
        { 0, 62, 0.0, 1.0 },  // D
        { 1, 64, 1.0, 1.0 },  // E
        { 2, 65, 2.0, 1.0 },  // F
        { 3, 67, 3.0, 1.0 },  // G
        { 4, 69, 4.0, 1.0 },  // A
        { 5, 71, 5.0, 1.0 },  // B
        { 6, 60, 6.0, 1.0 },  // C
    }, 1, 2);
    const auto a = analyseClipScale(clip);
    // Prefer relative Major when it wins; never D Aeolian for this set.
    CHECK_FALSE((a.primaryRoot == 2 && a.primaryMode == Mode::Aeolian));
    if (a.primaryRoot == 2)
        CHECK(a.primaryMode == Mode::Dorian);

    ClipEdit e;
    e.fitScale = true;
    e.root = 2;
    e.mode = Mode::Dorian;
    const auto r = resolveClip(clip, e);
    for (const auto& n : r.notes)
        CHECK(pitchInScale(n.pitch, 2, Mode::Dorian));
    // B must remain B, not Bb.
    bool hasB = false;
    for (const auto& n : r.notes)
        if ((n.pitch % 12 + 12) % 12 == 11)
            hasB = true;
    CHECK(hasB);
}

TEST_CASE("editIsClean detects default edits", "[editmodel]")
{
    ClipEdit e;
    CHECK(editIsClean(e));

    SECTION("octave") { e.octave = 1; CHECK_FALSE(editIsClean(e)); }
    SECTION("octaveRange") { e.octaveRange = 2; CHECK_FALSE(editIsClean(e)); }
    SECTION("pitchRange") { e.pitchMin = 48; CHECK_FALSE(editIsClean(e)); }
    SECTION("fitScale") { e.fitScale = true; CHECK_FALSE(editIsClean(e)); }
    SECTION("mapToRoot") { e.mapToRoot = true; CHECK_FALSE(editIsClean(e)); }
    SECTION("moves") { e.moves[3] = { 1, 0 }; CHECK_FALSE(editIsClean(e)); }
    SECTION("zero-delta move stays clean") { e.moves[3] = { 0, 0 }; CHECK(editIsClean(e)); }
    SECTION("trim") { e.removedBars = { 0 }; CHECK_FALSE(editIsClean(e)); }
    SECTION("root/mode alone are not edits")
    {
        e.root = 4;
        e.mode = Mode::Lydian;
        CHECK(editIsClean(e));
    }
}

TEST_CASE("resolveClip applies absolute octave and per-note moves", "[editmodel]")
{
    // Clip starts at C4 (60); absolute octave 3 → C3 (48).
    auto clip = makeClip({ { 0, 60, 0.0, 4.0 }, { 1, 64, 4.0, 2.0 } }, 1);
    ClipEdit e;
    e.octave = 3;
    e.moves[1] = { 2, 3 };

    const auto r = resolveClip(clip, e);
    REQUIRE(r.notes.size() == 2);
    CHECK(r.notes[0].pitch == 48);
    CHECK(r.notes[0].start == Approx(0.0));
    CHECK_FALSE(r.notes[0].moved);
    CHECK(r.notes[1].pitch == 64 + 2 - 12);
    CHECK(r.notes[1].start == Approx(7.0));
    CHECK(r.notes[1].moved);

    // Source clip untouched (non-destructive contract).
    CHECK(clip.notes[1].pitch == 64);
    CHECK(clip.notes[1].start == Approx(4.0));

    // -1 keeps the clip's native octave.
    e.octave = -1;
    e.moves.clear();
    CHECK(resolveClip(clip, e).notes[0].pitch == 60);
}

TEST_CASE("resolveClip octave range folds into the target window", "[editmodel]")
{
    // Lowest note C3 (48) anchors octave 3. One octave folds into C3–B3.
    auto clip = makeClip({ { 0, 48, 0.0, 2.0 }, { 1, 72, 2.0, 2.0 } }, 1);
    ClipEdit e;
    e.octaveRange = 1;
    const auto folded = resolveClip(clip, e);
    REQUIRE(folded.notes.size() == 2);
    CHECK(folded.notes[0].pitch == 48);
    CHECK(folded.notes[1].pitch == 48); // 72 wraps by 24, two octaves, into C3

    e.outsideRange = OutsideRangeMode::Clip;
    const auto clipped = resolveClip(clip, e);
    CHECK(clipped.notes[0].pitch == 48);
    CHECK(clipped.notes[1].pitch == 59); // clamped to the top of the one-octave window
}

TEST_CASE("resolveClip octave fold matches an octave window", "[editmodel]")
{
    // Octave 1 starts at MIDI 24. A 1-octave fold matches block transpose, not ±12.
    auto clip = makeClip({ { 0, 0, 0.0, 1.0 }, { 1, 11, 1.0, 1.0 }, { 2, 36, 2.0, 1.0 }, { 3, 72, 3.0, 1.0 } }, 1);
    ClipEdit e;
    e.octave = 1;
    e.octaveRange = 1;
    e.outsideRange = OutsideRangeMode::Fold;
    const auto r = resolveClip(clip, e);
    REQUIRE(r.notes.size() == 4);
    CHECK(r.notes[0].pitch == 24);
    CHECK(r.notes[1].pitch == 35);
    CHECK(r.notes[2].pitch == 24);
    CHECK(r.notes[3].pitch == 24);

    e.octaveRange = 2;
    const auto wide = resolveClip(clip, e);
    CHECK(wide.notes[0].pitch == 24); // 0 wraps up into the 24-wide window
    CHECK(mapPitchToOctaveWindow(12, 1, 2, OutsideRangeMode::Fold) == 36);
}

TEST_CASE("resolveClip pitch range folds notes into window", "[editmodel]")
{
    auto clip = makeClip({ { 0, 36, 0.0, 2.0 }, { 1, 84, 2.0, 2.0 } }, 1);
    ClipEdit e;
    e.pitchMin = 48;
    e.pitchMax = 72;
    const auto r = resolveClip(clip, e);
    REQUIRE(r.notes.size() == 2);
    CHECK(r.notes[0].pitch >= 48);
    CHECK(r.notes[0].pitch <= 72);
    CHECK(r.notes[1].pitch >= 48);
    CHECK(r.notes[1].pitch <= 72);
}

TEST_CASE("resolveClip reverse flips note starts within the clip", "[editmodel]")
{
    // 1 bar = 16 steps; note at 0 len 4 → start 12; note at 8 len 4 → start 4.
    auto clip = makeClip({ { 0, 60, 0.0, 4.0 }, { 1, 64, 8.0, 4.0 } }, 1);
    ClipEdit e;
    e.reverse = true;
    const auto r = resolveClip(clip, e);
    REQUIRE(r.notes.size() == 2);
    CHECK(r.notes[0].start == Catch::Approx(12.0));
    CHECK(r.notes[1].start == Catch::Approx(4.0));
}

TEST_CASE("resolveClip invert mirrors pitches around the midpoint", "[editmodel]")
{
    auto clip = makeClip({ { 0, 60, 0.0, 2.0 }, { 1, 64, 2.0, 2.0 } }, 1);
    ClipEdit e;
    e.invert = true;
    const auto r = resolveClip(clip, e);
    REQUIRE(r.notes.size() == 2);
    // Midpoint of 60..64 is 62; 60→64, 64→60.
    CHECK(r.notes[0].pitch == 64);
    CHECK(r.notes[1].pitch == 60);
}

TEST_CASE("editIsClean detects reverse and invert", "[editmodel]")
{
    ClipEdit e;
    CHECK(editIsClean(e));
    e.reverse = true;
    CHECK_FALSE(editIsClean(e));
    e.reverse = false;
    e.invert = true;
    CHECK_FALSE(editIsClean(e));
}

TEST_CASE("resolveClip map-to-root transposes by nearest direction", "[editmodel]")
{
    // Clip root G (7), target C (0): d = (0-7) = -7 -> wraps to +5.
    auto clip = makeClip({ { 0, 55, 0.0, 4.0 } }, 1, 7);
    ClipEdit e;
    e.mapToRoot = true;
    e.root = 0;
    CHECK(resolveClip(clip, e).notes[0].pitch == 60);

    // Clip root C (0), target G (7): d = 7 -> wraps to -5.
    clip.root = 0;
    e.root = 7;
    CHECK(resolveClip(clip, e).notes[0].pitch == 50);

    // No clip root -> no transpose.
    clip.root = -1;
    CHECK(resolveClip(clip, e).notes[0].pitch == 55);
}

TEST_CASE("resolveClip composes map-to-root then fit-to-scale", "[editmodel]")
{
    // Root D (2) clip: D and F#. Map to C, then snap into C Aeolian.
    auto clip = makeClip({ { 0, 62, 0.0, 2.0 }, { 1, 66, 2.0, 2.0 } }, 1, 2);
    ClipEdit e;
    e.mapToRoot = true;
    e.fitScale = true;
    e.root = 0;
    e.mode = Mode::Aeolian;

    const auto r = resolveClip(clip, e);
    CHECK(r.notes[0].pitch == 60);  // D -> C, in scale
    CHECK(r.notes[1].pitch == 63);  // F# -> E -> D# (C minor third)
}

TEST_CASE("resolveClip fit-to-scale survives octave-range remap", "[editmodel]")
{
    // Wide chromatic span + Fit to F major must not leave out-of-scale pitches
    // after octave-range compression (which remaps chromatically).
    auto clip = makeClip({
        { 0, 53, 0.0, 1.0 },  // F3
        { 1, 54, 1.0, 1.0 },  // F#
        { 2, 58, 2.0, 1.0 },  // Bb
        { 3, 66, 3.0, 1.0 },  // F#
        { 4, 77, 4.0, 1.0 },  // F5
    }, 1);
    ClipEdit e;
    e.fitScale = true;
    e.root = 5; // F
    e.mode = Mode::Ionian;
    e.octaveRange = 2;

    const auto r = resolveClip(clip, e);
    REQUIRE_FALSE(r.notes.empty());
    for (const auto& n : r.notes)
        CHECK(pitchInScale(n.pitch, 5, Mode::Ionian));
}

TEST_CASE("resolveClip trim shifts notes and shrinks bars", "[editmodel]")
{
    auto clip = makeClip({ { 0, 60, 16.0, 4.0 }, { 1, 62, 40.0, 2.0 } }, 4);
    ClipEdit e;
    e.removedBars = { 0, 3 };

    const auto r = resolveClip(clip, e);
    CHECK(r.bars == 2);
    CHECK(r.notes[0].start == Approx(0.0));
    CHECK(r.notes[1].start == Approx(24.0));

    SECTION("bars never drop below 1")
    {
        e.removedBars = { 0, 1, 2, 3 };
        CHECK(resolveClip(clip, e).bars == 1);
    }

    SECTION("middle empty bar is compacted")
    {
        auto mid = makeClip({ { 0, 60, 0.0, 4.0 }, { 1, 62, 32.0, 4.0 } }, 3);
        ClipEdit t;
        t.removedBars = { 1 };
        const auto out = resolveClip(mid, t);
        CHECK(out.bars == 2);
        CHECK(out.notes[0].start == Approx(0.0));
        CHECK(out.notes[1].start == Approx(16.0));
    }
}

TEST_CASE("emptyBars finds every fully-empty measure", "[editmodel]")
{
    std::vector<RollNote> notes { { 0, 60, 0.0, 4.0 }, { 1, 62, 32.0, 4.0 } };
    CHECK(emptyBars(notes, 3) == std::vector<int>{ 1 });

    std::vector<RollNote> edges { { 0, 60, 17.0, 2.0 }, { 1, 62, 36.0, 4.0 } };
    CHECK(emptyBars(edges, 4) == std::vector<int>({ 0, 3 }));
}

TEST_CASE("emptyEdgeBars counts fully-empty edge bars", "[editmodel]")
{
    std::vector<RollNote> notes { { 0, 60, 17.0, 2.0 }, { 1, 62, 36.0, 4.0 } };

    const auto e = emptyEdgeBars(notes, 4);
    CHECK(e.lead == 1);   // bar 0 empty (first note at step 17)
    CHECK(e.tail == 1);   // last note ends at step 40 -> bar 3 empty

    CHECK(emptyEdgeBars({}, 4).lead == 0);
    CHECK(emptyEdgeBars({}, 4).tail == 0);

    // Note ending exactly on a bar boundary leaves the next bar empty.
    std::vector<RollNote> exact { { 0, 60, 0.0, 16.0 } };
    CHECK(emptyEdgeBars(exact, 2).lead == 0);
    CHECK(emptyEdgeBars(exact, 2).tail == 1);
}

TEST_CASE("editBadges lists only non-default transforms", "[editmodel]")
{
    auto clip = makeClip({ { 0, 60, 0.0, 4.0 } }, 2, 0);

    ClipEdit e;
    CHECK(editBadges(clip, e).empty());

    e.octave = 2;
    e.fitScale = true;
    e.mapToRoot = true;
    e.root = 3;               // D#
    e.mode = Mode::Aeolian;
    e.moves[0] = { 0, 4 };
    e.removedBars = { 0 };

    const auto badges = editBadges(clip, e);
    REQUIRE(badges.size() == 5);
    CHECK(badges[0].label == "Oct 2");
    CHECK(badges[1].label == "D# Minor");
    CHECK(badges[2].label == "-> D# root");
    CHECK(badges[3].label == "1 note moved");
    CHECK(badges[4].label == "Trim -1 bar");

    SECTION("plurals")
    {
        e.moves[1] = { 1, 0 };
        e.removedBars = { 0, 1 };
        const auto b = editBadges(clip, e);
        CHECK(b[3].label == "2 notes moved");
        CHECK(b[4].label == "Trim -2 bars");
    }

    SECTION("native octave is not badged")
    {
        ClipEdit native;
        native.octave = -1;
        CHECK(editBadges(clip, native).empty());
    }
}

TEST_CASE("locked absolute octave retargets clips across native registers", "[editmodel]")
{
    // Clip in octave 5 (C5=72) with locked octave 4 → pitches drop one octave.
    auto clip = makeClip({ { 0, 72, 0.0, 2.0 }, { 1, 76, 2.0, 2.0 } }, 1);
    CHECK(clipReferenceOctave(clip) == 5);

    ClipEdit locked;
    locked.octave = 4;
    ClipEdit target;
    applyPitchLock(locked, target);

    const auto r = resolveClip(clip, target);
    REQUIRE(r.notes.size() == 2);
    CHECK(r.notes[0].pitch == 60);
    CHECK(r.notes[1].pitch == 64);
}

TEST_CASE("applyPitchLock copies pitch fields, preserves moves and trim", "[editmodel]")
{
    ClipEdit locked;
    locked.octave = 1;
    locked.fitScale = true;
    locked.mapToRoot = true;
    locked.root = 0;              // C
    locked.mode = Mode::Aeolian;

    locked.extendMult = 8;

    ClipEdit target;
    target.moves[7] = { 2, -4 };
    target.removedBars = { 0 };
    target.octave = -2;
    target.root = 9;
    target.extendMult = 4;

    applyPitchLock(locked, target);
    CHECK(target.octave == 1);
    CHECK(target.fitScale);
    CHECK(target.mapToRoot);
    CHECK(target.root == 0);
    CHECK(target.mode == Mode::Aeolian);
    CHECK(target.moves.at(7).dPitch == 2);    // per-note moves untouched
    CHECK(target.moves.at(7).dStep == -4);
    CHECK(target.removedBars == std::vector<int>{ 0 });  // trim untouched
    CHECK(target.extendMult == 4);                       // extend is Playback
}

TEST_CASE("playback lock holds extend and trim mode across clips", "[editmodel]")
{
    auto gapped = makeClip({ { 0, 60, 16.0, 4.0 } }, 2);
    auto full = makeClip({ { 0, 60, 0.0, 32.0 } }, 2);

    ClipEdit locked;
    locked.extendMult = 4;

    ClipEdit leftover;
    leftover.extendMult = 1;
    leftover.removedBars = { 1 };
    leftover.octave = 3;
    applyPlaybackLock(locked, leftover, false, gapped);
    CHECK(leftover.extendMult == 4);
    CHECK(leftover.removedBars.empty());
    CHECK(leftover.octave == 3);

    ClipEdit gappedEdit;
    applyPlaybackLock(locked, gappedEdit, true, gapped);
    CHECK(gappedEdit.extendMult == 4);
    CHECK(gappedEdit.removedBars == std::vector<int>{ 0 });

    ClipEdit fullEdit;
    fullEdit.removedBars = { 0 };
    applyPlaybackLock(locked, fullEdit, true, full);
    CHECK(fullEdit.extendMult == 4);
    CHECK(fullEdit.removedBars.empty());
}

TEST_CASE("parseMidiFile honours end-of-track length for trailing empty bars", "[editmodel]")
{
    // One beat of notes but an end-of-track marker at 16 beats: the clip
    // should keep its intended 4-bar length so Trim has something to do.
    juce::MidiFile mf;
    constexpr int tpq = 480;
    mf.setTicksPerQuarterNote(tpq);
    juce::MidiMessageSequence seq;
    seq.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0.0);
    seq.addEvent(juce::MidiMessage::noteOff(1, 60), tpq * 1.0);
    seq.addEvent(juce::MidiMessage::endOfTrack(), tpq * 16.0);
    seq.updateMatchedPairs();
    mf.addTrack(seq);

    const auto file = test::tempMidiFile("eot-length.mid");
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    {
        juce::FileOutputStream out(file);
        REQUIRE(out.openedOk());
        REQUIRE(mf.writeTo(out, 0));
    }

    const auto clip = parseMidiFile(file);
    REQUIRE(clip.notes.size() == 1);
    CHECK(clip.lengthBeats == Approx(16.0));

    const auto step = makeStepClip(clip);
    CHECK(step.bars == 4);
    const auto edges = emptyEdgeBars(step.notes, step.bars);
    CHECK(edges.lead == 0);
    CHECK(edges.tail == 3);   // bars 2-4 are empty and trimmable

    test::removeTempMidiFile(file);
}

TEST_CASE("makeStepClip converts beats to 16th steps with stable ids", "[editmodel]")
{
    MidiClip mc = test::makeClipWithNotes({
        { 60, 100, 0.0, 1.0, 1 },     // C4, beat 0, 1 beat
        { 43, 90, 2.5, 0.5, 1 },      // G2
    }, 8.0);

    const auto s = makeStepClip(mc);
    REQUIRE(s.notes.size() == 2);
    CHECK(s.notes[0].id == 0);
    CHECK(s.notes[1].id == 1);
    CHECK(s.notes[0].start == Approx(0.0));
    CHECK(s.notes[0].len == Approx(4.0));
    CHECK(s.notes[1].start == Approx(10.0));
    CHECK(s.notes[1].len == Approx(2.0));
    CHECK(s.notes[0].fileVelocity == 100);
    CHECK(s.bars == 2);   // 8 beats = 2 bars
    CHECK(s.root >= 0);
}

TEST_CASE("complexity stays low for a single note and rises for a dense part", "[editmodel]")
{
    const auto sparse = makeStepClip(test::makeClipWithNotes({
        { 60, 100, 0.0, 4.0, 1 }
    }, 16.0));

    std::vector<NoteEvent> busy;
    for (int bar = 0; bar < 4; ++bar)
    {
        for (int step = 0; step < 8; ++step)
        {
            const double beat = bar * 4.0 + step * 0.5;
            busy.push_back({ 60 + (step % 7), 40 + step * 10, beat, 0.4, 1 });
            busy.push_back({ 64 + (step % 5), 100, beat, 0.4, 1 });
        }
    }
    MidiClip denseClip;
    denseClip.name = "Dense";
    denseClip.notes = std::move(busy);
    denseClip.lengthBeats = 16.0;
    const auto dense = makeStepClip(denseClip);

    CHECK(sparse.complexity < 25);
    CHECK(dense.complexity > sparse.complexity + 10);
    CHECK(dense.complexity < 100);
}

TEST_CASE("channel-1 GM drum loop is classified as Drums", "[editmodel]")
{
    // Typical sample-pack drum loop on channel 1 (not channel 10).
    std::vector<NoteEvent> hits;
    for (int bar = 0; bar < 2; ++bar)
    {
        const double b = bar * 4.0;
        hits.push_back({ 36, 100, b + 0.0, 0.2, 1 });
        hits.push_back({ 42, 80,  b + 0.0, 0.15, 1 });
        hits.push_back({ 42, 70,  b + 0.5, 0.15, 1 });
        hits.push_back({ 38, 110, b + 1.0, 0.2, 1 });
        hits.push_back({ 42, 80,  b + 1.0, 0.15, 1 });
        hits.push_back({ 42, 70,  b + 1.5, 0.15, 1 });
        hits.push_back({ 36, 100, b + 2.0, 0.2, 1 });
        hits.push_back({ 42, 80,  b + 2.0, 0.15, 1 });
        hits.push_back({ 42, 70,  b + 2.5, 0.15, 1 });
        hits.push_back({ 38, 110, b + 3.0, 0.2, 1 });
        hits.push_back({ 42, 80,  b + 3.0, 0.15, 1 });
        hits.push_back({ 42, 70,  b + 3.5, 0.15, 1 });
    }
    MidiClip clip;
    clip.name = "Beat";
    clip.notes = std::move(hits);
    clip.lengthBeats = 8.0;
    const auto s = makeStepClip(clip);
    CHECK(s.kind == ClipKind::Drums);
    CHECK(s.kindConfidence >= 70);
    CHECK((s.tagMask & clipTag::Backbeat) != 0);
}

TEST_CASE("piano with one channel-10 click is not Drums", "[editmodel]")
{
    MidiClip clip = test::makeClipWithNotes({
        { 60, 90, 0.0, 2.0, 1 },
        { 64, 85, 0.0, 2.0, 1 },
        { 67, 85, 0.0, 2.0, 1 },
        { 60, 90, 2.0, 2.0, 1 },
        { 64, 85, 2.0, 2.0, 1 },
        { 67, 85, 2.0, 2.0, 1 },
        { 37, 40, 0.0, 0.1, 10 }, // stray drum click on ch 10
    }, 4.0);
    const auto s = makeStepClip(clip);
    CHECK(s.kind == ClipKind::Keys);
    CHECK(s.hasDrumContent);
}

TEST_CASE("low chordal clip is Piano not Bass", "[editmodel]")
{
    MidiClip clip = test::makeClipWithNotes({
        { 36, 90, 0.0, 2.0, 1 }, // C2
        { 40, 85, 0.0, 2.0, 1 }, // E2
        { 43, 85, 0.0, 2.0, 1 }, // G2
        { 36, 90, 2.0, 2.0, 1 },
        { 40, 85, 2.0, 2.0, 1 },
        { 43, 85, 2.0, 2.0, 1 },
    }, 4.0);
    const auto s = makeStepClip(clip);
    CHECK(s.kind == ClipKind::Keys);
}

TEST_CASE("drum complexity rises with hat layer not pitch-class count", "[editmodel]")
{
    MidiClip sparse;
    sparse.name = "SparseKick";
    sparse.lengthBeats = 4.0;
    sparse.notes = {
        { 36, 100, 0.0, 0.2, 1 },
        { 38, 110, 1.0, 0.2, 1 },
        { 36, 100, 2.0, 0.2, 1 },
        { 38, 110, 3.0, 0.2, 1 },
    };

    MidiClip withHats = sparse;
    withHats.name = "WithHats";
    for (double t = 0.0; t < 4.0; t += 0.25)
        withHats.notes.push_back({ 42, 60, t, 0.1, 1 });

    const auto a = makeStepClip(sparse);
    const auto b = makeStepClip(withHats);
    CHECK(a.kind == ClipKind::Drums);
    CHECK(b.kind == ClipKind::Drums);
    CHECK(b.complexity > a.complexity);
}

TEST_CASE("filename keywords hint Bass Drums Keys Lead and Arp", "[editmodel]")
{
    CHECK(hintKindFromFileName("BA_Funky_01.mid") == ClipKind::Bass);
    CHECK(hintKindFromFileName("Bassline_120_Cmin.mid") == ClipKind::Bass);
    CHECK(hintKindFromFileName("Deep_Bass_Groove.mid") == ClipKind::Bass);
    CHECK(hintKindFromFileName("DSS_Bass_Cmin_01.mid") == ClipKind::Bass);
    CHECK(hintKindFromFileName("DSS_Bass_05_F#.mid") == ClipKind::Bass);
    CHECK(hintKindFromFileName("Synth Bass.mid") == ClipKind::Bass);
    CHECK(hintKindFromFileName("Synth_Bass_Cmin.mid") == ClipKind::Bass);
    CHECK(hintKindFromFileName("808_Sub_C.mid") == ClipKind::Bass);
    CHECK(hintKindFromFileName("DSS_808_01.mid") == ClipKind::Bass);
    {
        StepClip stub;
        stub.name = "DSS_Bass_05_F#";
        stub.filePath = "/packs/DSS_Bass_05_F#.mid";
        stub.kind = ClipKind::Lead;
        CHECK(applyFilenameKindTag(stub));
        CHECK(stub.kind == ClipKind::Bass);
    }
    CHECK(hintKindFromFileName("Kit_01_BoomBap.mid") == ClipKind::Drums);
    CHECK(hintKindFromFileName("Drums_Loop_A.mid") == ClipKind::Drums);
    CHECK(hintKindFromFileName("Keys_Rhodes_Pad.mid") == ClipKind::Keys);
    CHECK(hintKindFromFileName("Piano_Chords_Em.mid") == ClipKind::Keys);
    CHECK(hintKindFromFileName("KY_Chords_01.mid") == ClipKind::Keys);
    CHECK(hintKindFromFileName("Lead_Synth_Hook.mid") == ClipKind::Lead);
    CHECK(hintKindFromFileName("LD_Pluck_01.mid") == ClipKind::Lead);
    CHECK(hintKindFromFileName("SY_Pluck_A.mid") == ClipKind::Lead);
    CHECK(hintKindFromFileName("Arp_Cmaj_120.mid") == ClipKind::Arp);
    CHECK(hintKindFromFileName("SEQ_Up_01.mid") == ClipKind::Arp);
    CHECK(hintKindFromFileName("Hats_16th_Loop.mid") == ClipKind::Drums);
    CHECK(hintKindFromFileName("HH_Closed_01.mid") == ClipKind::Drums);
    CHECK(hintKindFromFileName("SN_Rim_01.mid") == ClipKind::Drums);
    CHECK(hintKindFromFileName("Groove_A_120.mid") == ClipKind::Drums);
    CHECK(hintKindFromFileName("Top_Loop_01.mid") == ClipKind::Drums);
    CHECK(kindKeywordFromFileName("Untitled_01.mid") == std::nullopt);
}

TEST_CASE("bass filename wins over ambiguous mid register mono line", "[editmodel]")
{
    MidiClip clip = test::makeClipWithNotes({
        { 48, 100, 0.0, 0.5, 1 },
        { 50, 90, 0.5, 0.5, 1 },
        { 48, 100, 1.0, 0.5, 1 },
        { 47, 90, 1.5, 0.5, 1 },
    }, 4.0);
    clip.name = "BA_Walking_01";
    clip.filePath = "/Library/BA_Walking_01.mid";
    const auto s = makeStepClip(clip);
    CHECK(s.kind == ClipKind::Bass);
}

TEST_CASE("C1 kick pattern classifies as Drums without drum filename", "[editmodel]")
{
    MidiClip clip;
    clip.name = "Untitled_Loop";
    clip.lengthBeats = 4.0;
    // Ableton C1 kick on 1 and 3, plus a couple of other kit hits.
    clip.notes = {
        { 36, 110, 0.0, 0.2, 1 },
        { 42, 70, 0.5, 0.1, 1 },
        { 38, 100, 1.0, 0.2, 1 },
        { 42, 70, 1.5, 0.1, 1 },
        { 36, 110, 2.0, 0.2, 1 },
        { 42, 70, 2.5, 0.1, 1 },
        { 38, 100, 3.0, 0.2, 1 },
        { 42, 70, 3.5, 0.1, 1 },
    };
    const auto s = makeStepClip(clip);
    CHECK(s.kind == ClipKind::Drums);
}
