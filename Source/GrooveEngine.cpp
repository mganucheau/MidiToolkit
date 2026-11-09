#include "GrooveEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
        target.arpVariation = locked.arpVariation;
        target.arpNonce = locked.arpNonce;
        target.melodyEnabled = locked.melodyEnabled;
        target.melodyInput = locked.melodyInput;
        target.melodyStyleIndex = locked.melodyStyleIndex;
        target.melodyContourIndex = locked.melodyContourIndex;
        target.melodySpeedIndex = locked.melodySpeedIndex;
        target.melodyPhraseIndex = locked.melodyPhraseIndex;
        target.melodyVariation = locked.melodyVariation;
        target.melodyNonce = locked.melodyNonce;
        target.chordsEnabled = locked.chordsEnabled;
        target.chordsInput = locked.chordsInput;
        target.chordVoicingIndex = locked.chordVoicingIndex;
        target.chordColorIndex = locked.chordColorIndex;
        target.chordRhythmIndex = locked.chordRhythmIndex;
        target.chordVariation = locked.chordVariation;
        target.chordNonce = locked.chordNonce;
        target.bassEnabled = locked.bassEnabled;
        target.bassInput = locked.bassInput;
        target.bassMotionIndex = locked.bassMotionIndex;
        target.bassVariation = locked.bassVariation;
        target.bassNonce = locked.bassNonce;
        target.drumsEnabled = locked.drumsEnabled;
        target.drumStyleIndex = locked.drumStyleIndex;
        target.drumDensity = locked.drumDensity;
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
        tryAddDegree(bassDeg + 6, top, 0.25 + 0.55 * amt); // 7th-ish
        if (amt > 0.4)
            tryAddDegree(bassDeg + 1, top + 4, 0.15 + 0.40 * amt); // 9th-ish

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
    for (const auto& n : notes)
    {
        if (metricWeight(n.start) < 0.72)
            continue;
        const uint32_t h = knuthHash(n.id * 23 + seed);
        if ((h % 100u) >= (uint32_t) (amt * 55.0))
            continue;
        // Approach note a scale step below, one 16th early.
        int approach = n.pitch - 1;
        if (rootPc >= 0)
            approach = fitToScale(n.pitch - 2, root, mode);
        RollNote a = n;
        a.id = baseId + gen++;
        a.pitch = juce::jlimit(0, 127, approach);
        a.start = std::max(0.0, n.start - 1.0);
        a.len = 0.75;
        a.fileVelocity = juce::jmax(1, (int) std::lround(n.fileVelocity * 0.55));
        a.moved = true;
        out.push_back(a);
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

    const int var = juce::jlimit(1, 16, variationIndex);
    const auto anchors = anchorOnsets(notes, kind);
    constexpr int kCandidates = 10;
    std::vector<RollNote> best = notes;
    double bestScore = -1.0e9;
    bool found = false;

    for (int c = 0; c < kCandidates; ++c)
    {
        const int seed = var * 97 + c * 31;
        std::vector<RollNote> cand = notes;

        if (kind == ClipKind::Drums)
        {
            // Vary decoration only: nudge non-anchor hats/perc timing & drop/add ghosts.
            for (auto& n : cand)
            {
                if (isDrumAnchor(n))
                    continue;
                const uint32_t h = knuthHash(n.id * 11 + seed);
                if (drumVoiceOf(n.pitch) == DrumVoice::Hat || drumVoiceOf(n.pitch) == DrumVoice::Perc)
                {
                    if ((h % 100u) < 40u)
                        n.start += hashSigned(n.id + seed, 2) * 0.35;
                    n.moved = true;
                }
                else if (drumVoiceOf(n.pitch) == DrumVoice::Snare && n.fileVelocity < 55)
                {
                    if ((h % 100u) < 30u)
                        n.start += hashSigned(n.id + seed, 3) * 0.25;
                    n.moved = true;
                }
            }
            if (var >= 6)
            {
                // Mild hat density swap via a light complexity nudge.
                cand = morphDrumsOnce(cand, 0.25 + 0.04 * (double) var, false, seed);
            }
        }
        else if (kind == ClipKind::Keys)
        {
            auto groups = groupChords(cand);
            for (size_t gi = 0; gi < groups.size(); ++gi)
            {
                auto& g = groups[gi];
                const uint32_t h = knuthHash(seed + (int) gi * 13);
                // Rotate voicing: move bass up an octave (inversion).
                if ((h % 100u) < (uint32_t) (20 + var) && g.indices.size() >= 2)
                {
                    size_t bassIdx = g.indices.front();
                    for (size_t idx : g.indices)
                        if (cand[idx].pitch < cand[bassIdx].pitch)
                            bassIdx = idx;
                    cand[bassIdx].pitch = juce::jlimit(0, 127, cand[bassIdx].pitch + 12);
                    cand[bassIdx].moved = true;
                }
                // Syncopate every other chord.
                if ((h % 100u) < (uint32_t) (15 + var) && g.start >= 1.0 && (gi % 2) == 1)
                {
                    for (size_t idx : g.indices)
                    {
                        cand[idx].start = std::max(0.0, cand[idx].start - 1.0);
                        cand[idx].moved = true;
                    }
                }
            }
            // Optional diatonic recolor via a light enrich pass.
            if (var >= 8)
                cand = morphPianoOnce(cand, 0.2, false, seed, rootPc, mode);
        }
        else
        {
            // Bass / Lead: octave flips and pickup timing, keep contour pitches.
            for (auto& n : cand)
            {
                const uint32_t h = knuthHash(n.id * 7 + seed);
                if (metricWeight(n.start) >= 0.72 && (h % 100u) < (uint32_t) (10 + var))
                {
                    n.pitch = juce::jlimit(0, 127, n.pitch + ((h & 1u) ? 12 : -12));
                    n.moved = true;
                }
                else if ((h % 100u) < 35u)
                {
                    n.start += hashSigned(n.id + var, 4) * 0.3;
                    n.moved = true;
                }
            }
        }

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

        // Prefer candidates that still feel related: moderate note-count delta.
        const double sizeDelta = std::abs((double) cand.size() - (double) notes.size())
                               / (double) juce::jmax(1, (int) notes.size());
        const double score = 1.0 - sizeDelta + hashSigned(c, 8) * 0.01;
        if (!found || score > bestScore)
        {
            bestScore = score;
            best = std::move(cand);
            found = true;
        }
    }
    return found ? best : notes;
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
                                  int sourceComplexity,
                                  ClipKind kind,
                                  int rootPc,
                                  Mode mode)
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

} // namespace pflow
