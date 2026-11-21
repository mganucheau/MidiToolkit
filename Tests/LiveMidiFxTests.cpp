#include <catch2/catch_test_macros.hpp>
#include "LiveMidiFx.h"

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

TEST_CASE("Pitch Input transposes to the incoming note without Map to Root", "[LiveMidiFx]")
{
    ClipEdit e;
    e.pitchInput = true;
    e.mapToRoot = false;
    // Clip is in C; incoming D (MIDI 62) moves it up 2 semitones.
    CHECK(pitchInputDelta(62, e, 0) == 2);
    CHECK(transformLivePitch(60, e, 0, 4, false, 62) == 62);
    // Nearest direction: incoming B transposes down 1, not up 11.
    CHECK(pitchInputDelta(71, e, 0) == -1);
    // No latched note or Pitch Input off → untouched.
    CHECK(pitchInputDelta(-1, e, 0) == 0);
    e.pitchInput = false;
    CHECK(transformLivePitch(60, e, 0, 4, false, 62) == 60);
    // Drums are never transposed.
    e.pitchInput = true;
    CHECK(transformLivePitch(36, e, 0, 4, true, 62) == 36);
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
