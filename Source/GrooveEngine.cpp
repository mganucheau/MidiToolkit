#include "GrooveEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>

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
        && variationIndex == 0
        && revoice == 0
        && !arpEnabled
        && !melodyEnabled
        && !chordsEnabled
        && !bassEnabled
        && !drumsEnabled;
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
    if (revoice > 0) ++n;
    if (arpEnabled) ++n;
    if (melodyEnabled) ++n;
    if (chordsEnabled) ++n;
    if (bassEnabled) ++n;
    if (drumsEnabled) ++n;
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
        target.revoice = locked.revoice;
        target.delayTimeIndex = locked.delayTimeIndex;
        target.delayAmount = locked.delayAmount;
        target.delayFeedback = locked.delayFeedback;
        target.arpEnabled = locked.arpEnabled;
        target.arpInput = locked.arpInput;
        target.arpDirectionIndex = locked.arpDirectionIndex;
        target.arpShapeIndex = locked.arpShapeIndex;
        target.arpRateIndex = locked.arpRateIndex;
        target.arpGate = locked.arpGate;
        target.arpOctaves = locked.arpOctaves;
        target.arpDensity = locked.arpDensity;
        target.arpIntensity = locked.arpIntensity;
        target.arpDynamics = locked.arpDynamics;
        target.arpLengthIndex = locked.arpLengthIndex;
        target.arpFormIndex = locked.arpFormIndex;
        target.arpStyleIndex = locked.arpStyleIndex;
        target.arpGenreIndex = locked.arpGenreIndex;
        target.arpMotionIndex = locked.arpMotionIndex;
        target.arpVariation = locked.arpVariation;
        target.arpNonce = locked.arpNonce;
        target.melodyEnabled = locked.melodyEnabled;
        target.melodyInput = locked.melodyInput;
        target.melodyStyleIndex = locked.melodyStyleIndex;
        target.melodyGenreIndex = locked.melodyGenreIndex;
        target.melodyContourIndex = locked.melodyContourIndex;
        target.melodySpeedIndex = locked.melodySpeedIndex;
        target.melodyDensity = locked.melodyDensity;
        target.melodyPhraseIndex = locked.melodyPhraseIndex;
        target.melodyLengthIndex = locked.melodyLengthIndex;
        target.melodyFormIndex = locked.melodyFormIndex;
        target.melodyVariation = locked.melodyVariation;
        target.melodyNonce = locked.melodyNonce;
        target.chordsEnabled = locked.chordsEnabled;
        target.chordsInput = locked.chordsInput;
        target.chordVoicingIndex = locked.chordVoicingIndex;
        target.chordColorIndex = locked.chordColorIndex;
        target.chordGenreIndex = locked.chordGenreIndex;
        target.chordDensity = locked.chordDensity;
        target.chordRhythmIndex = locked.chordRhythmIndex;
        target.chordLengthIndex = locked.chordLengthIndex;
        target.chordFormIndex = locked.chordFormIndex;
        target.chordVariation = locked.chordVariation;
        target.chordNonce = locked.chordNonce;
        target.bassEnabled = locked.bassEnabled;
        target.bassInput = locked.bassInput;
        target.bassMotionIndex = locked.bassMotionIndex;
        target.bassStyleIndex = locked.bassStyleIndex;
        target.bassGenreIndex = locked.bassGenreIndex;
        target.bassDensity = locked.bassDensity;
        target.bassLengthIndex = locked.bassLengthIndex;
        target.bassFormIndex = locked.bassFormIndex;
        target.bassVariation = locked.bassVariation;
        target.bassNonce = locked.bassNonce;
        target.drumsEnabled = locked.drumsEnabled;
        target.drumStyleIndex = locked.drumStyleIndex;
        target.drumDensity = locked.drumDensity;
        target.drumFill = locked.drumFill;
        target.drumLengthIndex = locked.drumLengthIndex;
        target.drumFormIndex = locked.drumFormIndex;
        target.drumVariation = locked.drumVariation;
        target.drumNonce = locked.drumNonce;
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

enum class DrumVoice { Kick, Snare, Hat, Perc, Other };

DrumVoice drumVoiceOf(int pitch)
{
    switch (pitch)
    {
        case 35: case 36: return DrumVoice::Kick;
        case 37: case 38: case 40: return DrumVoice::Snare;
        case 42: case 44: case 46: return DrumVoice::Hat;
        default:
            return isGmDrumPitch(pitch) ? DrumVoice::Perc : DrumVoice::Other;
    }
}

int defaultPitchForVoice(DrumVoice v)
{
    switch (v)
    {
        case DrumVoice::Kick:  return 36;
        case DrumVoice::Snare: return 38;
        case DrumVoice::Hat:   return 42;
        case DrumVoice::Perc:  return 39;
        case DrumVoice::Other: break;
    }
    return 42;
}

bool isDrumAnchor(const RollNote& n)
{
    const auto v = drumVoiceOf(n.pitch);
    if (v != DrumVoice::Kick && v != DrumVoice::Snare)
        return false;
    int slot = ((int) std::lround(n.start)) % kStepsPerBar;
    if (slot < 0) slot += kStepsPerBar;
    // Kick on 1/3, snare on 2/4 (backbeat skeleton).
    if (v == DrumVoice::Kick)
        return slot == 0 || slot == 8;
    return slot == 4 || slot == 12;
}

std::vector<double> anchorOnsets(const std::vector<RollNote>& notes, ClipKind kind)
{
    std::vector<double> out;
    for (const auto& n : notes)
    {
        if (kind == ClipKind::Drums)
        {
            if (isDrumAnchor(n))
                out.push_back(n.start);
        }
        else if (kind == ClipKind::Keys)
        {
            // Chord attack times (cluster starts).
            bool isFirst = true;
            for (const auto& o : notes)
                if (std::abs(o.start - n.start) < 0.35 && o.pitch < n.pitch)
                {
                    isFirst = false;
                    break;
                }
            if (isFirst)
                out.push_back(n.start);
        }
        else if (metricWeight(n.start) >= 0.72)
        {
            out.push_back(n.start);
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end(),
                          [](double a, double b) { return std::abs(a - b) < 0.2; }),
              out.end());
    return out;
}

bool anchorsPreserved(const std::vector<double>& before, const std::vector<RollNote>& after, ClipKind kind)
{
    if (before.empty())
        return true;
    const auto afterAnchors = anchorOnsets(after, kind);
    // Piano syncopation may push attacks by a 16th; drums keep tighter anchors.
    const double tol = kind == ClipKind::Keys ? 1.5 : 0.35;
    int matched = 0;
    for (double t : before)
    {
        for (double u : afterAnchors)
            if (std::abs(u - t) < tol)
            {
                ++matched;
                break;
            }
    }
    return matched * 2 >= (int) before.size(); // at least half the anchors remain
}

bool notesNearlyIdentical(const std::vector<RollNote>& a, const std::vector<RollNote>& b)
{
    if (a.size() != b.size())
        return false;
    auto sa = a, sb = b;
    auto byKey = [](const RollNote& x, const RollNote& y)
    {
        if (x.start < y.start) return true;
        if (y.start < x.start) return false;
        return x.pitch < y.pitch;
    };
    std::sort(sa.begin(), sa.end(), byKey);
    std::sort(sb.begin(), sb.end(), byKey);
    for (size_t i = 0; i < sa.size(); ++i)
        if (std::abs(sa[i].start - sb[i].start) > 0.15
            || sa[i].pitch != sb[i].pitch)
            return false;
    return true;
}

bool noNewOutOfScalePitches(const std::vector<RollNote>& source,
                            const std::vector<RollNote>& cand,
                            int rootPc, Mode mode)
{
    if (rootPc < 0)
        return true;
    std::set<int> sourcePitches;
    for (const auto& n : source)
        sourcePitches.insert(n.pitch);
    for (const auto& n : cand)
    {
        if (pitchInScale(n.pitch, rootPc, mode))
            continue;
        if (sourcePitches.count(n.pitch) == 0)
            return false;
    }
    return true;
}

bool noNewNonDrumPitches(const std::vector<RollNote>& source,
                         const std::vector<RollNote>& cand)
{
    std::set<int> sourcePitches;
    for (const auto& n : source)
        sourcePitches.insert(n.pitch);
    for (const auto& n : cand)
    {
        if (drumVoiceOf(n.pitch) != DrumVoice::Other)
            continue;
        if (sourcePitches.count(n.pitch) == 0)
            return false;
    }
    return true;
}

double drumLayerScore(const std::vector<RollNote>& notes)
{
    std::set<int> voices;
    int off = 0, ghost = 0;
    for (const auto& n : notes)
    {
        voices.insert((int) drumVoiceOf(n.pitch));
        int slot = ((int) std::lround(n.start)) % kStepsPerBar;
        if (slot < 0) slot += kStepsPerBar;
        if ((slot % 2) != 0) ++off;
        if (n.fileVelocity < 50) ++ghost;
    }
    const double n = (double) juce::jmax(1, (int) notes.size());
    return (double) voices.size() + (double) off / n + (double) ghost / n;
}

double pianoComplexityScore(const std::vector<RollNote>& notes)
{
    std::vector<RollNote> sorted = notes;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const RollNote& a, const RollNote& b) { return a.start < b.start; });
    double chordSum = 0.0;
    int groups = 0, extensions = 0, syncop = 0;
    for (size_t i = 0; i < sorted.size();)
    {
        const double t = sorted[i].start;
        std::set<int> pcs;
        while (i < sorted.size() && std::abs(sorted[i].start - t) < 0.35)
        {
            pcs.insert(((sorted[i].pitch % 12) + 12) % 12);
            ++i;
        }
        chordSum += (double) pcs.size();
        ++groups;
        if (pcs.size() >= 4) ++extensions;
        int slot = ((int) std::lround(t)) % kStepsPerBar;
        if (slot < 0) slot += kStepsPerBar;
        if ((slot % 2) != 0) ++syncop;
    }
    const double g = (double) juce::jmax(1, groups);
    return chordSum / g + (double) extensions / g + (double) syncop / g;
}

double lineComplexityScore(const std::vector<RollNote>& notes)
{
    int pickups = 0;
    for (const auto& n : notes)
    {
        int slot = ((int) std::lround(n.start)) % kStepsPerBar;
        if (slot < 0) slot += kStepsPerBar;
        if ((slot % 2) != 0) ++pickups;
    }
    return (double) notes.size() * 0.05 + (double) pickups / (double) juce::jmax(1, (int) notes.size());
}

double directionScore(const std::vector<RollNote>& notes, ClipKind kind, bool wantMore)
{
    double s = 0.0;
    if (kind == ClipKind::Drums)
        s = drumLayerScore(notes);
    else if (kind == ClipKind::Keys)
        s = pianoComplexityScore(notes);
    else
        s = lineComplexityScore(notes);
    return wantMore ? s : -s;
}

std::vector<RollNote> morphDrumsOnce(const std::vector<RollNote>& notes, double amt, bool simplify, int seed)
{
    std::vector<RollNote> out = notes;
    if (simplify)
    {
        // Peel layers: Perc → Hat → ghost Snare. Keep kick/snare anchors.
        std::vector<char> keep(out.size(), 1);
        for (size_t i = 0; i < out.size(); ++i)
        {
            if (isDrumAnchor(out[i]))
                continue;
            const auto v = drumVoiceOf(out[i].pitch);
            const uint32_t h = knuthHash(out[i].id * 13 + seed);
            const bool ghost = out[i].fileVelocity < 55;
            double dropChance = 0.0;
            if (v == DrumVoice::Perc) dropChance = 0.55 + 0.40 * amt;
            else if (v == DrumVoice::Hat) dropChance = 0.25 + 0.55 * amt;
            else if (v == DrumVoice::Snare && ghost) dropChance = 0.15 + 0.50 * amt;
            if ((h % 100u) < (uint32_t) (dropChance * 100.0))
                keep[i] = 0;
        }
        std::vector<RollNote> filtered;
        for (size_t i = 0; i < out.size(); ++i)
            if (keep[i]) filtered.push_back(out[i]);
        return filtered.empty() ? notes : filtered;
    }

    // Add layers implied by the kit.
    int hatPitch = 42, snarePitch = 38, percPitch = 39;
    bool hasHat = false;
    for (const auto& n : notes)
    {
        if (drumVoiceOf(n.pitch) == DrumVoice::Hat) { hatPitch = n.pitch; hasHat = true; }
        if (drumVoiceOf(n.pitch) == DrumVoice::Snare) snarePitch = n.pitch;
        if (drumVoiceOf(n.pitch) == DrumVoice::Perc) percPitch = n.pitch;
    }
    const int baseId = nextGeneratedId(notes, seed);
    int gen = 0;
    double maxStart = 0.0;
    for (const auto& n : notes)
        maxStart = std::max(maxStart, n.start);

    auto addHit = [&](double start, int pitch, int vel)
    {
        RollNote g;
        g.id = baseId + gen++;
        g.pitch = pitch;
        g.start = std::max(0.0, start);
        g.len = 0.5;
        g.fileVelocity = juce::jlimit(1, 127, vel);
        g.channel = 10;
        g.moved = true;
        out.push_back(g);
    };

    // 8th then 16th hats on empty slots.
    const int bars = juce::jmax(1, (int) std::ceil((maxStart + 1.0) / (double) kStepsPerBar));
    for (int b = 0; b < bars; ++b)
    {
        for (int slot = 0; slot < kStepsPerBar; ++slot)
        {
            const double t = (double) (b * kStepsPerBar + slot);
            bool occupied = false;
            for (const auto& n : notes)
                if (drumVoiceOf(n.pitch) == DrumVoice::Hat && std::abs(n.start - t) < 0.3)
                {
                    occupied = true;
                    break;
                }
            if (occupied) continue;
            const uint32_t h = knuthHash(seed * 17 + b * 31 + slot);
            const bool eighth = (slot % 2) == 0;
            const double chance = eighth ? (0.20 + 0.55 * amt) : (0.05 + 0.45 * amt);
            if ((h % 100u) < (uint32_t) (chance * 100.0))
                addHit(t, hasHat ? hatPitch : defaultPitchForVoice(DrumVoice::Hat),
                       eighth ? 70 : 45);
        }
    }

    // Ghost snares ahead of backbeats.
    for (const auto& n : notes)
    {
        if (drumVoiceOf(n.pitch) != DrumVoice::Snare || n.fileVelocity < 70)
            continue;
        const uint32_t h = knuthHash(n.id + seed * 7);
        if ((h % 100u) < (uint32_t) (amt * 50.0))
            addHit(std::max(0.0, n.start - 1.0), snarePitch,
                   juce::jmax(1, (int) std::lround(n.fileVelocity * 0.35)));
    }

    // Light perc on offbeats when amt is high.
    if (amt > 0.45)
    {
        for (int b = 0; b < bars; ++b)
        {
            for (int slot : { 2, 6, 10, 14 })
            {
                const double t = (double) (b * kStepsPerBar + slot);
                const uint32_t h = knuthHash(seed * 41 + b * 9 + slot);
                if ((h % 100u) < (uint32_t) ((amt - 0.3) * 80.0))
                    addHit(t, percPitch, 40);
            }
        }
    }
    return out;
}

struct ChordGroup
{
    double start = 0.0;
    std::vector<size_t> indices;
};

std::vector<ChordGroup> groupChords(const std::vector<RollNote>& notes)
{
    std::vector<size_t> order(notes.size());
    for (size_t i = 0; i < notes.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b)
    {
        if (notes[a].start < notes[b].start) return true;
        if (notes[b].start < notes[a].start) return false;
        return notes[a].pitch < notes[b].pitch;
    });

    std::vector<ChordGroup> groups;
    for (size_t idx : order)
    {
        if (!groups.empty() && std::abs(notes[idx].start - groups.back().start) < 0.35)
            groups.back().indices.push_back(idx);
        else
            groups.push_back({ notes[idx].start, { idx } });
    }
    return groups;
}

int scaleDegreePitch(int rootPc, Mode mode, int degree /*0..6*/, int octaveMidi)
{
    const auto& iv = modeIntervals(mode);
    const int pc = ((rootPc + iv[(size_t) (degree % 7)]) % 12 + 12) % 12;
    const int base = octaveMidi - (octaveMidi % 12);
    int p = base + pc;
    while (p < octaveMidi - 6) p += 12;
    while (p > octaveMidi + 6) p -= 12;
    return juce::jlimit(0, 127, p);
}

std::vector<RollNote> morphPianoOnce(const std::vector<RollNote>& notes, double amt, bool simplify,
                                     int seed, int rootPc, Mode mode)
{
    if (notes.empty()) return notes;
    const int root = rootPc >= 0 ? rootPc : (((notes.front().pitch % 12) + 12) % 12);
    auto groups = groupChords(notes);
    std::vector<RollNote> out = notes;
    const int baseId = nextGeneratedId(notes, seed);
    int gen = 0;

    if (simplify)
    {
        std::vector<char> keep(out.size(), 1);
        for (const auto& g : groups)
        {
            if (g.indices.size() <= 2)
                continue;
            // Keep bass + a chord tone near the middle; drop extensions first.
            size_t bassIdx = g.indices.front();
            int bassPitch = out[bassIdx].pitch;
            for (size_t idx : g.indices)
                if (out[idx].pitch < bassPitch)
                {
                    bassPitch = out[idx].pitch;
                    bassIdx = idx;
                }
            std::vector<size_t> ranked = g.indices;
            std::sort(ranked.begin(), ranked.end(), [&](size_t a, size_t b)
            {
                return std::abs(out[a].pitch - (bassPitch + 12))
                     < std::abs(out[b].pitch - (bassPitch + 12));
            });
            const int keepCount = juce::jmax(2, (int) std::lround((double) ranked.size() * (1.0 - 0.6 * amt)));
            for (size_t i = (size_t) keepCount; i < ranked.size(); ++i)
            {
                if (ranked[i] == bassIdx) continue;
                const uint32_t h = knuthHash((int) ranked[i] + seed);
                if ((h % 100u) < (uint32_t) (amt * 90.0))
                    keep[ranked[i]] = 0;
            }
            // Straighten syncopated attacks toward the beat.
            if (amt > 0.35)
            {
                int slot = ((int) std::lround(g.start)) % kStepsPerBar;
                if (slot < 0) slot += kStepsPerBar;
                if ((slot % 2) != 0)
                {
                    const double snapped = std::round(g.start);
                    for (size_t idx : g.indices)
                        if (keep[idx])
                            out[idx].start += (snapped - out[idx].start) * (0.4 + 0.5 * amt);
                }
            }
        }
        std::vector<RollNote> filtered;
        for (size_t i = 0; i < out.size(); ++i)
            if (keep[i]) filtered.push_back(out[i]);
        return filtered.empty() ? notes : filtered;
    }

    // Enrich: add scale-tone extensions and anticipate some attacks.
    for (const auto& g : groups)
    {
        if (g.indices.empty()) continue;
        int bass = out[g.indices.front()].pitch;
        int top = bass;
        for (size_t idx : g.indices)
        {
            bass = std::min(bass, out[idx].pitch);
            top = std::max(top, out[idx].pitch);
        }
        std::set<int> pcs;
        for (size_t idx : g.indices)
            pcs.insert(((out[idx].pitch % 12) + 12) % 12);

        const uint32_t h = knuthHash(seed * 19 + (int) std::lround(g.start * 4.0));
        // Prefer 7th then 9th (scale degrees 6 then 1 above root of chord ≈ bass pc).
        const int bassPc = ((bass % 12) + 12) % 12;
        auto tryAddDegree = [&](int deg, int registerMidi, double chance)
        {
            const int p = scaleDegreePitch(root, mode, deg, registerMidi);
            const int pc = ((p % 12) + 12) % 12;
            if (pcs.count(pc) != 0) return;
            if (!pitchInScale(p, root, mode)) return;
            if ((knuthHash((int) p + seed + deg) % 100u) >= (uint32_t) (chance * 100.0))
                return;
            RollNote n = out[g.indices.front()];
            n.id = baseId + gen++;
            n.pitch = juce::jlimit(0, 127, p);
            n.start = g.start;
            n.len = std::max(0.5, n.len);
            n.fileVelocity = juce::jmax(1, (int) std::lround(n.fileVelocity * 0.7));
            n.moved = true;
            out.push_back(n);
            pcs.insert(pc);
        };
        // Map chord-bass to nearest scale degree index for stacking 7/9 above it.
        int bassDeg = 0;
        {
            const auto& iv = modeIntervals(mode);
            int best = 99;
            for (int d = 0; d < 7; ++d)
            {
                const int dist = std::abs(((root + iv[(size_t) d]) % 12) - bassPc);
                const int wrap = std::min(dist, 12 - dist);
                if (wrap < best) { best = wrap; bassDeg = d; }
            }
        }
        const int midReg = (bass + top) / 2;
        tryAddDegree(bassDeg + 6, midReg, 0.25 + 0.55 * amt); // 7th, same register
        if (amt > 0.4)
            tryAddDegree(bassDeg + 1, midReg, 0.15 + 0.40 * amt); // 9th, same register

        // Anticipate a chord attack (syncopation).
        if ((h % 100u) < (uint32_t) (amt * 40.0) && g.start >= 1.0)
        {
            for (size_t idx : g.indices)
            {
                out[idx].start = std::max(0.0, out[idx].start - 1.0);
                out[idx].moved = true;
            }
        }
        juce::ignoreUnused(bassPc);
    }
    return out;
}

std::vector<RollNote> morphLineOnce(const std::vector<RollNote>& notes, double amt, bool simplify,
                                    int seed, int rootPc, Mode mode)
{
    std::vector<RollNote> out = notes;
    if (simplify)
    {
        std::vector<char> keep(out.size(), 1);
        for (size_t i = 0; i < out.size(); ++i)
        {
            if (metricWeight(out[i].start) >= 0.72)
                continue;
            const uint32_t h = knuthHash(out[i].id + seed);
            if ((h % 100u) < (uint32_t) (amt * 70.0))
                keep[i] = 0;
        }
        std::vector<RollNote> filtered;
        for (size_t i = 0; i < out.size(); ++i)
            if (keep[i]) filtered.push_back(out[i]);
        return filtered.empty() ? notes : filtered;
    }

    const int root = rootPc >= 0 ? rootPc : 0;
    const int baseId = nextGeneratedId(notes, seed);
    int gen = 0;
    std::vector<size_t> order(notes.size());
    for (size_t i = 0; i < notes.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b)
    {
        return notes[a].start < notes[b].start;
    });

    for (size_t i = 0; i + 1 < order.size(); ++i)
    {
        const auto& a = notes[order[i]];
        const auto& b = notes[order[i + 1]];
        const double gap = b.start - a.start;
        if (gap < 1.5 || gap > 8.0)
            continue;
        const uint32_t h = knuthHash(a.id * 23 + seed + (int) i);
        if ((h % 100u) >= (uint32_t) (amt * 70.0))
            continue;
        // Passing tone between two pitches, on the scale, in the same register.
        const int mid = (a.pitch + b.pitch) / 2;
        const int pass = rootPc >= 0 ? fitToScale(mid, root, mode) : mid;
        if (pass == a.pitch || pass == b.pitch)
            continue;
        RollNote n = a;
        n.id = baseId + gen++;
        n.pitch = juce::jlimit(0, 127, pass);
        n.start = a.start + gap * 0.5;
        n.len = std::max(0.5, gap * 0.4);
        n.fileVelocity = juce::jmax(1, (int) std::lround(a.fileVelocity * 0.6));
        n.moved = true;
        out.push_back(n);
    }

    // A few weak attacks move onto the offbeat. Pitches stay put.
    for (auto& n : out)
    {
        if (n.id >= baseId)
            continue;
        if (metricWeight(n.start) >= 0.72)
            continue;
        const uint32_t h = knuthHash(n.id * 11 + seed);
        if ((h % 100u) >= (uint32_t) (amt * 35.0))
            continue;
        const double nudged = std::max(0.0, n.start - 0.5);
        if (std::abs(nudged - n.start) > 0.1)
        {
            n.start = nudged;
            n.moved = true;
        }
    }
    return out;
}

std::vector<RollNote> applyComplexityMorph(std::vector<RollNote> notes,
                                           int sourceComplexity,
                                           int targetComplexity,
                                           ClipKind kind,
                                           int rootPc,
                                           Mode mode)
{
    sourceComplexity = juce::jlimit(1, 100, sourceComplexity);
    targetComplexity = juce::jlimit(0, 100, targetComplexity);
    if (notes.empty() || targetComplexity == sourceComplexity)
        return notes;

    const bool simplify = targetComplexity < sourceComplexity;
    const double amt = simplify
        ? (double) (sourceComplexity - targetComplexity) / (double) juce::jmax(1, sourceComplexity)
        : (double) (targetComplexity - sourceComplexity) / (double) juce::jmax(1, 100 - sourceComplexity);

    const auto anchors = anchorOnsets(notes, kind);
    constexpr int kCandidates = 12;
    std::vector<RollNote> best = notes;
    double bestScore = -1.0e9;
    bool found = false;

    for (int c = 0; c < kCandidates; ++c)
    {
        std::vector<RollNote> cand;
        if (kind == ClipKind::Drums)
            cand = morphDrumsOnce(notes, amt, simplify, 100 + c * 17);
        else if (kind == ClipKind::Keys)
            cand = morphPianoOnce(notes, amt, simplify, 200 + c * 19, rootPc, mode);
        else
            cand = morphLineOnce(notes, amt, simplify, 300 + c * 23, rootPc, mode);

        if (cand.empty())
            continue;
        if (kind == ClipKind::Drums && !noNewNonDrumPitches(notes, cand))
            continue;
        if (kind == ClipKind::Keys && !noNewOutOfScalePitches(notes, cand, rootPc, mode))
            continue;
        if (!anchorsPreserved(anchors, cand, kind))
            continue;
        if (notesNearlyIdentical(notes, cand))
            continue;

        const double score = directionScore(cand, kind, !simplify)
                           + hashSigned(c, 5) * 0.01; // tiny tie-break
        if (!found || score > bestScore)
        {
            bestScore = score;
            best = std::move(cand);
            found = true;
        }
    }
    return found ? best : notes;
}

std::vector<RollNote> applyMusicalVariation(std::vector<RollNote> notes, int variationIndex,
                                            ClipKind kind, int rootPc, Mode mode)
{
    if (notes.empty() || variationIndex <= 0)
        return notes;

    // Jam-take variations: same clip identity, different performance —
    // phrasing, density, octave placement, and rhythmic feel — not micro nudges.
    const int var = juce::jlimit(1, kMaxVariations, variationIndex);
    const auto anchors = anchorOnsets(notes, kind);
    constexpr int kCandidates = 14;
    std::vector<RollNote> best = notes;
    double bestScore = -1.0e9;
    bool found = false;

    auto differenceScore = [&](const std::vector<RollNote>& cand) -> double
    {
        // Prefer takes that clearly diverge while staying same-ballpark size.
        const double sizeRatio = (double) cand.size() / (double) juce::jmax(1, (int) notes.size());
        const double sizeScore = 1.0 - std::abs(sizeRatio - 1.0); // soft prefer ~same density
        int pitchMoves = 0, timeMoves = 0, matched = 0;
        auto byKey = [](const RollNote& a, const RollNote& b)
        {
            if (a.start < b.start) return true;
            if (b.start < a.start) return false;
            return a.pitch < b.pitch;
        };
        auto sa = notes, sb = cand;
        std::sort(sa.begin(), sa.end(), byKey);
        std::sort(sb.begin(), sb.end(), byKey);
        size_t i = 0, j = 0;
        while (i < sa.size() && j < sb.size())
        {
            if (std::abs(sa[i].start - sb[j].start) < 1.5
                && std::abs(sa[i].pitch - sb[j].pitch) <= 12)
            {
                ++matched;
                if (sa[i].pitch != sb[j].pitch) ++pitchMoves;
                if (std::abs(sa[i].start - sb[j].start) > 0.4) ++timeMoves;
                ++i; ++j;
            }
            else if (sa[i].start < sb[j].start)
                ++i;
            else
                ++j;
        }
        const double n = (double) juce::jmax(1, (int) notes.size());
        const double change = ((double) pitchMoves + (double) timeMoves
                               + std::abs((double) cand.size() - (double) notes.size())) / n;
        // Want audible change (high) but still some identity (matched fraction).
        const double identity = (double) matched / n;
        return change * 2.0 + identity * 0.35 + sizeScore * 0.25;
    };

    for (int c = 0; c < kCandidates; ++c)
    {
        const int seed = var * 7919 + c * 104729 + 17;
        const uint32_t modeRoll = knuthHash(seed * 3 + var * 11);
        std::vector<RollNote> cand = notes;
        const double amt = 0.45 + 0.022 * (double) var; // substantial morph strength

        if (kind == ClipKind::Drums)
        {
            // Take A: denser hats/ghosts. Take B: peel decoration. Mix with rhythm shifts.
            if ((modeRoll % 3u) == 0u)
                cand = morphDrumsOnce(cand, amt, true, seed);
            else
                cand = morphDrumsOnce(cand, amt, false, seed);

            for (auto& n : cand)
            {
                const uint32_t h = knuthHash(n.id * 11 + seed + var);
                const auto v = drumVoiceOf(n.pitch);
                // Move non-anchor hits by a 16th / 8th — a real alternate groove feel.
                if (!isDrumAnchor(n) && (h % 100u) < (uint32_t) (35 + var))
                {
                    const double shift = ((h & 1u) ? -1.0 : 1.0)
                        * (((h >> 2) & 1u) ? 1.0 : 2.0);
                    n.start = std::max(0.0, n.start + shift);
                    n.moved = true;
                }
                // Occasionally swap hat open/closed or ghost velocity for a new pocket.
                if (v == DrumVoice::Hat && (h % 100u) < 40u)
                {
                    n.pitch = (n.pitch == 46) ? 42 : ((h & 2u) ? 46 : 42);
                    n.fileVelocity = juce::jlimit(1, 127,
                        n.fileVelocity + ((h & 4u) ? 18 : -14));
                    n.moved = true;
                }
                else if (v == DrumVoice::Snare && !isDrumAnchor(n) && (h % 100u) < 30u)
                {
                    n.fileVelocity = juce::jlimit(1, 127, 35 + (int) (h % 40u));
                    n.moved = true;
                }
            }
            // Second morph pass at higher var = denser alternate kit layers.
            if (var >= 5)
                cand = morphDrumsOnce(cand, 0.35 + 0.03 * (double) var, false, seed + 99);
        }
        else if (kind == ClipKind::Keys)
        {
            auto groups = groupChords(cand);
            for (size_t gi = 0; gi < groups.size(); ++gi)
            {
                auto& g = groups[gi];
                const uint32_t h = knuthHash(seed + (int) gi * 13 + var * 7);
                // Invert / open voicing frequently.
                if ((h % 100u) < (uint32_t) (45 + var) && g.indices.size() >= 2)
                {
                    size_t bassIdx = g.indices.front();
                    size_t topIdx = g.indices.front();
                    for (size_t idx : g.indices)
                    {
                        if (cand[idx].pitch < cand[bassIdx].pitch) bassIdx = idx;
                        if (cand[idx].pitch > cand[topIdx].pitch) topIdx = idx;
                    }
                    const int oct = ((h & 1u) ? 12 : -12);
                    cand[bassIdx].pitch = juce::jlimit(0, 127, cand[bassIdx].pitch + oct);
                    cand[bassIdx].moved = true;
                    if ((h % 100u) < 50u)
                    {
                        cand[topIdx].pitch = juce::jlimit(0, 127, cand[topIdx].pitch - oct);
                        cand[topIdx].moved = true;
                    }
                }
                // Anticipation / delay by an 8th — syncopated jam take.
                if ((h % 100u) < (uint32_t) (40 + var) && g.start >= 1.0)
                {
                    const double shift = ((h >> 3) & 1u) ? -2.0 : 2.0;
                    for (size_t idx : g.indices)
                    {
                        cand[idx].start = std::max(0.0, cand[idx].start + shift);
                        cand[idx].moved = true;
                    }
                }
                // Drop or restage a mid-phrase chord for breathing room.
                if ((h % 100u) < (uint32_t) (12 + var / 2) && gi > 0 && gi + 1 < groups.size())
                {
                    for (size_t idx : g.indices)
                        cand[idx].fileVelocity = 1; // soft kill; cleaned below
                }
            }
            cand.erase(std::remove_if(cand.begin(), cand.end(),
                                      [](const RollNote& n) { return n.fileVelocity <= 1; }),
                       cand.end());
            cand = morphPianoOnce(cand, amt, (modeRoll % 2u) == 0u, seed, rootPc, mode);
            if (var >= 8)
                cand = morphPianoOnce(cand, 0.4, false, seed + 41, rootPc, mode);
        }
        else
        {
            // Bass / Lead: rewrite register, pickups, and rhythmic placement.
            for (auto& n : cand)
            {
                const uint32_t h = knuthHash(n.id * 7 + seed + var * 3);
                if ((h % 100u) < (uint32_t) (40 + var))
                {
                    n.pitch = juce::jlimit(0, 127, n.pitch + ((h & 1u) ? 12 : -12));
                    n.moved = true;
                }
                if ((h % 100u) < (uint32_t) (50 + var))
                {
                    // 8th or 16th displace — alternate phrasing, not humanize jitter.
                    const double grid = ((h >> 2) & 1u) ? 2.0 : 1.0;
                    n.start = std::max(0.0, n.start + (((h >> 3) & 1u) ? -grid : grid));
                    n.moved = true;
                }
                if ((h % 100u) < (uint32_t) (20 + var / 2))
                {
                    n.len = juce::jmax(0.25, n.len * (((h >> 4) & 1u) ? 1.6 : 0.55));
                    n.moved = true;
                }
            }
            // Drop some weak offbeats / add octave answers for a new line shape.
            if ((modeRoll % 2u) == 1u)
            {
                std::vector<RollNote> kept;
                kept.reserve(cand.size());
                const int baseId = nextGeneratedId(cand, seed);
                int gen = 0;
                for (const auto& n : cand)
                {
                    const uint32_t h = knuthHash(n.id + seed * 5);
                    if (metricWeight(n.start) < 0.55 && (h % 100u) < (uint32_t) (25 + var))
                        continue; // rest that pickup this take
                    kept.push_back(n);
                    if (metricWeight(n.start) >= 0.72 && (h % 100u) < (uint32_t) (18 + var / 2))
                    {
                        RollNote echo = n;
                        echo.id = baseId + gen++;
                        echo.start = n.start + 2.0;
                        echo.pitch = juce::jlimit(0, 127, n.pitch + ((h & 1u) ? 12 : -12));
                        echo.fileVelocity = juce::jmax(1, (int) std::lround(n.fileVelocity * 0.7));
                        echo.moved = true;
                        kept.push_back(echo);
                    }
                }
                if (!kept.empty())
                    cand = std::move(kept);
            }
        }

        if (cand.empty())
            continue;
        if (kind == ClipKind::Drums && !noNewNonDrumPitches(notes, cand))
            continue;
        if (kind == ClipKind::Keys && !noNewOutOfScalePitches(notes, cand, rootPc, mode))
            continue;
        // Looser skeleton: jam takes may move more anchors; keep ~1/3.
        {
            const auto afterAnchors = anchorOnsets(cand, kind);
            const double tol = kind == ClipKind::Keys ? 2.5 : 1.0;
            int matched = 0;
            for (double t : anchors)
                for (double u : afterAnchors)
                    if (std::abs(u - t) < tol) { ++matched; break; }
            if (!anchors.empty() && matched * 3 < (int) anchors.size())
                continue;
        }
        if (notesNearlyIdentical(notes, cand))
            continue;

        const double score = differenceScore(cand) + hashSigned(c + var, 8) * 0.02;
        if (!found || score > bestScore)
        {
            bestScore = score;
            best = std::move(cand);
            found = true;
        }
    }
    return found ? best : notes;
}

/** Revoice: keep each note's rhythm and register, and play the scale degrees
    in a new order. amount 0 is the original line. */
std::vector<RollNote> applyRevoice(const std::vector<RollNote>& notes, int amount,
                                     int rootPc, Mode scaleMode)
{
    amount = juce::jlimit(0, kMaxRevoice, amount);
    if (amount <= 0 || notes.empty())
        return notes;

    const int root = rootPc >= 0 ? ((rootPc % 12) + 12) % 12
                                 : ((notes.front().pitch % 12) + 12) % 12;
    const Mode mode = scaleMode;

    std::vector<size_t> order(notes.size());
    for (size_t i = 0; i < notes.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b)
    {
        if (notes[a].start < notes[b].start) return true;
        if (notes[b].start < notes[a].start) return false;
        return notes[a].pitch < notes[b].pitch;
    });

    auto degreeOf = [&](int pitch) -> int
    {
        const int pc = ((pitch % 12) + 12) % 12;
        const auto& iv = modeIntervals(mode);
        int best = 0;
        int bestDist = 99;
        for (int d = 0; d < 7; ++d)
        {
            const int tone = (root + iv[(size_t) d]) % 12;
            const int dist = std::min(std::abs(tone - pc), 12 - std::abs(tone - pc));
            if (dist < bestDist)
            {
                bestDist = dist;
                best = d;
            }
        }
        return best;
    };

    std::vector<int> degrees(order.size());
    for (size_t i = 0; i < order.size(); ++i)
        degrees[i] = degreeOf(notes[order[i]].pitch);

    const int n = (int) degrees.size();
    int shift = juce::jmax(1, (amount * juce::jmax(1, n - 1)) / juce::jmax(1, kMaxRevoice));
    if (n > 0)
        shift %= n;
    if (shift == 0)
        shift = 1;
    std::vector<int> spun(n);
    for (int i = 0; i < n; ++i)
        spun[(size_t) i] = degrees[(size_t) ((i + shift) % n)];

    std::vector<RollNote> out = notes;
    for (int i = 0; i < n; ++i)
    {
        auto& note = out[order[(size_t) i]];
        const int placed = scaleDegreePitch(root, mode, spun[(size_t) i], note.pitch);
        if (placed == note.pitch)
            continue;
        note.pitch = placed;
        note.moved = true;
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

namespace {

/** Copy notes in `ids` from `varied` onto `base`. Match by id first; if an id
    is missing (rewritten), match by the original note's start+pitch. */
std::vector<RollNote> mergeVariedSelection(const std::vector<RollNote>& base,
                                           const std::vector<RollNote>& varied,
                                           const std::set<int>& ids,
                                           const std::vector<RollNote>& originals)
{
    if (ids.empty())
        return varied;

    std::map<int, const RollNote*> byId;
    for (const auto& n : varied)
        byId[n.id] = &n;

    struct Key { double start; int pitch; };
    auto keyLess = [](const Key& a, const Key& b)
    {
        if (std::abs(a.start - b.start) > 1.0e-6) return a.start < b.start;
        return a.pitch < b.pitch;
    };
    std::map<Key, const RollNote*, decltype(keyLess)> byStartPitch(keyLess);
    for (const auto& n : varied)
        byStartPitch[{ n.start, n.pitch }] = &n;

    std::map<int, Key> originalKey;
    for (const auto& n : originals)
        if (ids.count(n.id) > 0)
            originalKey[n.id] = { n.start, n.pitch };

    std::vector<RollNote> out = base;
    for (auto& n : out)
    {
        if (ids.count(n.id) == 0)
            continue;
        if (const auto it = byId.find(n.id); it != byId.end())
        {
            n = *it->second;
            continue;
        }
        if (const auto ok = originalKey.find(n.id); ok != originalKey.end())
            if (const auto it = byStartPitch.find(ok->second); it != byStartPitch.end())
                n = *it->second;
    }
    return out;
}

} // namespace

std::vector<RollNote> applyGroove(const std::vector<RollNote>& notes, const GrooveParams& k,
                                  int sourceComplexity,
                                  ClipKind kind,
                                  int rootPc,
                                  Mode mode,
                                  NoteProcessScope scope)
{
    // Selection: transform only the notes inside the region, then merge them
    // back. Delay can add notes, so the subset is replaced as a whole.
    // Variations need the full clip for musical context — build on everything,
    // then copy only the selected notes onto the scoped result.
    if (scope.restricts())
    {
        if (k.variationIndex > 0
            && scope.selectedIds != nullptr
            && !scope.selectedIds->empty())
        {
            GrooveParams noVar = k;
            noVar.variationIndex = 0;
            auto scoped = applyGroove(notes, noVar, sourceComplexity, kind, rootPc, mode, scope);
            auto fullFx = applyGroove(notes, noVar, sourceComplexity, kind, rootPc, mode);
            auto varied = applyMusicalVariation(std::move(fullFx), k.variationIndex, kind, rootPc, mode);
            return mergeVariedSelection(scoped, varied, *scope.selectedIds, notes);
        }

        std::vector<RollNote> subset, rest;
        subset.reserve(notes.size());
        rest.reserve(notes.size());
        for (const auto& n : notes)
            (scope.includes(n) ? subset : rest).push_back(n);
        auto processed = applyGroove(subset, k, sourceComplexity, kind, rootPc, mode);
        rest.insert(rest.end(), processed.begin(), processed.end());
        std::stable_sort(rest.begin(), rest.end(), [](const RollNote& a, const RollNote& b)
        {
            if (std::abs(a.start - b.start) > 1.0e-9) return a.start < b.start;
            if (a.pitch != b.pitch) return a.pitch < b.pitch;
            return a.id < b.id;
        });
        return rest;
    }

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

    std::vector<RollNote> sorted = (k.revoice > 0 && kind != ClipKind::Drums)
                                       ? applyRevoice(notes, k.revoice, rootPc, mode)
                                       : notes;
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
        out = applyComplexityMorph(std::move(out), srcCx, tgtCx, kind, rootPc, mode);

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

    if (k.variationIndex > 0 && !out.empty())
        out = applyMusicalVariation(std::move(out), k.variationIndex, kind, rootPc, mode);

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

NoteGroove noteGrooveFrom(const GrooveParams& g)
{
    NoteGroove n;
    n.swing = g.swing;
    n.pocket = g.pocket;
    n.humanize = g.humanize;
    n.dynamics = g.dynamics;
    n.length = g.length;
    n.intensity = g.intensity;
    n.swingGridIndex = g.swingGridIndex;
    n.quantizeGridIndex = g.quantizeGridIndex;
    n.quantizeStrength = g.quantizeStrength;
    n.articulationIndex = g.articulationIndex;
    n.articulationStrength = g.articulationStrength;
    n.sustainPedalMode = g.sustainPedalMode;
    n.complexityTarget = g.complexityTarget;
    n.delayTimeIndex = g.delayTimeIndex;
    n.delayAmount = g.delayAmount;
    n.delayFeedback = g.delayFeedback;
    n.velocityRangeLo = g.velocityRangeLo;
    n.velocityRangeHi = g.velocityRangeHi;
    n.variationIndex = g.variationIndex;
    n.revoice = g.revoice;
    n.strumDirectionIndex = g.strumDirectionIndex;
    n.strumSpeed = g.strumSpeed;
    n.strumAmount = g.strumAmount;
    return n;
}

void applyNoteGroove(GrooveParams& g, const NoteGroove& stored)
{
    g.swing = stored.swing;
    g.pocket = stored.pocket;
    g.humanize = stored.humanize;
    g.dynamics = stored.dynamics;
    g.length = stored.length;
    g.intensity = stored.intensity;
    g.swingGridIndex = stored.swingGridIndex;
    g.quantizeGridIndex = stored.quantizeGridIndex;
    g.quantizeStrength = stored.quantizeStrength;
    g.articulationIndex = stored.articulationIndex;
    g.articulationStrength = stored.articulationStrength;
    g.sustainPedalMode = stored.sustainPedalMode;
    g.complexityTarget = stored.complexityTarget;
    g.delayTimeIndex = stored.delayTimeIndex;
    g.delayAmount = stored.delayAmount;
    g.delayFeedback = stored.delayFeedback;
    g.velocityRangeLo = stored.velocityRangeLo;
    g.velocityRangeHi = stored.velocityRangeHi;
    g.variationIndex = stored.variationIndex;
    g.revoice = stored.revoice;
    g.strumDirectionIndex = stored.strumDirectionIndex;
    g.strumSpeed = stored.strumSpeed;
    g.strumAmount = stored.strumAmount;
}

bool clipFxIsDefault(const GrooveParams& g)
{
    return noteGrooveFrom(g) == NoteGroove{};
}

std::vector<RollNote> applyStoredGrooves(const std::vector<RollNote>& notes,
                                         const std::map<int, NoteGroove>& stored,
                                         const GrooveParams& base,
                                         int sourceComplexity,
                                         ClipKind kind,
                                         int rootPc,
                                         Mode mode)
{
    std::set<int> variationIndices;
    for (const auto& [id, fx] : stored)
        if (fx.variationIndex > 0)
            variationIndices.insert(fx.variationIndex);

    struct Group
    {
        NoteGroove fx;
        std::vector<RollNote> notes;
    };
    std::vector<Group> groups;
    for (const auto& n : notes)
    {
        NoteGroove fx{};
        if (const auto it = stored.find(n.id); it != stored.end())
            fx = it->second;
        // Defer variation so it can run on the full clip, then merge.
        fx.variationIndex = 0;
        bool found = false;
        for (auto& gr : groups)
        {
            if (gr.fx == fx)
            {
                gr.notes.push_back(n);
                found = true;
                break;
            }
        }
        if (!found)
            groups.push_back(Group{ fx, { n } });
    }

    std::vector<RollNote> out;
    out.reserve(notes.size());
    for (auto& gr : groups)
    {
        GrooveParams k = base;
        applyNoteGroove(k, gr.fx);
        k.variationIndex = 0;
        auto part = applyGroove(gr.notes, k, sourceComplexity, kind, rootPc, mode);
        out.insert(out.end(), part.begin(), part.end());
    }
    std::stable_sort(out.begin(), out.end(), [](const RollNote& a, const RollNote& b)
    {
        if (std::abs(a.start - b.start) > 1.0e-9) return a.start < b.start;
        if (a.pitch != b.pitch) return a.pitch < b.pitch;
        return a.id < b.id;
    });

    if (variationIndices.empty())
        return out;

    const auto baseOut = out;
    for (int var : variationIndices)
    {
        auto varied = applyMusicalVariation(baseOut, var, kind, rootPc, mode);
        std::set<int> ids;
        for (const auto& [id, fx] : stored)
            if (fx.variationIndex == var)
                ids.insert(id);
        out = mergeVariedSelection(out, varied, ids, notes);
    }
    return out;
}

} // namespace pflow
