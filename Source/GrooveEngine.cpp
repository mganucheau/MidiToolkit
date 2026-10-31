#include "GrooveEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pflow {

const std::array<KnobDef, kNumKnobs> kKnobDefs {{
    { "swing",     "Swing",      0,  100,   0 },   // 0 = straight (Ableton Timing)
    { "pocket",    "Pocket",  -100,  100,   0 },   // bipolar: - push / + laid-back loose
    { "humanize",  "Humanize",   0,  100,   0 },   // 0 = locked (Ableton Random)
    { "dynamics",  "Dynamics",-100,  100,   0 },   // bipolar: Ableton Velocity
    { "length",    "Length",    25,  200, 100 },   // 100% = original (center)
    { "intensity", "Intensity",  0,  200, 100 },   // 100% = original (center)
}};

int GrooveParams::get(int i) const
{
    switch (i)
    {
        case 0: return swing;
        case 1: return pocket;
        case 2: return humanize;
        case 3: return dynamics;
        case 4: return length;
        case 5: return intensity;
        default: return 0;
    }
}

void GrooveParams::set(int i, int v)
{
    if (i < 0 || i >= kNumKnobs) return;
    const auto& d = kKnobDefs[(size_t) i];
    v = std::clamp(v, d.min, d.max);
    switch (i)
    {
        case 0: swing = v; break;
        case 1: pocket = v; break;
        case 2: humanize = v; break;
        case 3: dynamics = v; break;
        case 4: length = v; break;
        case 5: intensity = v; break;
        default: break;
    }
}

bool GrooveParams::isDefault() const
{
    return swing == 0 && pocket == 0 && humanize == 0 && dynamics == 0
        && length == 100 && intensity == 100
        && swingGridIndex == 1
        && quantizeStrength == 0
        && articulationIndex == (int) Articulation::Off
        && articulationStrength == 0
        && sustainPedalMode == (int) SustainPedalMode::Off
        && complexityTarget < 0
        && delayAmount == 0
        && velocityRangeLo <= 1 && velocityRangeHi >= 127
        && variationIndex == 0;
}

int GrooveParams::activeCount() const
{
    int n = 0;
    for (int i = 0; i < kNumKnobs; ++i)
        if (get(i) != kKnobDefs[(size_t) i].def) ++n;
    if (swingGridIndex != 1) ++n;
    if (quantizeStrength > 0) ++n;
    if (articulationIndex != (int) Articulation::Off && articulationStrength > 0) ++n;
    if (sustainPedalMode != (int) SustainPedalMode::Off) ++n;
    if (complexityTarget >= 0) ++n;
    if (delayAmount > 0) ++n;
    if (velocityRangeLo > 1 || velocityRangeHi < 127) ++n;
    if (variationIndex > 0) ++n;
    return n;
}

void applyGrooveSectionLock(const GrooveParams& locked, GrooveParams& target, uint32_t locks)
{
    if ((locks & toolkitLock::Timing) != 0)
    {
        target.swing = locked.swing;
        target.pocket = locked.pocket;
        target.humanize = locked.humanize;
        target.length = locked.length;
        target.swingBase = locked.swingBase;
        target.swingGridIndex = locked.swingGridIndex;
        target.quantizeGridIndex = locked.quantizeGridIndex;
        target.quantizeStrength = locked.quantizeStrength;
    }
    if ((locks & toolkitLock::Performance) != 0)
    {
        target.dynamics = locked.dynamics;
        target.intensity = locked.intensity;
        target.articulationIndex = locked.articulationIndex;
        target.articulationStrength = locked.articulationStrength;
        target.velocityRangeLo = locked.velocityRangeLo;
        target.velocityRangeHi = locked.velocityRangeHi;
        target.sustainPedalMode = locked.sustainPedalMode;
        // Articulation rewrites Length, which lives in Timing. Keep that length
        // with a locked Performance section unless Timing is locked on its own.
        if ((locks & toolkitLock::Timing) == 0
            && locked.articulationIndex != (int) Articulation::Off)
            target.length = locked.length;
    }
    if ((locks & toolkitLock::Effects) != 0)
    {
        target.complexityTarget = locked.complexityTarget;
        target.variationIndex = locked.variationIndex;
        target.delayTimeIndex = locked.delayTimeIndex;
        target.delayAmount = locked.delayAmount;
        target.delayFeedback = locked.delayFeedback;
        target.arpModeIndex = locked.arpModeIndex;
        target.arpRateIndex = locked.arpRateIndex;
        target.arpGate = locked.arpGate;
        target.arpOctaves = locked.arpOctaves;
        target.strumDirectionIndex = locked.strumDirectionIndex;
        target.strumSpeed = locked.strumSpeed;
        target.strumAmount = locked.strumAmount;
    }
}

void captureGrooveSectionLock(const GrooveParams& src, GrooveParams& locked, uint32_t locks,
                              int clipComplexity)
{
    applyGrooveSectionLock(src, locked, locks);
    if ((locks & toolkitLock::Effects) != 0 && locked.complexityTarget < 0 && clipComplexity >= 0)
        locked.complexityTarget = juce::jlimit(0, 100, clipComplexity);
}

void GrooveParams::applyArticulationToKnobs()
{
    const auto art = articulation();
    const double s = juce::jlimit(0.0, 1.0, articulationStrength / 100.0);

    if (art == Articulation::Off || s <= 0.0)
    {
        length = 100;
        intensity = 100;
        dynamics = 0;
        return;
    }

    length = 100;
    intensity = 100;
    dynamics = 0;

    switch (art)
    {
        case Articulation::Staccato:
            length = juce::jlimit(25, 200, (int) std::lround(100.0 - 55.0 * s));
            intensity = juce::jlimit(0, 200, (int) std::lround(100.0 + 10.0 * s));
            dynamics = juce::jlimit(-100, 100, (int) std::lround(25.0 * s));
            break;
        case Articulation::Tenuto:
            length = juce::jlimit(25, 200, (int) std::lround(100.0 + 18.0 * s));
            break;
        case Articulation::Legato:
        case Articulation::Slur:
            length = juce::jlimit(25, 200, (int) std::lround(100.0 + 28.0 * s));
            break;
        case Articulation::Pianissimo:
            intensity = juce::jlimit(0, 200, (int) std::lround(100.0 - 55.0 * s));
            dynamics = juce::jlimit(-100, 100, (int) std::lround(-45.0 * s));
            break;
        case Articulation::Sforzando:
            intensity = juce::jlimit(0, 200, (int) std::lround(100.0 + 55.0 * s));
            dynamics = juce::jlimit(-100, 100, (int) std::lround(55.0 * s));
            break;
        case Articulation::Off:
        case Articulation::Count:
        default:
            break;
    }
}

juce::String grooveValueText(const KnobDef& def, int value)
{
    if (def.bipolar())
        return (value > 0 ? "+" : "") + juce::String(value);
    return juce::String(value) + "%";
}

namespace {

uint32_t knuthHash(int a)
{
    return (uint32_t) a * 2654435761u;
}

double hashSigned(int id, int salt)
{
    return (double) (knuthHash(id + salt) % 1000u) / 1000.0 * 2.0 - 1.0;
}

int swingPeriodSteps(int gridIndex)
{
    static const int periods[] = { 1, 2, 4, 8, 16, 32 };
    return periods[juce::jlimit(0, 5, gridIndex)];
}

/** Quantize period in 16th-steps (triplets use 2/3 of the straight value). */
double quantizePeriodSteps(QuantizeGrid g)
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

double delayPeriodSteps(DelayTime t)
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

double metricWeight(double start)
{
    const int s = ((int) std::lround(start) % kStepsPerBar + kStepsPerBar) % kStepsPerBar;
    if (s == 0)  return 1.00;
    if (s == 8)  return 0.88;
    if (s == 4 || s == 12) return 0.72;
    if (s % 2 == 0) return 0.55;
    return 0.38;
}

int nextGeneratedId(const std::vector<RollNote>& notes, int salt)
{
    int maxId = 0;
    for (const auto& n : notes)
        maxId = std::max(maxId, n.id);
    return maxId + 1 + salt;
}

std::vector<RollNote> applyComplexityMorph(std::vector<RollNote> notes,
                                           int sourceComplexity,
                                           int targetComplexity)
{
    sourceComplexity = juce::jlimit(1, 100, sourceComplexity);
    targetComplexity = juce::jlimit(0, 100, targetComplexity);
    if (notes.empty() || targetComplexity == sourceComplexity)
        return notes;

    if (targetComplexity < sourceComplexity)
    {
        const double amt = (double) (sourceComplexity - targetComplexity)
                         / (double) juce::jmax(1, sourceComplexity);
        std::vector<std::pair<double, size_t>> ranked;
        ranked.reserve(notes.size());
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const double w = metricWeight(notes[i].start)
                           * (0.35 + 0.65 * (notes[i].fileVelocity / 127.0));
            ranked.push_back({ w, i });
        }
        std::stable_sort(ranked.begin(), ranked.end(),
                         [](const auto& a, const auto& b) { return a.first > b.first; });

        const int keep = juce::jmax(1, (int) std::lround((double) notes.size() * (1.0 - 0.75 * amt)));
        std::vector<char> keepFlags(notes.size(), 0);
        for (int i = 0; i < keep && i < (int) ranked.size(); ++i)
            keepFlags[ranked[(size_t) i].second] = 1;

        if (amt > 0.45)
        {
            for (size_t i = 0; i < notes.size(); ++i)
            {
                if (!keepFlags[i]) continue;
                int chord = 0;
                for (size_t j = 0; j < notes.size(); ++j)
                    if (keepFlags[j] && std::abs(notes[j].start - notes[i].start) < 0.35)
                        ++chord;
                if (chord >= 3)
                {
                    for (size_t j = 0; j < notes.size(); ++j)
                    {
                        if (!keepFlags[j] || std::abs(notes[j].start - notes[i].start) >= 0.35)
                            continue;
                        const bool extreme = (notes[j].pitch <= notes[i].pitch - 7)
                                          || (notes[j].pitch >= notes[i].pitch + 7)
                                          || j == i;
                        if (!extreme && (knuthHash((int) j + 11) % 100u) < (uint32_t) (amt * 80.0))
                            keepFlags[j] = 0;
                    }
                }
            }
        }

        std::vector<RollNote> out;
        out.reserve((size_t) keep);
        for (size_t i = 0; i < notes.size(); ++i)
            if (keepFlags[i])
                out.push_back(notes[i]);
        return out.empty() ? notes : out;
    }

    const double amt = (double) (targetComplexity - sourceComplexity)
                     / (double) juce::jmax(1, 100 - sourceComplexity);
    std::vector<RollNote> out = notes;
    const int baseId = nextGeneratedId(notes, 0);
    int gen = 0;

    for (const auto& n : notes)
    {
        if (metricWeight(n.start) >= 0.72 && (knuthHash(n.id + 3) % 100u) < (uint32_t) (amt * 55.0))
        {
            RollNote g = n;
            g.id = baseId + gen++;
            g.start = std::max(0.0, n.start - 1.0);
            g.len = std::min(0.75, n.len * 0.45);
            g.fileVelocity = juce::jmax(1, (int) std::lround(n.fileVelocity * 0.45));
            g.moved = true;
            out.push_back(g);
        }

        if ((knuthHash(n.id + 17) % 100u) < (uint32_t) (amt * 35.0) && n.pitch <= 100)
        {
            RollNote o = n;
            o.id = baseId + gen++;
            o.pitch = juce::jlimit(0, 127, n.pitch + 12);
            o.len = std::min(n.len, 1.5);
            o.fileVelocity = juce::jmax(1, (int) std::lround(n.fileVelocity * 0.55));
            o.moved = true;
            out.push_back(o);
        }

        const double barPos = std::fmod(n.start, (double) kStepsPerBar);
        if (barPos >= 12.0 && (knuthHash(n.id + 29) % 100u) < (uint32_t) (amt * 40.0))
        {
            RollNote f = n;
            f.id = baseId + gen++;
            f.start = n.start + 0.5;
            f.len = 0.5;
            f.pitch = juce::jlimit(0, 127, n.pitch + ((knuthHash(n.id) % 2u) == 0 ? 2 : -1));
            f.fileVelocity = juce::jmax(1, (int) std::lround(n.fileVelocity * 0.6));
            f.moved = true;
            out.push_back(f);
        }
    }
    return out;
}

std::vector<RollNote> applyDelay(std::vector<RollNote> notes, const GrooveParams& k)
{
    if (k.delayAmount <= 0 || notes.empty())
        return notes;

    const double period = delayPeriodSteps(k.delayTime());
    const double wet = k.delayAmount / 100.0;
    const double fb = juce::jlimit(0.0, 0.95, k.delayFeedback / 100.0);
    const int taps = 1 + (int) std::lround(fb * 5.0);
    const int baseId = nextGeneratedId(notes, 5000);
    int gen = 0;

    std::vector<RollNote> out = notes;
    for (const auto& n : notes)
    {
        double velScale = wet;
        for (int tap = 1; tap <= taps; ++tap)
        {
            RollNote d = n;
            d.id = baseId + gen++;
            d.start = n.start + period * (double) tap;
            d.fileVelocity = juce::jmax(1, (int) std::lround(n.fileVelocity * velScale));
            d.len = std::max(0.25, n.len * (1.0 - 0.08 * tap));
            d.moved = true;
            out.push_back(d);
            velScale *= fb;
            if (velScale < 0.08) break;
        }
    }
    return out;
}

} // namespace

std::vector<RollNote> applyGroove(const std::vector<RollNote>& notes, const GrooveParams& k,
                                  int sourceComplexity)
{
    const int srcCx = sourceComplexity > 0 ? juce::jlimit(0, 100, sourceComplexity) : -1;
    const bool complexityActive = srcCx >= 0 && k.complexityTarget >= 0
                               && juce::jlimit(0, 100, k.complexityTarget) != srcCx;
    const int tgtCx = complexityActive ? juce::jlimit(0, 100, k.complexityTarget) : srcCx;

    if (k.isDefault() && !complexityActive)
        return notes;

    const int period = swingPeriodSteps(k.swingGridIndex);
    const double swingMax = (double) period * 0.5;
    const double pocketAmt = (k.pocket / 100.0) * 1.8;
    const double humanAmt = (k.humanize / 100.0) * 0.9;
    const double lenMul = std::max(0.05, k.length / 100.0);
    const double qPeriod = quantizePeriodSteps(k.quantizeGrid());
    const double qAmt = juce::jlimit(0.0, 1.0, k.quantizeStrength / 100.0);
    const bool articOn = k.articulation() != Articulation::Off;
    const double artAmt = articOn ? juce::jlimit(0.0, 1.0, k.articulationStrength / 100.0) : 0.0;
    const auto art = k.articulation();

    std::vector<RollNote> sorted = notes;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const RollNote& a, const RollNote& b) { return a.start < b.start; });

    std::vector<RollNote> out;
    out.reserve(sorted.size());

    for (size_t i = 0; i < sorted.size(); ++i)
    {
        const auto& n = sorted[i];
        double start = n.start;

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
            const double w = metricWeight(n.start);
            const double looseness = 1.0 - w;
            start += pocketAmt * (0.15 + 0.85 * looseness);
        }

        if (k.humanize > 0)
            start += hashSigned(n.id, 1) * humanAmt;

        double len = std::max(0.5, n.len * lenMul);

        if (artAmt > 0.0 && (art == Articulation::Legato || art == Articulation::Slur))
        {
            // Length/Intensity/Dynamics knobs carry the bulk of articulation;
            // Legato/Slur still need gap-aware sustain that knobs can't express.
            double nextStart = start + 64.0;
            for (size_t j = i + 1; j < sorted.size(); ++j)
                if (sorted[j].start > n.start + 1.0e-6)
                {
                    nextStart = sorted[j].start;
                    break;
                }
            const double gap = std::max(0.0, nextStart - start);
            const double target = std::max(len, gap * (art == Articulation::Slur ? 1.02 : 0.98));
            len += (target - len) * artAmt;
            len = std::max(0.35, len);
        }

        RollNote g = n;
        g.start = std::max(-2.0, start);
        g.len = len;
        out.push_back(g);
    }

    if (complexityActive)
        out = applyComplexityMorph(std::move(out), srcCx, tgtCx);

    out = applyDelay(std::move(out), k);

    // Velocity range: compress the clip's velocity span into [lo, hi].
    if ((k.velocityRangeLo > 1 || k.velocityRangeHi < 127) && !out.empty())
    {
        int lo = juce::jlimit(1, 127, k.velocityRangeLo);
        int hi = juce::jlimit(1, 127, k.velocityRangeHi);
        if (hi < lo) std::swap(lo, hi);
        int srcMin = 127, srcMax = 1;
        for (const auto& n : out)
        {
            srcMin = std::min(srcMin, n.fileVelocity);
            srcMax = std::max(srcMax, n.fileVelocity);
        }
        const int srcSpan = std::max(1, srcMax - srcMin);
        for (auto& n : out)
        {
            const double t = (double) (n.fileVelocity - srcMin) / (double) srcSpan;
            n.fileVelocity = juce::jlimit(1, 127,
                lo + (int) std::lround(t * (double) (hi - lo)));
        }
    }

    // Variations: deterministic alternate takes (rhythm nudges + mild inversion).
    if (k.variationIndex > 0 && !out.empty())
    {
        const int var = juce::jlimit(1, 16, k.variationIndex);
        int pitchMin = 127, pitchMax = 0;
        for (const auto& n : out)
        {
            pitchMin = std::min(pitchMin, n.pitch);
            pitchMax = std::max(pitchMax, n.pitch);
        }
        const int mid = (pitchMin + pitchMax) / 2;
        const int baseId = nextGeneratedId(out, 9000 + var);
        int gen = 0;
        std::vector<RollNote> varied;
        varied.reserve(out.size() + 4);
        for (const auto& n : out)
        {
            RollNote v = n;
            const uint32_t h = knuthHash(n.id * 31 + var * 17);
            // Slight rhythmic nudge on weaker notes.
            if ((h % 100u) < 35u)
                v.start += hashSigned(n.id + var, 4) * (0.25 + 0.15 * (var % 3));
            // Occasional pitch inversion around the phrase center.
            if ((h % 100u) < (uint32_t) (12 + var))
                v.pitch = juce::jlimit(0, 127, mid - (n.pitch - mid));
            // Sparse ghost pickup on stronger variants.
            if (var >= 8 && metricWeight(n.start) >= 0.72 && (h % 100u) < 18u)
            {
                RollNote g = n;
                g.id = baseId + gen++;
                g.start = std::max(0.0, n.start - 0.5);
                g.len = std::min(0.75, n.len * 0.4);
                g.fileVelocity = juce::jmax(1, (int) std::lround(n.fileVelocity * 0.4));
                g.moved = true;
                varied.push_back(g);
            }
            v.moved = true;
            varied.push_back(v);
        }
        out = std::move(varied);
    }

    return out;
}

std::vector<AutomationPoint> buildSustainPedalAutomation(const std::vector<RollNote>& notes,
                                                         double lengthBeats,
                                                         const GrooveParams& k)
{
    std::vector<AutomationPoint> pts;
    const auto mode = k.sustainMode();
    if (mode == SustainPedalMode::Off || lengthBeats <= 0.0)
        return pts;

    if (mode == SustainPedalMode::StartOfClip)
    {
        pts.push_back({ 0.0, 127 });
        pts.push_back({ std::max(0.05, lengthBeats - 0.02), 0 });
        return pts;
    }

    // Auto: phrase-aware pedal — down at phrase starts, lift in gaps, never robotic.
    if (notes.empty())
    {
        pts.push_back({ 0.0, 100 });
        pts.push_back({ std::max(0.05, lengthBeats - 0.02), 0 });
        return pts;
    }

    struct Ev { double t; bool on; };
    std::vector<Ev> evs;
    evs.reserve(notes.size() * 2);
    for (const auto& n : notes)
    {
        evs.push_back({ n.start / 4.0, true });
        evs.push_back({ (n.start + n.len) / 4.0, false });
    }
    std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b)
    {
        if (std::abs(a.t - b.t) < 1.0e-9) return a.on && !b.on;
        return a.t < b.t;
    });

    int active = 0;
    double lastSoundEnd = 0.0;
    bool pedalDown = false;
    constexpr double kGapLift = 0.35; // beats of silence before a lift
    int phraseSalt = 0;

    auto push = [&](double beat, int value)
    {
        // Slight humanize so Auto isn't a grid of identical edges.
        const double jitter = hashSigned(phraseSalt++, 9) * 0.03;
        pts.push_back({ std::max(0.0, beat + jitter), value });
    };

    for (const auto& e : evs)
    {
        if (e.on)
        {
            if (active == 0)
            {
                const double gap = e.t - lastSoundEnd;
                if (!pedalDown || gap >= kGapLift)
                {
                    if (pedalDown && gap >= kGapLift)
                        push(std::max(0.0, lastSoundEnd + 0.04), 0);
                    // Soft down (not always 127) for a natural performer feel.
                    const int depth = 95 + (int) std::lround(hashSigned(phraseSalt, 3) * 20.0);
                    push(e.t, juce::jlimit(70, 127, depth));
                    pedalDown = true;
                }
            }
            ++active;
        }
        else
        {
            active = std::max(0, active - 1);
            if (active == 0)
                lastSoundEnd = e.t;
        }
    }

    if (pedalDown)
        push(std::max(0.05, lengthBeats - 0.02), 0);

    // Deduplicate near-identical consecutive points.
    std::vector<AutomationPoint> cleaned;
    for (const auto& p : pts)
    {
        if (!cleaned.empty()
            && std::abs(cleaned.back().beat - p.beat) < 0.01
            && cleaned.back().value == p.value)
            continue;
        cleaned.push_back(p);
    }
    return cleaned;
}

double baseVel(int id)
{
    return 0.45 + (double) (knuthHash(id + 7) % 56u) / 100.0;
}

double noteVelocity(const RollNote& n, const GrooveParams& k)
{
    return noteVelocityWithBase(baseVel(n.id), n, k);
}

double noteVelocityWithBase(double base, const RollNote& n, const GrooveParams& k)
{
    double v = base * (k.intensity / 100.0);

    if (k.dynamics != 0)
    {
        const double w = metricWeight(n.start);
        const double amt = std::abs(k.dynamics) / 100.0;
        if (k.dynamics > 0)
        {
            const double shaped = 0.35 + w * 0.65;
            v *= (1.0 - amt) + amt * shaped;
        }
        else
        {
            const double shaped = 1.0 - w * 0.55;
            v *= (1.0 - amt) + amt * shaped;
        }
    }

    return std::clamp(v, 0.04, 1.0);
}

} // namespace pflow
