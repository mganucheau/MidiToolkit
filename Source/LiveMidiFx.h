#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "EditModel.h"
#include "GrooveEngine.h"
#include <array>
#include <vector>

namespace pflow {

/** Snapshot of Toolkit state consumed on the audio thread for live MIDI. */
struct LiveFxState
{
    ClipEdit edit;
    GrooveParams groove;
    /** Scale root of the selected clip (-1 = unset); used by map-to-root. */
    int sourceRoot = -1;
    /** Reference octave of the selected clip (0..6); used by absolute octave. */
    int sourceOctave = 4;
    double bpmMultiplier = 1.0;
};

/** Transform a live MIDI pitch with the stream-capable ClipEdit pitch fields.
    Channel 10 (drums) skips octave-window / scale remap when `drumChannel` is true. */
int transformLivePitch(int midi, const ClipEdit& edit, int sourceRoot, int sourceOctave,
                       bool drumChannel = false);

/** True when the note's pitch-class is muted by the note filter. */
bool liveNoteMuted(int midi, const ClipEdit& edit);

/** Map an incoming velocity (1..127) through groove intensity/dynamics/range. */
int transformLiveVelocity(int velocity, int noteId, double stepPos, const GrooveParams& groove);

/**
 * Realtime MIDI FX for Passthrough mode.
 * Audio-thread only for process/flush; setState is message-thread safe.
 */
class LiveMidiFx
{
public:
    void setState(const LiveFxState& state);
    LiveFxState getState() const;

    /** Process host MIDI in-place with Toolkit transforms.
        When the host transport is stopped, timing follows an internal clock so
        swing and quantize still land on offbeats. */
    void process(juce::MidiBuffer& midi, double sampleRate, double hostBpm,
                 double hostBeatPos, int numSamples, bool hostIsPlaying = true);

    /** Release every held / pending live note into `out`. */
    void flush(juce::MidiBuffer& out, int samplePosition = 0);

    bool hasHeldNotes() const;

private:
    struct Pending
    {
        int samplesUntil = 0;
        juce::MidiMessage message;
    };

    struct Held
    {
        bool active = false;
        int outPitch = 0;
        int timingOffsetSamples = 0;
        int noteId = 0;
        double onBeat = 0.0;
    };

    struct InputNote
    {
        bool active = false;
        int velocity = 100;
        int order = 0;
    };

    void emitOrQueue(juce::MidiBuffer& out, const juce::MidiMessage& msg, int samplePos,
                     int numSamples);
    void scheduleDelayTaps(const GrooveParams& groove, juce::MidiBuffer& out, bool isNoteOn,
                           int channel, int outPitch, int velocity, int samplePos, int numSamples,
                           double sampleRate, double bpm);
    void syncSustain(const GrooveParams& groove, juce::MidiBuffer& out, int numSamples);
    void emitTransformed(juce::MidiBuffer& out, const LiveFxState& snap, int channel, int inPitch,
                         int velocity, int samplePos, int numSamples, double sampleRate, double bpm,
                         double beatPos, bool isNoteOn, bool drumChannel);
    void runGenerators(juce::MidiBuffer& out, const LiveFxState& snap, double sampleRate,
                       double bpm, double beatPos, int numSamples);
    std::vector<int> heldPitchesSorted(bool asPlayed) const;
    int nextArpPitch(const GrooveParams& g, const std::vector<int>& pitches);
    int nextMelodyPitch(const LiveFxState& snap, const std::vector<int>& held);
    void maybeEmitDrum(juce::MidiBuffer& out, const LiveFxState& snap, double sampleRate,
                       double bpm, double stepPos, int samplePos, int numSamples);

    mutable juce::CriticalSection lock_;
    LiveFxState state_;
    std::array<std::array<Held, 128>, 16> held_ {};
    std::array<InputNote, 128> inputHeld_ {};
    std::vector<int> latchedPitches_;
    int inputOrder_ = 0;
    std::vector<Pending> pending_;
    int nextNoteId_ = 1;
    /** Beat clock used while the host transport is stopped. */
    double freeBeat_ = 0.0;
    std::array<bool, 16> sustainDown_ {};
    std::array<bool, 16> sustainLatched_ {};

    double arpNextStep_ = 0.0;
    int arpIndex_ = 0;
    int arpDirection_ = 1;
    int arpSoundingPitch_ = -1;
    int arpSoundingCh_ = 1;

    double melodyNextStep_ = 0.0;
    int melodyPhase_ = 0;
    int melodySounding_ = -1;

    double drumNextStep_ = 0.0;
    int drumStep_ = 0;
};

} // namespace pflow
