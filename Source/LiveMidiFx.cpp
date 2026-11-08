#include "LiveMidiFx.h"
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
    latchedPitches_.clear();
    if (arpSoundingPitch_ >= 0)
        out.addEvent(juce::MidiMessage::noteOff(arpSoundingCh_, arpSoundingPitch_), samplePosition);
    if (melodySounding_ >= 0)
        out.addEvent(juce::MidiMessage::noteOff(1, melodySounding_), samplePosition);
    arpSoundingPitch_ = -1;
    melodySounding_ = -1;
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
    if (items.empty())
    {
        for (int p : latchedPitches_)
            items.push_back({ p, p });
    }
    if (asPlayed)
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.order < b.order; });
    else
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.pitch < b.pitch; });
    std::vector<int> out;
    out.reserve(items.size());
    for (const auto& it : items) out.push_back(it.pitch);
    return out;
}

int LiveMidiFx::nextArpPitch(const GrooveParams& g, const std::vector<int>& pitches)
{
    if (pitches.empty()) return -1;
    const int octs = juce::jlimit(1, 3, g.arpOctaves);
    const int n = (int) pitches.size() * octs;
    if (n <= 0) return -1;

    auto pitchAt = [&](int idx) {
        idx = ((idx % n) + n) % n;
        const int base = pitches[(size_t) (idx % (int) pitches.size())];
        const int oct = idx / (int) pitches.size();
        return juce::jlimit(0, 127, base + oct * 12);
    };

    switch (g.arpMode())
    {
        case ArpMode::Down:
            arpIndex_ = (arpIndex_ - 1 + n) % n;
            return pitchAt(n - 1 - arpIndex_);
        case ArpMode::UpDown:
        {
            const int span = juce::jmax(1, n * 2 - 2);
            arpIndex_ = (arpIndex_ + 1) % span;
            const int tri = arpIndex_ < n ? arpIndex_ : span - arpIndex_;
            return pitchAt(tri);
        }
        case ArpMode::Random:
            arpIndex_ = (int) (knuth(nextNoteId_ + arpIndex_) % (uint32_t) n);
            return pitchAt(arpIndex_);
        case ArpMode::AsPlayed:
            arpIndex_ = (arpIndex_ + 1) % (int) pitches.size();
            return pitches[(size_t) arpIndex_];
        case ArpMode::Chord:
            return pitches.front(); // caller emits all
        case ArpMode::Up:
        case ArpMode::Off:
        case ArpMode::Count:
            break;
    }
    arpIndex_ = (arpIndex_ + 1) % n;
    return pitchAt(arpIndex_);
}

int LiveMidiFx::nextMelodyPitch(const LiveFxState& snap, const std::vector<int>& held)
{
    const int root = snap.edit.root >= 0 ? snap.edit.root
                   : (snap.sourceRoot >= 0 ? snap.sourceRoot
                      : (held.empty() ? 0 : held.front() % 12));
    const Mode mode = snap.edit.mode;
    const auto& iv = modeIntervals(mode);
    const int range = juce::jlimit(8, 24, snap.groove.melodyRange);
    const int base = held.empty() ? 48 + root : held.front();
    const int lo = juce::jlimit(0, 127, base);
    const int seed = snap.groove.melodyVariation * 97 + melodyPhase_;

    auto scalePitch = [&](int degree, int octShift) {
        const int deg = ((degree % 7) + 7) % 7;
        return juce::jlimit(0, 127, lo - (lo % 12) + root + iv[(size_t) deg] + octShift * 12);
    };

    switch (snap.groove.melodyStyle())
    {
        case MelodyStyle::ScaleRun:
            ++melodyPhase_;
            return scalePitch(melodyPhase_, (melodyPhase_ / 7) % juce::jmax(1, range / 12 + 1));
        case MelodyStyle::CallResponse:
        {
            const bool call = ((melodyPhase_ / 4) % 2) == 0;
            const int deg = (int) (knuth(seed) % 7u);
            ++melodyPhase_;
            return scalePitch(call ? deg : (deg + 3) % 7, call ? 0 : 1);
        }
        case MelodyStyle::Motif:
        case MelodyStyle::Count:
            break;
    }
    static const int kMotif[] = { 0, 2, 4, 2, 0, 4, 5, 4 };
    const int deg = kMotif[(size_t) (melodyPhase_ % 8)];
    ++melodyPhase_;
    const int jitter = (int) (knuth(seed + 3) % 3u) - 1;
    return juce::jlimit(lo, juce::jmin(127, lo + range), scalePitch(deg + jitter, 0));
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

void LiveMidiFx::maybeEmitDrum(juce::MidiBuffer& out, const LiveFxState& snap, double sampleRate,
                               double bpm, double stepPos, int samplePos, int numSamples)
{
    const int dens = snap.groove.drumDensity;
    const int var = snap.groove.drumVariation;
    const int step = drumStep_ % 16;
    auto hit = [&](int pitch, int chance, int vel) {
        if ((int) (knuth(var * 31 + step * 17 + pitch) % 100u) >= chance) return;
        emitTransformed(out, snap, 10, pitch, vel, samplePos, numSamples, sampleRate, bpm,
                        stepPos / 4.0, true, true);
        // Short drum hits — schedule note-off a 16th later.
        const int offAt = samplePos + stepsToSamples(0.85, sampleRate, bpm);
        emitOrQueue(out, juce::MidiMessage::noteOff(10, pitch), juce::jmax(0, offAt), numSamples);
    };

    const bool kick = [&] {
        switch (snap.groove.drumStyle())
        {
            case DrumStyle::FourOnFloor: return (step % 4) == 0;
            case DrumStyle::HalfTime:    return step == 0 || step == 8;
            case DrumStyle::Break:       return step == 0 || step == 6 || step == 10;
            case DrumStyle::Backbeat:
            case DrumStyle::Count:       return step == 0 || step == 8;
        }
        return false;
    }();
    const bool snare = [&] {
        switch (snap.groove.drumStyle())
        {
            case DrumStyle::HalfTime: return step == 8;
            case DrumStyle::Break:    return step == 4 || step == 11 || step == 14;
            case DrumStyle::FourOnFloor:
            case DrumStyle::Backbeat:
            case DrumStyle::Count:    return step == 4 || step == 12;
        }
        return false;
    }();
    const bool hat = (step % 2) == 0;

    if (kick)  hit(36, juce::jlimit(40, 100, dens + 30), 100);
    if (snare) hit(38, juce::jlimit(30, 100, dens + 20), 96);
    if (hat)   hit(42, juce::jlimit(20, 100, dens), 70);
    ++drumStep_;
}

void LiveMidiFx::runGenerators(juce::MidiBuffer& out, const LiveFxState& snap, double sampleRate,
                               double bpm, double beatPos, int numSamples)
{
    const double blockStartStep = beatPos * 4.0;
    const double blockEndStep = blockStartStep
        + ((sampleRate > 0.0) ? ((double) numSamples / sampleRate) * (bpm / 60.0) * 4.0 : 0.0);
    const auto held = heldPitchesSorted(snap.groove.arpMode() == ArpMode::AsPlayed);
    const bool arpOn = snap.groove.arpMode() != ArpMode::Off && !held.empty();
    const bool melOn = snap.groove.melodyEnabled;
    const bool drumOn = snap.groove.drumsEnabled;

    if (arpOn)
    {
        const double period = delayPeriodStepsLocal(snap.groove.arpRate());
        if (arpNextStep_ < blockStartStep - period)
            arpNextStep_ = std::floor(blockStartStep / period) * period;
        while (arpNextStep_ < blockEndStep)
        {
            const double localSteps = arpNextStep_ - blockStartStep;
            const int samplePos = stepsToSamples(localSteps, sampleRate, bpm);
            const double gate = juce::jlimit(0.1, 1.0, snap.groove.arpGate / 100.0);
            const int offAt = samplePos + stepsToSamples(period * gate, sampleRate, bpm);

            if (arpSoundingPitch_ >= 0)
            {
                emitOrQueue(out, juce::MidiMessage::noteOff(arpSoundingCh_, arpSoundingPitch_),
                            juce::jmax(0, samplePos), numSamples);
                arpSoundingPitch_ = -1;
            }

            if (snap.groove.arpMode() == ArpMode::Chord)
            {
                for (int p : held)
                {
                    for (int o = 0; o < juce::jlimit(1, 3, snap.groove.arpOctaves); ++o)
                    {
                        const int pitch = juce::jlimit(0, 127, p + o * 12);
                        emitTransformed(out, snap, 1, pitch, 100, juce::jmax(0, samplePos),
                                        numSamples, sampleRate, bpm, beatPos, true, false);
                        emitOrQueue(out, juce::MidiMessage::noteOff(1,
                                        transformLivePitch(pitch, snap.edit, snap.sourceRoot,
                                                           snap.sourceOctave, false)),
                                    juce::jmax(0, offAt), numSamples);
                    }
                }
            }
            else
            {
                const int pitch = nextArpPitch(snap.groove, held);
                if (pitch >= 0)
                {
                    emitTransformed(out, snap, 1, pitch, 100, juce::jmax(0, samplePos),
                                    numSamples, sampleRate, bpm, beatPos, true, false);
                    arpSoundingPitch_ = transformLivePitch(pitch, snap.edit, snap.sourceRoot,
                                                           snap.sourceOctave, false);
                    arpSoundingCh_ = 1;
                    emitOrQueue(out, juce::MidiMessage::noteOff(1, arpSoundingPitch_),
                                juce::jmax(0, offAt), numSamples);
                }
            }
            arpNextStep_ += period;
        }
    }
    else if (arpSoundingPitch_ >= 0)
    {
        emitOrQueue(out, juce::MidiMessage::noteOff(arpSoundingCh_, arpSoundingPitch_), 0, numSamples);
        arpSoundingPitch_ = -1;
    }

    if (melOn)
    {
        const double period = delayPeriodStepsLocal(snap.groove.melodyRate());
        if (melodyNextStep_ < blockStartStep - period)
            melodyNextStep_ = std::floor(blockStartStep / period) * period;
        while (melodyNextStep_ < blockEndStep)
        {
            const int chance = juce::jlimit(5, 100, snap.groove.melodyDensity);
            const double localSteps = melodyNextStep_ - blockStartStep;
            const int samplePos = stepsToSamples(localSteps, sampleRate, bpm);
            if ((int) (knuth(snap.groove.melodyVariation * 13 + (int) melodyNextStep_) % 100u) < chance)
            {
                if (melodySounding_ >= 0)
                {
                    emitOrQueue(out, juce::MidiMessage::noteOff(1, melodySounding_),
                                juce::jmax(0, samplePos), numSamples);
                    melodySounding_ = -1;
                }
                const int pitch = nextMelodyPitch(snap, held);
                emitTransformed(out, snap, 1, pitch, 90, juce::jmax(0, samplePos), numSamples,
                                sampleRate, bpm, beatPos, true, false);
                melodySounding_ = transformLivePitch(pitch, snap.edit, snap.sourceRoot,
                                                     snap.sourceOctave, false);
                const int offAt = samplePos + stepsToSamples(period * 0.85, sampleRate, bpm);
                emitOrQueue(out, juce::MidiMessage::noteOff(1, melodySounding_),
                            juce::jmax(0, offAt), numSamples);
            }
            melodyNextStep_ += period;
        }
    }

    if (drumOn)
    {
        const double period = 1.0; // 16th grid
        if (drumNextStep_ < blockStartStep - period)
            drumNextStep_ = std::floor(blockStartStep / period) * period;
        while (drumNextStep_ < blockEndStep)
        {
            const double localSteps = drumNextStep_ - blockStartStep;
            const int samplePos = stepsToSamples(localSteps, sampleRate, bpm);
            maybeEmitDrum(out, snap, sampleRate, bpm, drumNextStep_, juce::jmax(0, samplePos),
                          numSamples);
            drumNextStep_ += period;
        }
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

    const bool arpOn = snap.groove.arpMode() != ArpMode::Off;

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
                if (snap.groove.arpLatch)
                {
                    latchedPitches_.clear();
                    for (int p = 0; p < 128; ++p)
                        if (inputHeld_[(size_t) p].active)
                            latchedPitches_.push_back(p);
                }
            }

            // Arp consumes melodic input; drums / CC still pass.
            if (arpOn && ch != 10)
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

            if (arpOn && ch != 10)
            {
                if (!snap.groove.arpLatch)
                {
                    bool any = false;
                    for (const auto& n : inputHeld_)
                        if (n.active) { any = true; break; }
                    if (!any) latchedPitches_.clear();
                }
                continue;
            }

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

