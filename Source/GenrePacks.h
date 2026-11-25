#pragma once
#include "GrooveEngine.h"
#include <array>
#include <juce_core/juce_core.h>

namespace pflow {

/** One 16th step. velocity 0 is empty. ticks is a 480-PPQ offset from the step. */
struct PackStep
{
    int velocity = 0;
    int ticks = 0;
};

/**
 * Genre writing rules. The JSON files under Source/GenrePacks are the only
 * copy of these numbers; the plugin embeds those files.
 */
struct GenrePack
{
    int bpmMin = 120;
    int bpmMax = 128;
    /** Fraction of a 16th added to off-beat 8ths (steps 2, 6, 10, 14). */
    double swing = 0.0;
    float densityMul = 1.0f;
    float syncopation = 0.35f;
    float sustainBias = 0.0f;
    bool preferOffbeats = false;
    double bassGate = 0.4;    // quarter notes
    double chordGate = 0.5;
    std::array<PackStep, 16> kick {};
    std::array<PackStep, 16> snare {};
    std::array<PackStep, 16> hat {};
    std::array<PackStep, 16> ghost {};
    std::array<PackStep, 16> bass {};
    std::array<PackStep, 16> chords {};
    std::array<PackStep, 16> arp {};

    bool anyBass() const;
    bool anyChords() const;
    bool anyArp() const;
};

const GenrePack& genrePack(DrumStyle style);

/** Parse one pack document. Used by tests and by the embedded loader. */
GenrePack genrePackFromJson(const juce::String& json);

} // namespace pflow
