#include "GeneratorEngines.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace pflow {
namespace gen {
namespace {

float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

std::uint32_t mix(std::uint32_t v)
{
    v ^= v >> 16;
    v *= 0x7feb352dU;
    v ^= v >> 15;
    v *= 0x846ca68bU;
    v ^= v >> 16;
    return v;
}

float unit(std::uint32_t seed, int a, int b = 0, int salt = 0)
{
    const auto h = mix(seed
                       ^ (std::uint32_t) a * 2246822519u
                       ^ ((std::uint32_t) b << 8)
                       ^ (std::uint32_t) salt * 374761393u);
    return (float) (h & 0xFFFFFFu) / (float) 0x1000000u;
}

double delayPeriodBeats(DelayTime t)
{
    switch (t)
    {
        case DelayTime::Quarter:       return 1.0;
        case DelayTime::DottedEighth:  return 0.75;
        case DelayTime::Eighth:        return 0.5;
        case DelayTime::EighthT:       return 1.0 / 3.0;
        case DelayTime::Sixteenth:     return 0.25;
        case DelayTime::SixteenthT:    return 1.0 / 6.0;
        case DelayTime::ThirtySecond:  return 0.125;
        case DelayTime::Count:         break;
    }
    return 0.25;
}

int phraseBars(MelodyPhrase p)
{
    switch (p)
    {
        case MelodyPhrase::OneBar:    return 1;
        case MelodyPhrase::TwoBars:   return 2;
        case MelodyPhrase::FourBars:  return 4;
        case MelodyPhrase::EightBars: return 8;
        case MelodyPhrase::Count:     break;
    }
    return 4;
}

void sortEvents(std::vector<Event>& out)
{
    std::sort(out.begin(), out.end(), [](const Event& a, const Event& b)
    {
        if (a.ppq != b.ppq) return a.ppq < b.ppq;
        if (a.note != b.note) return a.note < b.note;
        return a.velocity < b.velocity;
    });
}

int toneOf(const Harmony& h, int degree)
{
    switch (degree)
    {
        case 1: return (h.root + h.third) % 12;
        case 2: return (h.root + h.fifth) % 12;
        case 3: return h.seventh >= 0 ? (h.root + h.seventh) % 12 : (h.root + h.fifth) % 12;
        default: return h.root;
    }
}

} // namespace

const int* scaleDegrees(Mode mode, int& count)
{
    static constexpr int major[] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int minor[] = { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr int dorian[] = { 0, 2, 3, 5, 7, 9, 10 };
    static constexpr int mixo[] = { 0, 2, 4, 5, 7, 9, 10 };
    count = 7;
    switch (mode)
    {
        case Mode::Aeolian:
        case Mode::Phrygian:
        case Mode::Locrian: return minor;
        case Mode::Dorian:  return dorian;
        case Mode::Mixolydian: return mixo;
        case Mode::Ionian:
        case Mode::Lydian:
        default: return major;
    }
}

int pitchInRange(int pitchClass, int low, int high, int prefer)
{
    int best = low;
    int bestDistance = 999;
    const int pc = ((pitchClass % 12) + 12) % 12;
    const int bottom = std::max(0, low);
    const int top = std::min(127, high);
    for (int note = bottom; note <= top; ++note)
    {
        if (note % 12 != pc) continue;
        const int distance = std::abs(note - prefer);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = note;
        }
    }
    return best;
}

Harmony progressionHarmony(int keyRoot, Mode mode, int bar)
{
    const int root = juce::jlimit(0, 11, keyRoot);
    struct Degree { int semitone; int third; };
    const Degree major[] = { { 0, 4 }, { 7, 4 }, { 9, 3 }, { 5, 4 } };
    const Degree minor[] = { { 0, 3 }, { 5, 3 }, { 7, 3 }, { 8, 4 } };
    const bool isMinor = mode == Mode::Aeolian || mode == Mode::Dorian
                      || mode == Mode::Phrygian || mode == Mode::Locrian;
    const Degree degree = (isMinor ? minor : major)[(std::max(bar, 0) / 2) % 4];
    Harmony h;
    h.root = (root + degree.semitone) % 12;
    h.third = degree.third;
    h.fifth = 7;
    return h;
}

Harmony harmonyFromHeld(const int* chordNotes, int chordCount, int keyRoot, Mode mode, int bar)
{
    if (chordNotes != nullptr && chordCount > 0)
    {
        bool present[12] = {};
        int lowest = 127;
        int used = 0;
        for (int i = 0; i < chordCount; ++i)
        {
            const int note = chordNotes[i];
            if (note < 0 || note > 127) continue;
            present[note % 12] = true;
            lowest = std::min(lowest, note);
            ++used;
        }
        if (used > 0)
        {
            Harmony h;
            h.root = lowest % 12;
            h.third = present[(h.root + 3) % 12] ? 3 : 4;
            h.fifth = present[(h.root + 6) % 12] ? 6 : 7;
            if (present[(h.root + 10) % 12]) h.seventh = 10;
            else if (present[(h.root + 11) % 12]) h.seventh = 11;
            return h;
        }
    }
    return progressionHarmony(keyRoot, mode, bar);
}

// ── Arp ──────────────────────────────────────────────────────────────────────

void buildArpSequence(const GrooveParams& g, const int* chordNotes, int chordCount,
                      int keyRoot, Mode mode, std::vector<int>& sequence)
{
    sequence.clear();
    std::vector<int> unique;
    if (chordNotes != nullptr)
    {
        for (int i = 0; i < chordCount; ++i)
        {
            const int note = chordNotes[i];
            if (note < 0 || note > 127) continue;
            if (std::find(unique.begin(), unique.end(), note) == unique.end())
                unique.push_back(note);
        }
    }
    if (unique.empty())
    {
        // Algo triad in the current key.
        const int root = 60 + juce::jlimit(0, 11, keyRoot);
        int count = 0;
        const int* deg = scaleDegrees(mode, count);
        unique.push_back(root);
        unique.push_back(root + deg[2]);
        unique.push_back(root + deg[4]);
    }

    if (g.arpDirection() != ArpDirection::Random)
        std::sort(unique.begin(), unique.end());
    if (g.arpDirection() == ArpDirection::Down)
        std::reverse(unique.begin(), unique.end());

    const int octaves = juce::jlimit(1, 3, g.arpOctaves);
    for (int octave = 0; octave < octaves; ++octave)
    {
        for (int note : unique)
        {
            const int shifted = note + octave * 12
                * (g.arpDirection() == ArpDirection::Down ? -1 : 1);
            if (shifted >= 0 && shifted <= 127)
                sequence.push_back(shifted);
        }
    }

    if ((g.arpDirection() == ArpDirection::UpDown || g.arpShape() == ArpShape::Arch)
        && sequence.size() > 2)
    {
        const auto rising = sequence;
        for (int i = (int) rising.size() - 2; i > 0; --i)
            sequence.push_back(rising[(size_t) i]);
    }

    switch (g.arpShape())
    {
        case ArpShape::Skip:
            if (sequence.size() > 1)
            {
                std::vector<int> skipped;
                for (size_t i = 0; i < sequence.size(); i += 2)
                    skipped.push_back(sequence[i]);
                sequence.swap(skipped);
            }
            break;
        case ArpShape::Pedal:
            if (!sequence.empty())
            {
                std::vector<int> pedal;
                const int held = sequence.front();
                for (int note : sequence)
                {
                    pedal.push_back(held);
                    if (note != held) pedal.push_back(note);
                }
                sequence.swap(pedal);
            }
            break;
        case ArpShape::Climb:
            if (!sequence.empty())
            {
                const auto base = sequence;
                for (int note : base)
                {
                    const int lifted = note + 12;
                    if (lifted <= 127) sequence.push_back(lifted);
                }
            }
            break;
        case ArpShape::Walk:
        case ArpShape::Arch:
        case ArpShape::Count:
            break;
    }
}

int arpNoteForStep(const GrooveParams& g, const std::vector<int>& sequence, int step)
{
    if (sequence.empty()) return 60;
    const std::uint32_t seed = (std::uint32_t) (g.arpVariation * 97 + g.arpNonce * 13);
    if (g.arpDirection() == ArpDirection::Random)
    {
        const int index = (int) (unit(seed, step) * (float) sequence.size());
        return sequence[(size_t) std::min(index, (int) sequence.size() - 1)];
    }
    return sequence[(size_t) (step % (int) sequence.size())];
}

void renderArp(const GrooveParams& g, double startPpq, double endPpq,
               const int* chordNotes, int chordCount, int keyRoot, Mode mode,
               std::vector<Event>& out)
{
    out.clear();
    if (!(endPpq > startPpq) || !g.arpEnabled) return;
    if (g.arpInput && (chordNotes == nullptr || chordCount <= 0)) return;

    std::vector<int> sequence;
    buildArpSequence(g, chordNotes, chordCount, keyRoot, mode, sequence);
    if (sequence.empty()) return;

    const double stepLength = delayPeriodBeats(g.arpRate());
    const float gate = clampf(g.arpGate / 100.0f, 0.05f, 1.0f);
    const int first = std::max(0, (int) std::floor((startPpq - stepLength) / stepLength) - 1);
    const int last = (int) std::ceil(endPpq / stepLength) + 1;
    const std::uint32_t seed = (std::uint32_t) (g.arpVariation * 97 + g.arpNonce * 13);

    for (int step = first; step <= last; ++step)
    {
        double onPpq = step * stepLength;
        // Seed spin: occasional skip for color, anchors stay on strong beats.
        if ((step % 4) != 0 && unit(seed, step + 77) < 0.12f)
            continue;
        const double offPpq = onPpq + stepLength * (double) gate;
        const int note = arpNoteForStep(g, sequence, step);
        const bool strong = (step % 4) == 0;
        const int velocity = strong ? 104 : 82;
        if (onPpq >= startPpq && onPpq < endPpq)
            out.push_back({ onPpq, note, velocity });
        if (offPpq >= startPpq && offPpq < endPpq)
            out.push_back({ offPpq, note, 0 });
    }
    sortEvents(out);
}

// ── Melody ───────────────────────────────────────────────────────────────────

bool melodyStepPlays(const GrooveParams& g, int phraseBar, int step)
{
    const auto style = g.melodyStyle();
    const auto speed = g.melodySpeed();
    const std::uint32_t seed = (std::uint32_t) (g.melodyVariation * 97 + g.melodyNonce * 13);
    const float roll = unit(seed, phraseBar, step, 8);

    if (speed == MelodySpeed::Slow && (step % 2) == 1) return false;
    if (speed == MelodySpeed::Medium && (step % 4) == 1) return false;

    const bool downbeat = step == 0 || step == 8;
    switch (style)
    {
        case MelodyStyle::March:
            return step == 0 || step == 4 || step == 8 || step == 12
                || ((step % 2) == 0 && roll < 0.35f);
        case MelodyStyle::Arch:
        {
            const float mid = 1.0f - std::abs(step - 8) / 8.0f;
            return downbeat || roll < mid * 0.7f;
        }
        case MelodyStyle::Call:
            return phraseBar % 2 == 0
                ? (downbeat || step == 4 || roll < 0.45f)
                : (downbeat || roll < 0.2f);
        case MelodyStyle::Turn:
            return downbeat || step == 6 || step == 7 || step == 14 || step == 15
                || ((step % 4) == 2 && roll < 0.4f);
        case MelodyStyle::Hold:
            return downbeat || (step == 4 && roll < 0.25f);
        case MelodyStyle::Drift:
            return downbeat || ((step % 2) == 1 && roll < 0.35f)
                || (step == 10 && roll < 0.5f);
        case MelodyStyle::Count:
            break;
    }
    return downbeat;
}

int melodyNote(const GrooveParams& g, const Harmony& harmony, int phraseBar, int step,
               int keyRoot, Mode mode)
{
    int count = 0;
    const int* deg = scaleDegrees(mode, count);
    const std::uint32_t seed = (std::uint32_t) (g.melodyVariation * 97 + g.melodyNonce * 13);

    int bias = 0;
    switch (g.melodyContour())
    {
        case MelodyContour::Rise: bias = step - 8; break;
        case MelodyContour::Fall: bias = 8 - step; break;
        case MelodyContour::Leap: bias = (step % 4) == 0 ? 0 : 5; break;
        case MelodyContour::Even:
        case MelodyContour::Count: break;
    }
    const int center = juce::jlimit(36, 96, 67 + bias);
    const int low = std::max(36, center - 12);
    const int high = std::min(96, center + 12);
    const bool downbeat = step == 0 || step == 8;

    if (downbeat || g.melodyContour() != MelodyContour::Leap)
    {
        const int chordPcs[] = {
            harmony.root,
            (harmony.root + harmony.third) % 12,
            (harmony.root + harmony.fifth) % 12
        };
        const int pick = (int) (unit(seed, phraseBar, step, 11) * 3.0f) % 3;
        if (downbeat)
            return pitchInRange(chordPcs[pick], low, high, center);
    }

    int index = 0;
    for (int i = 0; i < count; ++i)
        if ((keyRoot + deg[i]) % 12 == harmony.root) { index = i; break; }
    const float amount = unit(seed, phraseBar, step, 13);
    const int hops = 1 + (int) (amount * 3.0f);
    const int direction = amount < 0.5f ? -1 : 1;
    int at = index + direction * hops;
    int octShift = 0;
    while (at < 0) { at += count; --octShift; }
    while (at >= count) { at -= count; ++octShift; }
    const int pc = (keyRoot + deg[at]) % 12;
    return pitchInRange(pc, low, high, center + octShift * 12);
}

void renderMelody(const GrooveParams& g, double startPpq, double endPpq,
                  const int* chordNotes, int chordCount, int keyRoot, Mode mode,
                  std::vector<Event>& out)
{
    out.clear();
    if (!(endPpq > startPpq) || !g.melodyEnabled) return;
    if (g.melodyInput && (chordNotes == nullptr || chordCount <= 0)) return;

    const int bars = phraseBars(g.melodyPhrase());
    const int first = std::max(0, (int) std::floor((startPpq - 1.0) / 0.25) - 1);
    const int last = (int) std::ceil((endPpq + 0.25) / 0.25) + 1;

    for (int index = first; index <= last; ++index)
    {
        const int bar = index / 16;
        const int step = index % 16;
        if (bar < 0) continue;
        const int phraseBar = bar % bars;
        if (!melodyStepPlays(g, phraseBar, step)) continue;

        const Harmony harmony = g.melodyInput
            ? harmonyFromHeld(chordNotes, chordCount, keyRoot, mode, bar)
            : progressionHarmony(keyRoot, mode, bar);
        const int note = melodyNote(g, harmony, phraseBar, step, keyRoot, mode);
        const bool downbeat = step == 0 || step == 8;
        const double onPpq = bar * 4.0 + step * 0.25;
        const double length = downbeat ? 0.7 : 0.32;
        const int velocity = downbeat ? 104 : 82;
        if (onPpq >= startPpq && onPpq < endPpq)
            out.push_back({ onPpq, note, velocity });
        const double offPpq = onPpq + length;
        if (offPpq >= startPpq && offPpq < endPpq)
            out.push_back({ offPpq, note, 0 });
    }
    sortEvents(out);
}

// ── Chords ───────────────────────────────────────────────────────────────────

void chordVoices(const GrooveParams& g, const Harmony& harmony, std::vector<int>& pitches)
{
    pitches.clear();
    const bool seventh = g.chordColor() == ChordColor::Seventh || harmony.seventh >= 0;
    const int pcs[4] = {
        harmony.root,
        (harmony.root + harmony.third) % 12,
        (harmony.root + harmony.fifth) % 12,
        seventh ? (harmony.seventh >= 0 ? (harmony.root + harmony.seventh) % 12
                                        : (harmony.root + 10) % 12)
                : -1
    };
    const int n = seventh ? 4 : 3;
    int prefers[4] = { 52, 60, 67, 72 };
    switch (g.chordVoicing())
    {
        case ChordVoicing::Open:
            prefers[0] = 48; prefers[1] = 60; prefers[2] = 67; prefers[3] = 79;
            break;
        case ChordVoicing::Drop:
            prefers[0] = 55; prefers[1] = 60; prefers[2] = 64; prefers[3] = 72;
            break;
        case ChordVoicing::Close:
        case ChordVoicing::Count:
            break;
    }
    for (int i = 0; i < n; ++i)
    {
        if (pcs[i] < 0) continue;
        pitches.push_back(pitchInRange(pcs[i], 48, 84, prefers[i]));
    }
    if (g.chordVoicing() == ChordVoicing::Drop && pitches.size() >= 2)
        std::swap(pitches[0], pitches[1]);
}

Harmony chordHarmony(const GrooveParams& g, const int* chordNotes, int chordCount,
                     int keyRoot, Mode mode, int bar)
{
    if (g.chordsInput && chordNotes != nullptr && chordCount > 0)
    {
        // Lowest held note picks a diatonic degree; extras can add a 7th.
        const int lowest = *std::min_element(chordNotes, chordNotes + chordCount);
        int count = 0;
        const int* deg = scaleDegrees(mode, count);
        int best = 0;
        int bestDist = 12;
        for (int i = 0; i < count; ++i)
        {
            const int pc = (keyRoot + deg[i]) % 12;
            const int d = std::min(std::abs(lowest % 12 - pc), 12 - std::abs(lowest % 12 - pc));
            if (d < bestDist) { bestDist = d; best = i; }
        }
        Harmony h;
        h.root = (keyRoot + deg[best]) % 12;
        const bool isMinor = mode == Mode::Aeolian || mode == Mode::Dorian
                          || mode == Mode::Phrygian || mode == Mode::Locrian;
        // Diatonic third quality from degree.
        static constexpr int majThirds[] = { 1, 0, 0, 1, 1, 0, 0 };
        h.third = (isMinor ? (best == 0 || best == 3 || best == 4) : majThirds[best]) ? 4 : 3;
        h.fifth = 7;
        if (chordCount > 1 || g.chordColor() == ChordColor::Seventh)
            h.seventh = (best == 0 || best == 3) ? 11 : 10;
        return h;
    }
    auto h = progressionHarmony(keyRoot, mode, bar);
    if (g.chordColor() == ChordColor::Seventh)
        h.seventh = 10;
    return h;
}

void renderChords(const GrooveParams& g, double startPpq, double endPpq,
                  const int* chordNotes, int chordCount, int keyRoot, Mode mode,
                  std::vector<Event>& out)
{
    out.clear();
    if (!(endPpq > startPpq) || !g.chordsEnabled) return;
    if (g.chordsInput && (chordNotes == nullptr || chordCount <= 0)) return;

    const int first = std::max(0, (int) std::floor((startPpq - 1.0) / 0.25) - 1);
    const int last = (int) std::ceil((endPpq + 0.25) / 0.25) + 1;
    const std::uint32_t seed = (std::uint32_t) (g.chordVariation * 97 + g.chordNonce * 13);

    for (int index = first; index <= last; ++index)
    {
        const int bar = index / 16;
        const int step = index % 16;
        if (bar < 0) continue;

        bool fire = false;
        double length = 1.2;
        int velocity = 92;
        switch (g.chordRhythm())
        {
            case ChordRhythm::Block:
                fire = step == 0 || step == 8;
                length = 1.6;
                break;
            case ChordRhythm::Pulse:
                fire = step == 0 || step == 4 || step == 8 || step == 12;
                length = 0.7;
                // Seed spin: occasional push on the & of 2.
                if (step == 6 && unit(seed, bar, step, 3) < 0.35f) fire = true;
                break;
            case ChordRhythm::Strum:
                fire = step == 0 || step == 8;
                length = 0.35;
                break;
            case ChordRhythm::Count:
                break;
        }
        if (!fire) continue;

        const Harmony harmony = chordHarmony(g, chordNotes, chordCount, keyRoot, mode, bar);
        std::vector<int> pitches;
        chordVoices(g, harmony, pitches);
        const double onBase = bar * 4.0 + step * 0.25;
        for (size_t i = 0; i < pitches.size(); ++i)
        {
            const double onPpq = g.chordRhythm() == ChordRhythm::Strum
                ? onBase + (double) i * 0.04
                : onBase;
            const double offPpq = onPpq + length;
            const int vel = juce::jlimit(1, 127, velocity - (int) i * 4);
            if (onPpq >= startPpq && onPpq < endPpq)
                out.push_back({ onPpq, pitches[i], vel });
            if (offPpq >= startPpq && offPpq < endPpq)
                out.push_back({ offPpq, pitches[i], 0 });
        }
    }
    sortEvents(out);
}

// ── Bass ─────────────────────────────────────────────────────────────────────

void renderBass(const GrooveParams& g, double startPpq, double endPpq,
                const int* chordNotes, int chordCount, int keyRoot, Mode mode,
                std::vector<Event>& out)
{
    out.clear();
    if (!(endPpq > startPpq) || !g.bassEnabled) return;
    if (g.bassInput && (chordNotes == nullptr || chordCount <= 0)) return;

    const int first = std::max(0, (int) std::floor((startPpq - 1.0) / 0.25) - 1);
    const int last = (int) std::ceil((endPpq + 0.25) / 0.25) + 1;
    const std::uint32_t seed = (std::uint32_t) (g.bassVariation * 97 + g.bassNonce * 13);
    const int bassBias = (int) (mix(seed) >> 9) % 5;
    static constexpr int bassShift[] = { 0, 3, 7, 10, 12 };
    const int preferShift = bassShift[bassBias];
    const bool color = ((mix(seed) >> 20) & 1u) != 0;

    auto emit = [&](int bar, int step, int degree, int prefer, float chance, int vel, double len)
    {
        if (chance < 0.999f && unit(seed, bar, step, degree + 20) >= chance)
            return;
        const Harmony harmony = g.bassInput
            ? harmonyFromHeld(chordNotes, chordCount, keyRoot, mode, bar)
            : progressionHarmony(keyRoot, mode, bar);
        const int note = pitchInRange(toneOf(harmony, degree), 28, 52, prefer + preferShift);
        const double onPpq = bar * 4.0 + step * 0.25;
        const double offPpq = onPpq + len;
        if (onPpq >= startPpq && onPpq < endPpq)
            out.push_back({ onPpq, note, vel });
        if (offPpq >= startPpq && offPpq < endPpq)
            out.push_back({ offPpq, note, 0 });
    };

    for (int index = first; index <= last; ++index)
    {
        const int bar = index / 16;
        const int step = index % 16;
        if (bar < 0) continue;

        switch (g.bassMotion())
        {
            case BassMotion::Half:
                if (step == 0) emit(bar, step, 0, 40, 1.0f, 104, 1.8);
                if (step == 8) emit(bar, step, 2, 40, 0.55f, 90, 1.4);
                break;
            case BassMotion::Shuffle:
                if (step == 0) emit(bar, step, 0, 40, 1.0f, 108, 0.4);
                if (step == 6) emit(bar, step, 2, 43, 1.0f, 96, 0.35);
                if (step == 10) emit(bar, step, 0, 45, 1.0f, 100, 0.4);
                if (step % 2 == 1) emit(bar, step, 1, 42, 0.35f, 72, 0.2);
                break;
            case BassMotion::Sparse:
                if (step == 0) emit(bar, step, 0, 36, 1.0f, 110, 1.2);
                if (step == 8) emit(bar, step, 2, 43, 0.4f, 84, 0.8);
                break;
            case BassMotion::Pulse:
            case BassMotion::Count:
                if (step == 0 || step == 8) emit(bar, step, 0, 40, 1.0f, 106, 0.45);
                if (step == 4 || step == 12) emit(bar, step, 2, 43, 0.5f, 88, 0.35);
                if (step % 2 == 1) emit(bar, step, 0, 40, 0.25f, 70, 0.18);
                break;
        }
        if (color && step == 4)
            emit(bar, step, 2, 43, 1.0f, 92, 0.35);
    }
    sortEvents(out);
}

// ── Drums ────────────────────────────────────────────────────────────────────

struct DrumHit { int note; float chance; int velocity; };

void drumHitsForStep(DrumStyle style, int step, float density, std::uint32_t seed,
                     std::vector<DrumHit>& hits)
{
    hits.clear();
    auto add = [&](int note, float chance, int vel)
    {
        hits.push_back({ note, chance, vel });
    };

    // Anchors stay; density only scales optional hats / ghosts / pickups.
    const float dens = clampf(density / 100.0f, 0.0f, 1.0f);
    switch (style)
    {
        case DrumStyle::HalfTime:
            if (step == 0) { add(36, 1.0f, 116); add(42, 1.0f, 82); }
            if (step == 2) add(42, 0.55f * dens, 64);
            if (step == 4) add(42, 1.0f, 74);
            if (step == 6) { add(36, 0.35f * dens, 86); add(42, 0.4f * dens, 58); }
            if (step == 8) { add(38, 1.0f, 118); add(42, 1.0f, 84); }
            if (step == 10) add(42, 0.5f * dens, 66);
            if (step == 12) add(42, 1.0f, 76);
            if (step == 14) { add(36, 0.3f * dens, 80); add(42, 0.35f * dens, 54); }
            break;
        case DrumStyle::Shuffle:
            if (step == 0) { add(36, 1.0f, 110); add(42, 1.0f, 86); }
            if (step == 3) add(42, 1.0f, 70);
            if (step == 4) { add(38, 1.0f, 112); add(42, 1.0f, 84); }
            if (step == 7) add(42, 1.0f, 68);
            if (step == 8) { add(36, 1.0f, 104); add(42, 1.0f, 82); }
            if (step == 11) add(42, 1.0f, 66);
            if (step == 12) { add(38, 1.0f, 114); add(42, 1.0f, 86); }
            if (step == 15) add(42, 1.0f, 72);
            break;
        case DrumStyle::Sparse:
            if (step == 0) { add(36, 1.0f, 108); add(42, 1.0f, 70); }
            if (step == 4) add(42, 0.45f * dens, 58);
            if (step == 8) { add(36, 0.55f, 96); add(42, 1.0f, 68); }
            if (step == 12) add(38, 1.0f, 110);
            if (step == 14) add(42, 0.3f * dens, 52);
            break;
        case DrumStyle::Straight:
        case DrumStyle::Count:
            if (step == 0) { add(36, 1.0f, 112); add(42, 1.0f, 88); }
            if (step == 1) add(42, 0.45f * dens, 58);
            if (step == 2) add(42, 1.0f, 78);
            if (step == 3) add(42, 0.35f * dens, 52);
            if (step == 4) { add(38, 1.0f, 114); add(42, 1.0f, 88); }
            if (step == 5) add(42, 0.4f * dens, 56);
            if (step == 6) add(42, 1.0f, 76);
            if (step == 7) add(42, 0.3f * dens, 50);
            if (step == 8) { add(36, 1.0f, 108); add(42, 1.0f, 86); }
            if (step == 9) add(42, 0.45f * dens, 58);
            if (step == 10) { add(36, 0.4f * dens, 90); add(42, 1.0f, 78); }
            if (step == 11) add(42, 0.35f * dens, 52);
            if (step == 12) { add(38, 1.0f, 116); add(42, 1.0f, 90); }
            if (step == 13) add(42, 0.4f * dens, 56);
            if (step == 14) add(42, 1.0f, 80);
            if (step == 15) add(46, 0.25f * dens, 84);
            break;
    }
    // Seed spin: occasional extra kick / open hat.
    const int extraKick = (int) ((mix(seed) >> 3) % 5);
    static constexpr int kickSteps[] = { 6, 10, 14, 7, -1 };
    if (kickSteps[extraKick] == step)
        add(36, 0.55f * dens, 90);
}

std::vector<RollNote> eventsToRollNotes(const std::vector<Event>& events, int channel)
{
    struct Open { double onPpq = 0.0; int velocity = 100; int id = 0; };
    std::map<int, std::vector<Open>> openByPitch;
    std::vector<RollNote> notes;
    int nextId = 1;
    for (const auto& ev : events)
    {
        if (ev.velocity > 0)
        {
            openByPitch[ev.note].push_back({ ev.ppq, ev.velocity, nextId++ });
            continue;
        }
        auto it = openByPitch.find(ev.note);
        if (it == openByPitch.end() || it->second.empty())
            continue;
        Open on = it->second.front();
        it->second.erase(it->second.begin());
        RollNote n;
        n.id = on.id;
        n.pitch = juce::jlimit(0, 127, ev.note);
        n.start = on.onPpq * 4.0;
        n.len = juce::jmax(0.25, (ev.ppq - on.onPpq) * 4.0);
        n.fileVelocity = juce::jlimit(1, 127, on.velocity);
        n.channel = juce::jlimit(1, 16, channel);
        notes.push_back(n);
    }
    // Close any hanging notes at a 16th.
    for (auto& [pitch, opens] : openByPitch)
    {
        for (const auto& on : opens)
        {
            RollNote n;
            n.id = on.id;
            n.pitch = juce::jlimit(0, 127, pitch);
            n.start = on.onPpq * 4.0;
            n.len = 1.0;
            n.fileVelocity = juce::jlimit(1, 127, on.velocity);
            n.channel = juce::jlimit(1, 16, channel);
            notes.push_back(n);
        }
    }
    std::sort(notes.begin(), notes.end(), [](const RollNote& a, const RollNote& b)
    {
        if (a.start != b.start) return a.start < b.start;
        return a.pitch < b.pitch;
    });
    return notes;
}

StepClip renderPartClip(Part part, const GrooveParams& src, int keyRoot, Mode mode,
                        const int* chordNotes, int chordCount)
{
    GrooveParams g = src;
    g.arpEnabled = part == Part::Arp;
    g.melodyEnabled = part == Part::Melody;
    g.chordsEnabled = part == Part::Chords;
    g.bassEnabled = part == Part::Bass;
    g.drumsEnabled = part == Part::Drums;
    // Offline Generate always writes a pattern. Live "Input" waiting for held
    // notes must not leave the piano roll empty when nothing is held.
    if (chordNotes == nullptr || chordCount <= 0)
    {
        g.arpInput = false;
        g.melodyInput = false;
        g.chordsInput = false;
        g.bassInput = false;
    }

    int bars = 4;
    if (part == Part::Melody)
    {
        switch (g.melodyPhrase())
        {
            case MelodyPhrase::OneBar:    bars = 1; break;
            case MelodyPhrase::TwoBars:   bars = 2; break;
            case MelodyPhrase::FourBars:  bars = 4; break;
            case MelodyPhrase::EightBars: bars = 8; break;
            case MelodyPhrase::Count:     break;
        }
    }
    else if (part == Part::Drums || part == Part::Bass)
        bars = 2;
    else if (part == Part::Arp)
        bars = 2;

    const double endPpq = (double) bars * 4.0;
    std::vector<Event> events;
    int channel = 1;
    const char* name = "Generator";
    ClipKind kind = ClipKind::Lead;
    switch (part)
    {
        case Part::Arp:
            gen::renderArp(g, 0.0, endPpq, chordNotes, chordCount, keyRoot, mode, events);
            name = "Arpeggio";
            kind = ClipKind::Lead;
            break;
        case Part::Melody:
            gen::renderMelody(g, 0.0, endPpq, chordNotes, chordCount, keyRoot, mode, events);
            name = "Melody";
            kind = ClipKind::Lead;
            break;
        case Part::Chords:
            gen::renderChords(g, 0.0, endPpq, chordNotes, chordCount, keyRoot, mode, events);
            name = "Chords";
            kind = ClipKind::Piano;
            break;
        case Part::Bass:
            gen::renderBass(g, 0.0, endPpq, chordNotes, chordCount, keyRoot, mode, events);
            name = "Bass";
            kind = ClipKind::Bass;
            break;
        case Part::Drums:
            gen::renderDrums(g, 0.0, endPpq, events);
            name = "Drums";
            kind = ClipKind::Drums;
            channel = 10;
            break;
        case Part::Count:
            break;
    }

    StepClip clip;
    clip.name = name;
    clip.filePath = "generator://" + juce::String(name).toLowerCase();
    clip.kind = kind;
    clip.hasDrumContent = part == Part::Drums;
    clip.root = juce::jlimit(0, 11, keyRoot);
    clip.bars = bars;
    clip.notes = eventsToRollNotes(events, channel);
    clip.noteCount = (int) clip.notes.size();
    clip.notesResident = true;
    std::set<int> pitches;
    for (const auto& n : clip.notes)
        pitches.insert(n.pitch);
    clip.difNotes = (int) pitches.size();
    clip.complexity = juce::jlimit(1, 100, clip.noteCount);
    return clip;
}

void renderDrums(const GrooveParams& g, double startPpq, double endPpq, std::vector<Event>& out)
{
    out.clear();
    if (!(endPpq > startPpq) || !g.drumsEnabled) return;

    const int first = std::max(0, (int) std::floor((startPpq - 1.0) / 0.25) - 1);
    const int last = (int) std::ceil((endPpq + 0.25) / 0.25) + 1;
    const std::uint32_t seed = (std::uint32_t) (g.drumVariation * 97 + g.drumNonce * 13);
    std::vector<DrumHit> hits;

    for (int index = first; index <= last; ++index)
    {
        const int bar = index / 16;
        const int step = index % 16;
        if (bar < 0) continue;
        drumHitsForStep(g.drumStyle(), step, (float) g.drumDensity, seed, hits);
        const double onPpq = bar * 4.0 + step * 0.25;
        for (const auto& hit : hits)
        {
            if (hit.chance < 0.999f && unit(seed, bar, step, hit.note) >= hit.chance)
                continue;
            if (onPpq >= startPpq && onPpq < endPpq)
                out.push_back({ onPpq, hit.note, hit.velocity });
            const double offPpq = onPpq + 0.2;
            if (offPpq >= startPpq && offPpq < endPpq)
                out.push_back({ offPpq, hit.note, 0 });
        }
    }
    sortEvents(out);
}

} // namespace gen
} // namespace pflow
