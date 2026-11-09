#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "LiveMidiFx.h"
#include "GeneratorEngines.h"

using namespace pflow;

TEST_CASE("transformLivePitch applies shift and fit-to-scale", "[LiveMidiFx]")
{
    ClipEdit e;
    e.pitchShift = 2;
    REQUIRE(transformLivePitch(60, e, -1, 4) == 62);

    e.fitScale = true;
    e.root = 0; // C
    e.mode = Mode::Ionian;
    // D# (63) snaps into C major
    const int snapped = transformLivePitch(61, e, -1, 4); // C# + 2 = D#
    REQUIRE(pitchInScale(snapped, 0, Mode::Ionian));
}

TEST_CASE("transformLivePitch map-to-root uses source root", "[LiveMidiFx]")
{
    ClipEdit e;
    e.mapToRoot = true;
    e.root = 2; // D
    // Source C → D is +2
    REQUIRE(transformLivePitch(60, e, 0, 4) == 62);
}

TEST_CASE("liveNoteMuted respects mute filter", "[LiveMidiFx]")
{
    ClipEdit e;
    e.noteFilterMask = 0x0FFE; // mute C (pc 0)
    e.noteFilterType = NoteFilterType::Mute;
    REQUIRE(liveNoteMuted(60, e));
    REQUIRE_FALSE(liveNoteMuted(62, e));
}

TEST_CASE("LiveMidiFx transforms note-on pitch", "[LiveMidiFx]")
{
    LiveMidiFx fx;
    LiveFxState state;
    state.edit.pitchShift = 12;
    fx.setState(state);

    juce::MidiBuffer buf;
    buf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    fx.process(buf, 44100.0, 120.0, 0.0, 512);

    bool saw = false;
    for (const auto m : buf)
    {
        auto msg = m.getMessage();
        if (msg.isNoteOn())
        {
            REQUIRE(msg.getNoteNumber() == 72);
            saw = true;
        }
    }
    REQUIRE(saw);
}

TEST_CASE("LiveMidiFx scales velocity with intensity", "[LiveMidiFx]")
{
    LiveMidiFx fx;
    LiveFxState state;
    state.groove.intensity = 200;
    fx.setState(state);

    juce::MidiBuffer buf;
    buf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 40), 0);
    fx.process(buf, 44100.0, 120.0, 0.0, 512);

    bool saw = false;
    for (const auto m : buf)
    {
        auto msg = m.getMessage();
        if (msg.isNoteOn())
        {
            CHECK(msg.getVelocity() > 40);
            saw = true;
        }
    }
    REQUIRE(saw);
}

TEST_CASE("LiveMidiFx swings offbeats while the host is stopped", "[LiveMidiFx]")
{
    LiveMidiFx fx;
    LiveFxState state;
    state.groove.swing = 100;
    state.groove.swingGridIndex = 1; // 1/8
    fx.setState(state);

    juce::MidiBuffer silent;
    fx.process(silent, 44100.0, 120.0, 0.0, 11025, false);

    juce::MidiBuffer buf;
    buf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    fx.process(buf, 44100.0, 120.0, 0.0, 11025, false);

    int onAt = -1;
    for (const auto m : buf)
        if (m.getMessage().isNoteOn())
            onAt = m.samplePosition;
    REQUIRE(onAt > 1000);
}

TEST_CASE("Arp keeps anchors and spins optional skips", "[Generators]")
{
    GrooveParams g;
    g.arpEnabled = true;
    g.arpVariation = 1;
    g.arpNonce = 0;
    std::vector<gen::Event> out;
    const int chord[] = { 60, 64, 67 };
    gen::renderArp(g, 0.0, 4.0, chord, 3, 0, Mode::Ionian, out);
    int ons = 0;
    for (const auto& e : out)
        if (e.velocity > 0) ++ons;
    REQUIRE(ons >= 4);
}

TEST_CASE("Melody downbeats land on chord tones", "[Generators]")
{
    GrooveParams g;
    g.melodyEnabled = true;
    g.melodyStyleIndex = (int) MelodyStyle::Hold;
    g.melodyPhraseIndex = (int) MelodyPhrase::OneBar;
    std::vector<gen::Event> out;
    const int chord[] = { 60, 64, 67 };
    gen::renderMelody(g, 0.0, 4.0, chord, 3, 0, Mode::Ionian, out);
    bool sawDownbeat = false;
    for (const auto& e : out)
    {
        if (e.velocity <= 0) continue;
        const int step = (int) std::lround(e.ppq / 0.25) % 16;
        if (step == 0 || step == 8)
        {
            const int pc = e.note % 12;
            REQUIRE((pc == 0 || pc == 4 || pc == 7));
            sawDownbeat = true;
        }
    }
    REQUIRE(sawDownbeat);
}

TEST_CASE("Chords Input builds a diatonic triad from one finger", "[Generators]")
{
    GrooveParams g;
    g.chordsEnabled = true;
    g.chordsInput = true;
    g.chordRhythmIndex = (int) ChordRhythm::Block;
    std::vector<gen::Event> out;
    const int held[] = { 62 }; // D in C major → ii
    gen::renderChords(g, 0.0, 4.0, held, 1, 0, Mode::Ionian, out);
    int ons = 0;
    for (const auto& e : out)
        if (e.velocity > 0) ++ons;
    REQUIRE(ons >= 3);
}

TEST_CASE("Bass stays in the low register", "[Generators]")
{
    GrooveParams g;
    g.bassEnabled = true;
    g.bassMotionIndex = (int) BassMotion::Pulse;
    std::vector<gen::Event> out;
    gen::renderBass(g, 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, out);
    for (const auto& e : out)
    {
        if (e.velocity <= 0) continue;
        REQUIRE(e.note >= 28);
        REQUIRE(e.note <= 52);
    }
}

TEST_CASE("Drums keep written kicks regardless of density", "[Generators]")
{
    GrooveParams g;
    g.drumsEnabled = true;
    g.drumStyleIndex = (int) DrumStyle::Straight;
    g.drumDensity = 0;
    std::vector<gen::Event> out;
    gen::renderDrums(g, 0.0, 4.0, out);
    bool kick = false;
    for (const auto& e : out)
        if (e.velocity > 0 && e.note == 36)
            kick = true;
    REQUIRE(kick);
}

TEST_CASE("Pitch Input latch is ignored while recording", "[Generators]")
{
    LiveMidiFx fx;
    LiveFxState state;
    state.edit.pitchInput = true;
    state.recording = true;
    state.edit.root = 0;
    fx.setState(state);

    juce::MidiBuffer buf;
    buf.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
    fx.process(buf, 44100.0, 120.0, 0.0, 512);
    REQUIRE(fx.pitchLatchMidi() < 0);

    state.recording = false;
    fx.setState(state);
    juce::MidiBuffer buf2;
    buf2.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
    fx.process(buf2, 44100.0, 120.0, 0.0, 512);
    REQUIRE(fx.pitchLatchMidi() == 64);
}

TEST_CASE("renderPartClip fills the piano-roll StepClip for every part", "[Generators]")
{
    GrooveParams g;
    // Input-on with no held notes must still produce an offline pattern.
    g.arpInput = g.melodyInput = g.chordsInput = g.bassInput = true;
    g.arpVariation = g.melodyVariation = g.chordVariation = g.bassVariation = g.drumVariation = 3;
    g.arpNonce = g.melodyNonce = g.chordNonce = g.bassNonce = g.drumNonce = 2;

    for (int i = 0; i < (int) gen::Part::Count; ++i)
    {
        const auto part = (gen::Part) i;
        auto clip = gen::renderPartClip(part, g, 0, Mode::Ionian);
        REQUIRE(clip.notesResident);
        REQUIRE(clip.bars >= 1);
        REQUIRE_FALSE(clip.notes.empty());
        REQUIRE(clip.noteCount == (int) clip.notes.size());
    }
}
