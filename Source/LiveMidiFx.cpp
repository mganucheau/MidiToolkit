#include "LiveMidiFx.h"
#include "GeneratorEngines.h"
#include <algorithm>
#include <cmath>

namespace pflow {

namespace {

int swingPeriodStepsLocal(int gridIndex)
{
    static const int periods[] = { 1, 2, 4, 8, 16, 32 };
    return periods[juce::jlimit(0, 5, gridIndex)];
}

double quantizePeriodStepsLocal(QuantizeGrid g)
{
    switch (g)
    {
        case QuantizeGrid::Quarter:       return 4.0;
        case QuantizeGrid::QuarterT:      return 8.0 / 3.0;
        case QuantizeGrid::Eighth:        return 2.0;
        case QuantizeGrid::EighthT:       return 4.0 / 3.0;
        case QuantizeGrid::Sixteenth:     return 1.0;
        case QuantizeGrid::SixteenthT:    return 2.0 / 3.0;
        case QuantizeGrid::ThirtySecond:  return 0.5;
        case QuantizeGrid::ThirtySecondT: return 1.0 / 3.0;
        case QuantizeGrid::Count:         break;
    }
    return 2.0;
}

double delayPeriodStepsLocal(DelayTime t)
{
    switch (t)
    {
        case DelayTime::Quarter:       return 4.0;
        case DelayTime::DottedEighth:  return 3.0;
        case DelayTime::Eighth:        return 2.0;
        case DelayTime::EighthT:       return 4.0 / 3.0;
        case DelayTime::Sixteenth:     return 1.0;
        case DelayTime::SixteenthT:    return 2.0 / 3.0;
        case DelayTime::ThirtySecond:  return 0.5;
        case DelayTime::Count:         break;
    }
    return 2.0;
}

double metricWeightLocal(double start)
{
    const int s = ((int) std::lround(start) % kStepsPerBar + kStepsPerBar) % kStepsPerBar;
    if (s == 0)  return 1.00;
    if (s == 8)  return 0.88;
    if (s == 4 || s == 12) return 0.70;
    if ((s % 2) == 0) return 0.55;
    return 0.40;
}

uint32_t knuth(int id)
{
    return (uint32_t) id * 2654435761u;
}

double hashSignedLocal(int id, int salt)
{
    return (double) (knuth(id + salt) % 1000u) / 1000.0 * 2.0 - 1.0;
}

int stepsToSamples(double steps, double sampleRate, double bpm)
{
    if (sampleRate <= 0.0 || bpm <= 0.0) return 0;
    const double beats = steps / 4.0;
    return (int) std::lround(beats * (60.0 / bpm) * sampleRate);
}

int foldPitchIntoRange(int pitch, int lo, int hi)
{
    lo = juce::jlimit(0, 127, lo);
    hi = juce::jlimit(0, 127, hi);
    if (hi < lo) std::swap(lo, hi);
    int p = pitch;
    while (p > hi) p -= 12;
    while (p < lo) p += 12;
    return juce::jlimit(lo, hi, p);
}

int timingOffsetSamplesFor(const GrooveParams& k, double stepPos, int noteId,
                           double sampleRate, double bpm)
{
    double start = stepPos;
    const int period = swingPeriodStepsLocal(k.swingGridIndex);
    const double swingMax = (double) period * 0.5;
    const double pocketAmt = (k.pocket / 100.0) * 1.8;
    const double humanAmt = (k.humanize / 100.0) * 0.9;
    const double qPeriod = quantizePeriodStepsLocal(k.quantizeGrid());
    const double qAmt = juce::jlimit(0.0, 1.0, k.quantizeStrength / 100.0);

    if (qAmt > 0.0 && qPeriod > 1.0e-6)
    {
        const double snapped = std::round(start / qPeriod) * qPeriod;
        start += (snapped - start) * qAmt;
    }

    if (k.swing > 0)
    {
        const int slot = (int) std::floor(start / (double) period);
        if (((slot % 2) + 2) % 2 == 1)
            start += (k.swing / 100.0) * swingMax;
    }

    if (k.pocket != 0)
    {
        const double w = metricWeightLocal(stepPos);
        const double looseness = 1.0 - w;
        start += pocketAmt * (0.15 + 0.85 * looseness);
    }

    if (k.humanize > 0)
        start += hashSignedLocal(noteId, 1) * humanAmt;

    return stepsToSamples(start - stepPos, sampleRate, bpm);
}

} // namespace

int transformLivePitch(int midi, const ClipEdit& edit, int sourceRoot, int sourceOctave,
                       bool drumChannel)
{
    int pitch = juce::jlimit(0, 127, midi);
    if (drumChannel)
        return pitch;

    const int srcOct = juce::jlimit(0, 6, sourceOctave);
    const int tgtOct = edit.octave >= 0 ? juce::jlimit(0, 6, edit.octave) : srcOct;
    if (edit.octaveRange <= 0 && edit.octave >= 0)
        pitch += (tgtOct - srcOct) * 12;

    pitch += edit.pitchShift;

    if (edit.mapToRoot && edit.root >= 0 && sourceRoot >= 0)
    {
        int d = (edit.root - sourceRoot) % 12;
        if (d > 6)  d -= 12;
        if (d < -6) d += 12;
        pitch += d;
    }

    if (edit.fitScale && edit.root >= 0)
        pitch = fitToScale(pitch, edit.root, edit.mode);

    pitch = juce::jlimit(0, 127, pitch);

    if (edit.octaveRange > 0)
        pitch = mapPitchToOctaveWindow(pitch, tgtOct, edit.octaveRange, edit.outsideRange);

    if (edit.hasPitchRange())
        pitch = foldPitchIntoRange(pitch, edit.pitchMin, edit.pitchMax);

    if (edit.fitScale && edit.root >= 0)
        pitch = fitToScale(pitch, edit.root, edit.mode);

    if (edit.hasNoteFilter() && edit.noteFilterType == NoteFilterType::Fold)
        pitch = foldToEnabledPitchClasses(pitch, edit.noteFilterMask, edit.root, edit.mode);

    return juce::jlimit(0, 127, pitch);
}

bool liveNoteMuted(int midi, const ClipEdit& edit)
{
    return edit.hasNoteFilter()
        && edit.noteFilterType == NoteFilterType::Mute
        && !edit.isNoteFilterEnabled(midi);
}

int transformLiveVelocity(int velocity, int noteId, double stepPos, const GrooveParams& groove)
{
    const double base = juce::jlimit(0.04, 1.0, (double) juce::jlimit(1, 127, velocity) / 127.0);
    RollNote tmp;
    tmp.id = noteId;
    tmp.start = stepPos;
    tmp.fileVelocity = juce::jlimit(1, 127, velocity);
    double v = noteVelocityWithBase(base, tmp, groove);

    int midiVel = juce::jlimit(1, 127, (int) std::lround(v * 127.0));

    if (groove.velocityRangeLo > 1 || groove.velocityRangeHi < 127)
    {
        int lo = juce::jlimit(1, 127, groove.velocityRangeLo);
        int hi = juce::jlimit(1, 127, groove.velocityRangeHi);
        if (hi < lo) std::swap(lo, hi);
        const double t = (double) (midiVel - 1) / 126.0;
        midiVel = juce::jlimit(1, 127, lo + (int) std::lround(t * (double) (hi - lo)));
    }
    return midiVel;
}

void LiveMidiFx::setState(const LiveFxState& state)
{
    juce::ScopedLock sl(lock_);
    state_ = state;
}

LiveFxState LiveMidiFx::getState() const
{
    juce::ScopedLock sl(lock_);
    return state_;
}

bool LiveMidiFx::hasHeldNotes() const
{
    for (const auto& ch : held_)
        for (const auto& n : ch)
            if (n.active) return true;
    return !pending_.empty();
}

void LiveMidiFx::emitOrQueue(juce::MidiBuffer& out, const juce::MidiMessage& msg, int samplePos,
                             int numSamples)
{
    if (samplePos < numSamples)
        out.addEvent(msg, juce::jmax(0, samplePos));
    else
        pending_.push_back({ samplePos - numSamples, msg });
}

void LiveMidiFx::scheduleDelayTaps(const GrooveParams& groove, juce::MidiBuffer& out, bool isNoteOn,
                                   int channel, int outPitch, int velocity, int samplePos,
                                   int numSamples, double sampleRate, double bpm)
{
    const auto& k = groove;
    if (k.delayAmount <= 0) return;

    const double period = delayPeriodStepsLocal(k.delayTime());
    const double wet = k.delayAmount / 100.0;
    const double fb = juce::jlimit(0.0, 0.95, k.delayFeedback / 100.0);
    const int taps = 1 + (int) std::lround(fb * 5.0);
    double velScale = wet;

    for (int tap = 1; tap <= taps; ++tap)
    {
        const int delaySamples = stepsToSamples(period * (double) tap, sampleRate, bpm);
        const int pos = samplePos + delaySamples;
        if (isNoteOn)
        {
            const int tapVel = juce::jmax(1, (int) std::lround((double) velocity * velScale));
            emitOrQueue(out, juce::MidiMessage::noteOn(channel, outPitch, (juce::uint8) tapVel),
                        pos, numSamples);
        }
        else
        {
            emitOrQueue(out, juce::MidiMessage::noteOff(channel, outPitch), pos, numSamples);
        }
        velScale *= fb;
        if (velScale < 0.08) break;
    }
}

void LiveMidiFx::syncSustain(const GrooveParams& groove, juce::MidiBuffer& out, int numSamples)
{
    const auto mode = groove.sustainMode();
    for (int ch = 0; ch < 16; ++ch)
    {
        bool held = false;
        for (int n = 0; n < 128; ++n)
        {
            if (held_[(size_t) ch][(size_t) n].active)
            {
                held = true;
                break;
            }
        }

        if (mode == SustainPedalMode::StartOfClip && held)
            sustainLatched_[(size_t) ch] = true;
        if (mode == SustainPedalMode::Off)
            sustainLatched_[(size_t) ch] = false;

        const bool wantDown = (mode == SustainPedalMode::StartOfClip)
                                  ? sustainLatched_[(size_t) ch]
                                  : (mode == SustainPedalMode::Auto ? held : false);
        if (wantDown == sustainDown_[(size_t) ch])
            continue;
        sustainDown_[(size_t) ch] = wantDown;
        emitOrQueue(out, juce::MidiMessage::controllerEvent(ch + 1, 64, wantDown ? 127 : 0),
                    0, numSamples);
    }
}

void LiveMidiFx::flush(juce::MidiBuffer& out, int samplePosition)
{
    for (int ch = 0; ch < 16; ++ch)
    {
        for (int n = 0; n < 128; ++n)
        {
            if (!held_[(size_t) ch][(size_t) n].active) continue;
            out.addEvent(juce::MidiMessage::noteOff(ch + 1, held_[(size_t) ch][(size_t) n].outPitch),
                         samplePosition);
            held_[(size_t) ch][(size_t) n] = {};
        }
        out.addEvent(juce::MidiMessage::controllerEvent(ch + 1, 64, 0), samplePosition);
        out.addEvent(juce::MidiMessage::allNotesOff(ch + 1), samplePosition);
    }
    for (auto& n : inputHeld_) n = {};
    pending_.clear();
    freeBeat_ = 0.0;
    sustainDown_.fill(false);
    sustainLatched_.fill(false);
}

std::vector<int> LiveMidiFx::heldPitchesSorted(bool asPlayed) const
{
    struct Item { int pitch; int order; };
    std::vector<Item> items;
    for (int p = 0; p < 128; ++p)
        if (inputHeld_[(size_t) p].active)
            items.push_back({ p, inputHeld_[(size_t) p].order });
    if (asPlayed)
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.order < b.order; });
    else
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.pitch < b.pitch; });
    std::vector<int> out;
    out.reserve(items.size());
    for (const auto& it : items) out.push_back(it.pitch);
    return out;
}

int LiveMidiFx::effectiveKeyRoot(const LiveFxState& snap) const
{
    if (snap.edit.pitchInput && !snap.recording)
    {
        const int latch = pitchLatchMidi_.load();
        if (latch >= 0)
            return latch % 12;
    }
    if (snap.edit.root >= 0)
        return snap.edit.root;
    if (snap.sourceRoot >= 0)
        return snap.sourceRoot;
    return 0;
}

void LiveMidiFx::emitTransformed(juce::MidiBuffer& out, const LiveFxState& snap, int channel,
                                 int inPitch, int velocity, int samplePos, int numSamples,
                                 double sampleRate, double bpm, double beatPos, bool isNoteOn,
                                 bool drumChannel)
{
    const double beatsIntoBlock = (sampleRate > 0.0)
        ? ((double) samplePos / sampleRate) * (bpm / 60.0) : 0.0;
    const double stepPos = (beatPos + beatsIntoBlock) * 4.0;
    const int noteId = nextNoteId_++;
    const int outPitch = transformLivePitch(inPitch, snap.edit, snap.sourceRoot, snap.sourceOctave,
                                            drumChannel);
    const int vel = transformLiveVelocity(velocity, noteId, stepPos, snap.groove);
    const int timing = timingOffsetSamplesFor(snap.groove, stepPos, noteId, sampleRate, bpm);
    const int emitAt = juce::jmax(0, samplePos + timing);
    const int ch = juce::jlimit(1, 16, channel);

    if (isNoteOn)
    {
        auto& slot = held_[(size_t) (ch - 1)][(size_t) juce::jlimit(0, 127, inPitch)];
        if (slot.active)
            emitOrQueue(out, juce::MidiMessage::noteOff(ch, slot.outPitch), samplePos, numSamples);
        slot.active = true;
        slot.outPitch = outPitch;
        slot.timingOffsetSamples = timing;
        slot.noteId = noteId;
        slot.onBeat = beatPos + beatsIntoBlock;
        emitOrQueue(out, juce::MidiMessage::noteOn(ch, outPitch, (juce::uint8) juce::jlimit(1, 127, vel)),
                    emitAt, numSamples);
        scheduleDelayTaps(snap.groove, out, true, ch, outPitch, vel, emitAt, numSamples, sampleRate, bpm);
    }
    else
    {
        auto& slot = held_[(size_t) (ch - 1)][(size_t) juce::jlimit(0, 127, inPitch)];
        if (!slot.active) return;
        const double lenMul = std::max(0.05, snap.groove.length / 100.0);
        const double offBeat = beatPos + beatsIntoBlock;
        const double durBeats = juce::jmax(0.001, offBeat - slot.onBeat);
        const double remainingBeats = durBeats * lenMul - durBeats;
        const int offAt = samplePos + slot.timingOffsetSamples
                        + stepsToSamples(remainingBeats * 4.0, sampleRate, bpm);
        const int pitch = slot.outPitch;
        emitOrQueue(out, juce::MidiMessage::noteOff(ch, pitch), juce::jmax(0, offAt), numSamples);
        scheduleDelayTaps(snap.groove, out, false, ch, pitch, 0, juce::jmax(0, offAt), numSamples,
                          sampleRate, bpm);
        slot = {};
    }
}

void LiveMidiFx::runGenerators(juce::MidiBuffer& out, const LiveFxState& snap, double sampleRate,
                               double bpm, double beatPos, int numSamples)
{
    if (sampleRate <= 0.0 || bpm <= 0.0) return;

    const double startPpq = beatPos;
    const double endPpq = beatPos + (double) numSamples * (bpm / 60.0) / sampleRate;
    const auto held = heldPitchesSorted(false);
    const int* chordPtr = held.empty() ? nullptr : held.data();
    const int chordCount = (int) held.size();
    const int keyRoot = effectiveKeyRoot(snap);
    const Mode mode = snap.edit.mode;

    arpNonce_ = snap.groove.arpNonce;
    melodyNonce_ = snap.groove.melodyNonce;
    chordNonce_ = snap.groove.chordNonce;
    bassNonce_ = snap.groove.bassNonce;
    drumNonce_ = snap.groove.drumNonce;

    auto emitEvents = [&](const std::vector<gen::Event>& events, int channel, bool drumChannel)
    {
        for (const auto& ev : events)
        {
            const double beatsIntoBlock = ev.ppq - startPpq;
            const int samplePos = juce::jlimit(0, juce::jmax(0, numSamples - 1),
                (int) std::lround(beatsIntoBlock * sampleRate * 60.0 / bpm));
            if (ev.velocity > 0)
            {
                emitTransformed(out, snap, channel, ev.note, ev.velocity, samplePos, numSamples,
                                sampleRate, bpm, beatPos, true, drumChannel);
            }
            else
            {
                const int outPitch = transformLivePitch(ev.note, snap.edit, snap.sourceRoot,
                                                        snap.sourceOctave, drumChannel);
                emitOrQueue(out, juce::MidiMessage::noteOff(channel, outPitch),
                            samplePos, numSamples);
            }
        }
    };

    std::vector<gen::Event> events;
    if (snap.groove.arpEnabled)
    {
        const int* arpChord = snap.groove.arpInput ? chordPtr : nullptr;
        const int arpCount = snap.groove.arpInput ? chordCount : 0;
        gen::renderArp(snap.groove, startPpq, endPpq, arpChord, arpCount, keyRoot, mode, events);
        emitEvents(events, 1, false);
    }
    if (snap.groove.melodyEnabled)
    {
        const int* melChord = snap.groove.melodyInput ? chordPtr : nullptr;
        const int melCount = snap.groove.melodyInput ? chordCount : 0;
        gen::renderMelody(snap.groove, startPpq, endPpq, melChord, melCount, keyRoot, mode, events);
        emitEvents(events, 1, false);
    }
    if (snap.groove.chordsEnabled)
    {
        const int* chChord = snap.groove.chordsInput ? chordPtr : nullptr;
        const int chCount = snap.groove.chordsInput ? chordCount : 0;
        gen::renderChords(snap.groove, startPpq, endPpq, chChord, chCount, keyRoot, mode, events);
        emitEvents(events, 1, false);
    }
    if (snap.groove.bassEnabled)
    {
        const int* bChord = snap.groove.bassInput ? chordPtr : nullptr;
        const int bCount = snap.groove.bassInput ? chordCount : 0;
        gen::renderBass(snap.groove, startPpq, endPpq, bChord, bCount, keyRoot, mode, events);
        emitEvents(events, 1, false);
    }
    if (snap.groove.drumsEnabled)
    {
        gen::renderDrums(snap.groove, startPpq, endPpq, events);
        emitEvents(events, 10, true);
    }
}

void LiveMidiFx::process(juce::MidiBuffer& midi, double sampleRate, double hostBpm,
                         double hostBeatPos, int numSamples, bool hostIsPlaying)
{
    LiveFxState snap;
    {
        juce::ScopedLock sl(lock_);
        snap = state_;
    }

    const double bpm = juce::jmax(1.0, hostBpm);
    double beatPos = hostBeatPos;
    if (!hostIsPlaying)
    {
        beatPos = freeBeat_;
        if (sampleRate > 0.0)
            freeBeat_ += (double) numSamples * (bpm / 60.0) / sampleRate;
        if (freeBeat_ > 1.0e6)
            freeBeat_ = std::fmod(freeBeat_, 16.0);
    }
    else
        freeBeat_ = 0.0;

    juce::MidiBuffer out;
    {
        std::vector<Pending> keep;
        keep.reserve(pending_.size());
        for (auto& p : pending_)
        {
            if (p.samplesUntil < numSamples)
                out.addEvent(p.message, juce::jmax(0, p.samplesUntil));
            else
            {
                p.samplesUntil -= numSamples;
                keep.push_back(p);
            }
        }
        pending_.swap(keep);
    }

    const bool arpConsumes = snap.groove.arpEnabled && snap.groove.arpInput;

    for (const auto metadata : midi)
    {
        auto msg = metadata.getMessage();
        const int samplePos = metadata.samplePosition;

        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            const int ch = juce::jlimit(1, 16, msg.getChannel());
            const int inPitch = juce::jlimit(0, 127, msg.getNoteNumber());
            if (ch != 10)
            {
                inputHeld_[(size_t) inPitch].active = true;
                inputHeld_[(size_t) inPitch].velocity = msg.getVelocity();
                inputHeld_[(size_t) inPitch].order = inputOrder_++;
                if (snap.edit.pitchInput && !snap.recording)
                    pitchLatchMidi_.store(inPitch);
            }

            // Arp with Input on consumes melodic input; drums / CC still pass.
            if (arpConsumes && ch != 10)
                continue;

            if (liveNoteMuted(inPitch, snap.edit) && ch != 10)
                continue;

            emitTransformed(out, snap, ch, inPitch, msg.getVelocity(), samplePos, numSamples,
                            sampleRate, bpm, beatPos, true, ch == 10);
            continue;
        }

        if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
            const int ch = juce::jlimit(1, 16, msg.getChannel());
            const int inPitch = juce::jlimit(0, 127, msg.getNoteNumber());
            if (ch != 10)
                inputHeld_[(size_t) inPitch] = {};

            if (arpConsumes && ch != 10)
                continue;

            emitTransformed(out, snap, ch, inPitch, 0, samplePos, numSamples, sampleRate, bpm,
                            beatPos, false, ch == 10);
            continue;
        }

        out.addEvent(msg, samplePos);
    }

    runGenerators(out, snap, sampleRate, bpm, beatPos, numSamples);
    syncSustain(snap.groove, out, numSamples);
    midi.swapWith(out);
}

} // namespace pflow

