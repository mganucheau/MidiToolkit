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

/** Resolve Auto → a concrete form from the seed. */
GeneratorForm resolveForm(GeneratorForm form, std::uint32_t seed)
{
    if (form != GeneratorForm::Auto)
        return form;
    static constexpr GeneratorForm kChoices[] = {
        GeneratorForm::AAAB, GeneratorForm::ABAB, GeneratorForm::ABAC,
        GeneratorForm::ABCA, GeneratorForm::AABA
    };
    return kChoices[(mix(seed) >> 4) % 5];
}

/** Four section letters (0=A,1=B,2=C) covering the clip. */
void formLetters(GeneratorForm form, int out[4])
{
    switch (form)
    {
        case GeneratorForm::AAAB: out[0]=0; out[1]=0; out[2]=0; out[3]=1; break;
        case GeneratorForm::ABAB: out[0]=0; out[1]=1; out[2]=0; out[3]=1; break;
        case GeneratorForm::ABAC: out[0]=0; out[1]=1; out[2]=0; out[3]=2; break;
        case GeneratorForm::ABCA: out[0]=0; out[1]=1; out[2]=2; out[3]=0; break;
        case GeneratorForm::AABA: out[0]=0; out[1]=0; out[2]=1; out[3]=0; break;
        case GeneratorForm::Auto:
        case GeneratorForm::Count:
            out[0]=0; out[1]=0; out[2]=0; out[3]=1; break;
    }
}

/** Map absolute bar → (motif letter, bar within motif cell). */
void mapFormBar(int totalBars, GeneratorForm form, int absBar, int& letter, int& localBar)
{
    const int bars = juce::jmax(1, totalBars);
    const int sectionBars = juce::jmax(1, bars / 4);
    int letters[4];
    formLetters(form, letters);
    const int section = juce::jlimit(0, 3, absBar / sectionBars);
    letter = letters[section];
    localBar = absBar % sectionBars;
}

/** True when [start,end) covers a full offline phrase (not a live audio block). */
bool isFullPhraseWindow(double startPpq, double endPpq, int phraseBars)
{
    const double phraseBeats = (double) juce::jmax(1, phraseBars) * 4.0;
    return startPpq <= 0.01 && endPpq + 0.01 >= phraseBeats;
}

int wrapBar(int absBar, int phraseBars)
{
    const int n = juce::jmax(1, phraseBars);
    int b = absBar % n;
    if (b < 0) b += n;
    return b;
}

/** Soft musical QA: anchor downbeats, thin overcrowded bars, close phrases. */
void musicalQaPass(std::vector<Event>& events, int bars, bool drums)
{
    if (events.empty() || bars <= 0) return;
    sortEvents(events);

    // Ensure each bar has an anchor on beat 1 when possible.
    for (int bar = 0; bar < bars; ++bar)
    {
        const double beat1 = bar * 4.0;
        bool hasAnchor = false;
        for (const auto& e : events)
        {
            if (e.velocity <= 0) continue;
            if (e.ppq >= beat1 - 0.01 && e.ppq < beat1 + 0.3)
            {
                hasAnchor = true;
                break;
            }
        }
        if (hasAnchor) continue;
        // Nudge nearest note-on in the bar onto beat 1.
        Event* nearest = nullptr;
        double best = 1.0e9;
        for (auto& e : events)
        {
            if (e.velocity <= 0) continue;
            if (e.ppq < beat1 || e.ppq >= beat1 + 4.0) continue;
            const double d = std::abs(e.ppq - beat1);
            if (d < best) { best = d; nearest = &e; }
        }
        if (nearest != nullptr && best < 1.1)
            nearest->ppq = beat1;
        else if (drums)
        {
            events.push_back({ beat1, 36, 108 });
            events.push_back({ beat1 + 0.2, 36, 0 });
        }
    }

    // Thin bars that are denser than ~14 note-ons (non-drums) / 22 (drums).
    const int cap = drums ? 22 : 14;
    for (int bar = 0; bar < bars; ++bar)
    {
        const double lo = bar * 4.0;
        const double hi = lo + 4.0;
        std::vector<Event*> ons;
        for (auto& e : events)
            if (e.velocity > 0 && e.ppq >= lo && e.ppq < hi)
                ons.push_back(&e);
        if ((int) ons.size() <= cap) continue;
        // Drop weakest off-beat notes first.
        std::sort(ons.begin(), ons.end(), [](const Event* a, const Event* b)
        {
            const bool aStrong = std::fmod(a->ppq, 1.0) < 0.01;
            const bool bStrong = std::fmod(b->ppq, 1.0) < 0.01;
            if (aStrong != bStrong) return !aStrong && bStrong;
            return a->velocity < b->velocity;
        });
        const int drop = (int) ons.size() - cap;
        for (int i = 0; i < drop; ++i)
            ons[(size_t) i]->velocity = 0;
    }

    // Drop thinned note-ons (velocity forced to 0) and orphaned offs for those pitches/times.
    std::vector<Event> cleaned;
    cleaned.reserve(events.size());
    for (const auto& e : events)
    {
        if (e.velocity > 0)
            cleaned.push_back(e);
        else
        {
            // Keep offs only when a surviving on exists for this pitch at/before this time.
            bool keep = false;
            for (const auto& on : cleaned)
                if (on.velocity > 0 && on.note == e.note && on.ppq <= e.ppq + 1.0e-6)
                    keep = true;
            if (keep) cleaned.push_back(e);
        }
    }
    events.swap(cleaned);
    sortEvents(events);
}

void shiftEvents(std::vector<Event>& events, double deltaPpq)
{
    for (auto& e : events)
        e.ppq += deltaPpq;
}

void mutateMotif(std::vector<Event>& motif, std::uint32_t seed, int salt)
{
    // Light variation: nudge some velocities and occasionally drop/add a weak note.
    for (auto& e : motif)
    {
        if (e.velocity <= 0) continue;
        const float r = unit(seed, (int) (e.ppq * 4.0), e.note, salt);
        if (r < 0.12f)
            e.velocity = juce::jlimit(1, 127, e.velocity - 12);
        else if (r > 0.88f)
            e.velocity = juce::jlimit(1, 127, e.velocity + 10);
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

// ── Arp (Sting-inspired pattern engine) ──────────────────────────────────────
// Generates related 16-step note / velocity / gate / octave banks, then tiles
// them across the clip with song-form sections — not a classic up/down arp.

struct ArpBanks
{
    int noteIdx[16];   // index into pitch pool
    int velLayer[16];  // 0=rest, 1=soft, 2=mid, 3=accent
    float gateMul[16]; // duration scale
    int octShift[16];  // -1 / 0 / +1
};

void buildPitchPool(const GrooveParams& g, const int* chordNotes, int chordCount,
                    int keyRoot, Mode mode, std::vector<int>& pool)
{
    pool.clear();
    if (chordNotes != nullptr)
    {
        for (int i = 0; i < chordCount; ++i)
        {
            const int note = chordNotes[i];
            if (note < 0 || note > 127) continue;
            if (std::find(pool.begin(), pool.end(), note) == pool.end())
                pool.push_back(note);
        }
    }
    if (pool.empty())
    {
        const int root = 60 + juce::jlimit(0, 11, keyRoot);
        int count = 0;
        const int* deg = scaleDegrees(mode, count);
        pool.push_back(root);
        pool.push_back(root + deg[2]);
        pool.push_back(root + deg[4]);
        if (count > 5) pool.push_back(root + deg[5]);
    }
    std::sort(pool.begin(), pool.end());
    const int octs = juce::jlimit(1, 3, g.arpOctaves);
    const auto base = pool;
    for (int o = 1; o < octs; ++o)
        for (int n : base)
            if (n + o * 12 <= 127)
                pool.push_back(n + o * 12);
}

void generateArpBanks(const GrooveParams& g, std::uint32_t seed, int salt, ArpBanks& banks)
{
    const float density = clampf((float) g.arpDensity / 100.0f, 0.05f, 1.0f);
    // Shape index acts like Sting Type: low = more random intervals, high = chord tones.
    const float type = (float) g.arpShapeIndex / (float) juce::jmax(1, (int) ArpShape::Count - 1);

    for (int i = 0; i < 16; ++i)
    {
        const float r = unit(seed, i, salt, 3);
        const bool strong = (i % 4) == 0;
        const bool play = strong ? (r < density * 1.15f + 0.15f) : (r < density);
        if (!play)
        {
            banks.noteIdx[i] = 0;
            banks.velLayer[i] = 0;
            banks.gateMul[i] = 0.5f;
            banks.octShift[i] = 0;
            continue;
        }

        // Prefer chord-tone indices when type is high.
        const float pick = unit(seed, i, salt, 9);
        int idx;
        if (pick < type * 0.75f + 0.15f)
            idx = (i / 2 + (int) (unit(seed, i, salt, 11) * 3.0f)) % 4;
        else
            idx = (int) (unit(seed, i, salt, 13) * 8.0f);
        banks.noteIdx[i] = idx;

        const float accentRoll = unit(seed, i, salt, 17);
        if (strong || accentRoll > 0.72f) banks.velLayer[i] = 3;
        else if (accentRoll > 0.35f)      banks.velLayer[i] = 2;
        else                              banks.velLayer[i] = 1;

        const float gRoll = unit(seed, i, salt, 19);
        banks.gateMul[i] = gRoll < 0.33f ? 0.35f : (gRoll < 0.7f ? 0.65f : 1.05f);

        const float oRoll = unit(seed, i, salt, 23);
        banks.octShift[i] = oRoll < 0.18f ? -1 : (oRoll > 0.82f ? 1 : 0);
    }
    // Always land something on step 0.
    if (banks.velLayer[0] == 0)
    {
        banks.velLayer[0] = 3;
        banks.noteIdx[0] = 0;
        banks.gateMul[0] = 0.8f;
        banks.octShift[0] = 0;
    }
}

void renderArpBanksBar(const GrooveParams& g, const ArpBanks& banks,
                       const std::vector<int>& pool, int bar,
                       double startPpq, double endPpq, std::vector<Event>& out)
{
    if (pool.empty()) return;
    const double stepLength = delayPeriodBeats(g.arpRate());
    const float gateScale = clampf(g.arpGate / 100.0f, 0.08f, 1.2f);
    const float intensity = clampf((float) g.arpIntensity / 100.0f, 0.15f, 1.0f);
    const float dynamics = clampf((float) g.arpDynamics / 100.0f, 0.0f, 1.0f);
    const int velLo = juce::roundToInt(48.0f + 20.0f * intensity);
    const int velMid = juce::roundToInt(72.0f + 28.0f * intensity);
    const int velHi = juce::roundToInt(96.0f + 28.0f * intensity);

    const int stepsPerBar = juce::jmax(1, (int) std::lround(4.0 / stepLength));
    for (int s = 0; s < stepsPerBar; ++s)
    {
        const int patternStep = s % 16;
        const int layer = banks.velLayer[patternStep];
        if (layer <= 0) continue;

        int flat = juce::jlimit(1, 127, (velLo + velMid + velHi) / 3);
        int layered = layer == 1 ? velLo : (layer == 2 ? velMid : velHi);
        const int velocity = juce::jlimit(1, 127,
            juce::roundToInt((float) flat * (1.0f - dynamics) + (float) layered * dynamics));

        const int idx = ((banks.noteIdx[patternStep] % (int) pool.size()) + (int) pool.size())
                        % (int) pool.size();
        int note = pool[(size_t) idx] + banks.octShift[patternStep] * 12;
        note = juce::jlimit(0, 127, note);

        const double onPpq = bar * 4.0 + s * stepLength;
        const double offPpq = onPpq + stepLength * (double) (banks.gateMul[patternStep] * gateScale);
        if (onPpq >= startPpq && onPpq < endPpq)
            out.push_back({ onPpq, note, velocity });
        if (offPpq >= startPpq && offPpq < endPpq)
            out.push_back({ offPpq, note, 0 });
    }
}

void renderArp(const GrooveParams& g, double startPpq, double endPpq,
               const int* chordNotes, int chordCount, int keyRoot, Mode mode,
               std::vector<Event>& out)
{
    out.clear();
    if (!(endPpq > startPpq) || !g.arpEnabled) return;
    if (g.arpInput && (chordNotes == nullptr || chordCount <= 0)) return;

    const std::uint32_t seed = (std::uint32_t) (g.arpVariation * 97 + g.arpNonce * 13);
    std::vector<int> pool;
    buildPitchPool(g, chordNotes, chordCount, keyRoot, mode, pool);
    if (pool.empty()) return;

    ArpBanks bankA {}, bankB {}, bankC {};
    generateArpBanks(g, seed, 1, bankA);
    generateArpBanks(g, seed, 2, bankB);
    generateArpBanks(g, seed, 3, bankC);
    // B/C stay related: copy A then lightly re-roll density holes.
    for (int i = 0; i < 16; ++i)
    {
        if (unit(seed, i, 40, 1) < 0.55f) bankB.noteIdx[i] = bankA.noteIdx[i];
        if (unit(seed, i, 41, 1) < 0.55f) bankC.noteIdx[i] = bankA.noteIdx[i];
    }

    const int phraseBars = generatorLengthBars(g.arpLength());
    const GeneratorForm form = resolveForm(g.arpForm(), seed);
    const ArpBanks* banks[3] = { &bankA, &bankB, &bankC };
    const int firstBar = (int) std::floor(startPpq / 4.0) - 1;
    const int lastBar = (int) std::floor((endPpq - 1.0e-9) / 4.0) + 1;

    for (int absBar = firstBar; absBar <= lastBar; ++absBar)
    {
        if (absBar < 0) continue;
        int letter = 0, local = 0;
        mapFormBar(phraseBars, form, wrapBar(absBar, phraseBars), letter, local);
        juce::ignoreUnused(local);
        renderArpBanksBar(g, *banks[juce::jlimit(0, 2, letter)], pool, absBar, startPpq, endPpq, out);
    }
    if (isFullPhraseWindow(startPpq, endPpq, phraseBars))
        musicalQaPass(out, phraseBars, false);
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

    const int phraseBars = generatorLengthBars(g.melodyLength());
    const std::uint32_t seed = (std::uint32_t) (g.melodyVariation * 97 + g.melodyNonce * 13);
    const GeneratorForm form = resolveForm(g.melodyForm(), seed);
    const int sectionBars = juce::jmax(1, phraseBars / 4);
    const int first = std::max(0, (int) std::floor((startPpq - 1.0) / 0.25) - 1);
    const int last = (int) std::ceil((endPpq + 0.25) / 0.25) + 1;

    for (int index = first; index <= last; ++index)
    {
        const int bar = index / 16;
        const int step = index % 16;
        if (bar < 0) continue;

        int letter = 0, localBar = 0;
        mapFormBar(phraseBars, form, wrapBar(bar, phraseBars), letter, localBar);
        GrooveParams motif = g;
        motif.melodyVariation = juce::jlimit(1, 16, g.melodyVariation + letter);
        const int phraseBar = localBar % juce::jmax(1, sectionBars);
        if (!melodyStepPlays(motif, phraseBar, step)) continue;

        const Harmony harmony = g.melodyInput
            ? harmonyFromHeld(chordNotes, chordCount, keyRoot, mode, bar)
            : progressionHarmony(keyRoot, mode, bar);
        const int note = melodyNote(motif, harmony, phraseBar, step, keyRoot, mode);
        const bool downbeat = step == 0 || step == 8;
        const double onPpq = bar * 4.0 + step * 0.25;
        const double length = downbeat ? 0.7 : 0.32;
        const int velocity = juce::jlimit(1, 127, (downbeat ? 104 : 82) + letter * 2);
        if (onPpq >= startPpq && onPpq < endPpq)
            out.push_back({ onPpq, note, velocity });
        const double offPpq = onPpq + length;
        if (offPpq >= startPpq && offPpq < endPpq)
            out.push_back({ offPpq, note, 0 });
    }
    if (isFullPhraseWindow(startPpq, endPpq, phraseBars))
        musicalQaPass(out, phraseBars, false);
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

    const int phraseBars = generatorLengthBars(g.chordLength());
    const int first = std::max(0, (int) std::floor((startPpq - 1.0) / 0.25) - 1);
    const int last = (int) std::ceil((endPpq + 0.25) / 0.25) + 1;
    const std::uint32_t seed = (std::uint32_t) (g.chordVariation * 97 + g.chordNonce * 13);
    const GeneratorForm form = resolveForm(g.chordForm(), seed);

    for (int index = first; index <= last; ++index)
    {
        const int bar = index / 16;
        const int step = index % 16;
        if (bar < 0) continue;

        int letter = 0, localBar = 0;
        mapFormBar(phraseBars, form, wrapBar(bar, phraseBars), letter, localBar);
        juce::ignoreUnused(localBar);
        const std::uint32_t motifSeed = seed ^ (std::uint32_t) (letter * 31u);

        bool fire = false;
        double length = 1.2;
        int velocity = 92 + letter * 3;
        switch (g.chordRhythm())
        {
            case ChordRhythm::Block:
                fire = step == 0 || step == 8;
                length = 1.6;
                // B/C sections add a mid-bar hit for contrast.
                if (letter > 0 && step == 4 && unit(motifSeed, bar, step, 2) < 0.55f)
                    fire = true;
                break;
            case ChordRhythm::Pulse:
                fire = step == 0 || step == 4 || step == 8 || step == 12;
                length = 0.7;
                if (step == 6 && unit(motifSeed, bar, step, 3) < (0.35f + 0.1f * (float) letter))
                    fire = true;
                break;
            case ChordRhythm::Strum:
                fire = step == 0 || step == 8;
                length = 0.35;
                if (letter == 2 && step == 12) fire = true;
                break;
            case ChordRhythm::Count:
                break;
        }
        if (!fire) continue;

        const Harmony harmony = chordHarmony(g, chordNotes, chordCount, keyRoot, mode, bar + letter);
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
    if (isFullPhraseWindow(startPpq, endPpq, phraseBars))
        musicalQaPass(out, phraseBars, false);
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

    const int phraseBars = generatorLengthBars(g.bassLength());
    const int first = std::max(0, (int) std::floor((startPpq - 1.0) / 0.25) - 1);
    const int last = (int) std::ceil((endPpq + 0.25) / 0.25) + 1;
    const std::uint32_t seed = (std::uint32_t) (g.bassVariation * 97 + g.bassNonce * 13);
    const GeneratorForm form = resolveForm(g.bassForm(), seed);
    const int bassBias = (int) (mix(seed) >> 9) % 5;
    static constexpr int bassShift[] = { 0, 3, 7, 10, 12 };
    const int preferShift = bassShift[bassBias];
    const bool color = ((mix(seed) >> 20) & 1u) != 0;

    auto emit = [&](int bar, int step, int letter, int degree, int prefer, float chance, int vel, double len)
    {
        const std::uint32_t motifSeed = seed ^ (std::uint32_t) (letter * 29u);
        if (chance < 0.999f && unit(motifSeed, bar, step, degree + 20) >= chance)
            return;
        const Harmony harmony = g.bassInput
            ? harmonyFromHeld(chordNotes, chordCount, keyRoot, mode, bar)
            : progressionHarmony(keyRoot, mode, bar + letter);
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

        int letter = 0, localBar = 0;
        mapFormBar(phraseBars, form, wrapBar(bar, phraseBars), letter, localBar);
        juce::ignoreUnused(localBar);
        const float densBoost = letter == 0 ? 0.0f : (letter == 1 ? 0.12f : 0.2f);

        switch (g.bassMotion())
        {
            case BassMotion::Half:
                if (step == 0) emit(bar, step, letter, 0, 40, 1.0f, 104, 1.8);
                if (step == 8) emit(bar, step, letter, 2, 40, 0.55f + densBoost, 90, 1.4);
                break;
            case BassMotion::Shuffle:
                if (step == 0) emit(bar, step, letter, 0, 40, 1.0f, 108, 0.4);
                if (step == 6) emit(bar, step, letter, 2, 43, 1.0f, 96, 0.35);
                if (step == 10) emit(bar, step, letter, 0, 45, 1.0f, 100, 0.4);
                if (step % 2 == 1) emit(bar, step, letter, 1, 42, 0.35f + densBoost, 72, 0.2);
                break;
            case BassMotion::Sparse:
                if (step == 0) emit(bar, step, letter, 0, 36, 1.0f, 110, 1.2);
                if (step == 8) emit(bar, step, letter, 2, 43, 0.4f + densBoost, 84, 0.8);
                break;
            case BassMotion::Pulse:
            case BassMotion::Count:
                if (step == 0 || step == 8) emit(bar, step, letter, 0, 40, 1.0f, 106, 0.45);
                if (step == 4 || step == 12) emit(bar, step, letter, 2, 43, 0.5f + densBoost, 88, 0.35);
                if (step % 2 == 1) emit(bar, step, letter, 0, 40, 0.25f + densBoost * 0.5f, 70, 0.18);
                break;
        }
        if ((color || letter > 0) && step == 4)
            emit(bar, step, letter, 2, 43, 1.0f, 92, 0.35);
    }
    if (isFullPhraseWindow(startPpq, endPpq, phraseBars))
        musicalQaPass(out, phraseBars, false);
    sortEvents(out);
}

// ── Drums (genre styles) ─────────────────────────────────────────────────────
// GM: 36 kick, 38 snare, 37 side stick, 39 clap, 42 closed hat, 46 open hat,
// 43 low tom, 47 mid tom, 50 high tom.

struct DrumHit { int note; float chance; int velocity; };

void drumHitsForStep(DrumStyle style, int step, float density, float fill,
                     bool fillBar, std::uint32_t seed, std::vector<DrumHit>& hits)
{
    hits.clear();
    auto add = [&](int note, float chance, int vel)
    {
        hits.push_back({ note, chance, vel });
    };

    const float dens = clampf(density / 100.0f, 0.0f, 1.0f);
    const float fillAmt = clampf(fill / 100.0f, 0.0f, 1.0f);

    if (fillBar && fillAmt > 0.05f)
    {
        // End-of-phrase fill: denser snare/tom work in the second half.
        if (step >= 8)
        {
            if (step == 8 || step == 10 || step == 12 || step == 14)
                add(38, 0.55f + 0.4f * fillAmt, 100 + (int) (20 * fillAmt));
            if (step == 9 || step == 11 || step == 13 || step == 15)
                add(step >= 13 ? 50 : 47, 0.35f * fillAmt + 0.25f, 88);
            if (step == 15) add(49, 0.55f * fillAmt, 110); // crash
            if (step == 8) add(36, 1.0f, 112);
            return;
        }
    }

    switch (style)
    {
        case DrumStyle::House:
            // Four-on-the-floor, clap on 2&4, offbeat open hats.
            if (step == 0 || step == 4 || step == 8 || step == 12) add(36, 1.0f, 114);
            if (step == 4 || step == 12) add(39, 1.0f, 108); // clap
            if ((step % 2) == 0) add(42, 0.85f + 0.15f * dens, 78);
            if ((step % 2) == 1) add(46, 0.35f * dens, 70);
            if (step == 14) add(36, 0.4f * dens, 90);
            break;

        case DrumStyle::Breakbeat:
            // Amen-ish: syncopated kick + snare on 2 and the & of 3.
            if (step == 0) { add(36, 1.0f, 116); add(42, 1.0f, 80); }
            if (step == 2) add(42, 0.7f * dens, 64);
            if (step == 4) { add(38, 1.0f, 118); add(42, 1.0f, 82); }
            if (step == 5) add(36, 0.55f * dens, 92);
            if (step == 6) add(42, 0.6f * dens, 70);
            if (step == 7) add(38, 0.35f * dens, 86);
            if (step == 8) { add(36, 0.7f, 104); add(42, 1.0f, 78); }
            if (step == 10) { add(38, 1.0f, 112); add(42, 0.8f, 74); }
            if (step == 11) add(36, 0.45f * dens, 88);
            if (step == 12) add(42, 1.0f, 80);
            if (step == 13) add(38, 0.4f * dens, 90);
            if (step == 14) { add(36, 0.5f * dens, 96); add(42, 0.7f, 72); }
            if (step == 15) add(46, 0.45f * dens, 84);
            break;

        case DrumStyle::Dub:
            // Sparse kick, snare on 3, space, rim / stick color.
            if (step == 0) add(36, 1.0f, 118);
            if (step == 4) add(42, 0.4f * dens, 58);
            if (step == 8) { add(38, 1.0f, 112); add(37, 0.55f * dens, 78); }
            if (step == 12) add(42, 0.35f * dens, 56);
            if (step == 14) add(36, 0.3f * dens, 86);
            if (step == 6) add(37, 0.25f * dens, 70);
            break;

        case DrumStyle::Techno:
            // Driving 16ths, solid kick, occasional offbeat kick.
            if (step == 0 || step == 4 || step == 8 || step == 12) add(36, 1.0f, 116);
            add(42, 0.75f + 0.2f * dens, (step % 2) == 0 ? 82 : 64);
            if (step == 6 || step == 14) add(36, 0.45f * dens, 94);
            if (step == 4 || step == 12) add(39, 0.7f, 100);
            if (step == 15) add(46, 0.35f * dens, 88);
            break;

        case DrumStyle::Funk:
            // Syncopated kick, ghost snares, swung hats.
            if (step == 0) add(36, 1.0f, 112);
            if (step == 3) add(36, 0.65f * dens, 96);
            if (step == 4) { add(38, 1.0f, 114); add(42, 1.0f, 78); }
            if (step == 5) add(38, 0.4f * dens, 58); // ghost
            if (step == 7) add(38, 0.3f * dens, 52);
            if (step == 8) { add(36, 0.55f, 100); add(42, 1.0f, 76); }
            if (step == 10) add(36, 0.7f * dens, 104);
            if (step == 12) { add(38, 1.0f, 116); add(42, 1.0f, 80); }
            if (step == 13) add(38, 0.35f * dens, 56);
            if ((step % 2) == 0) add(42, 0.85f, 74);
            if ((step % 2) == 1) add(42, 0.45f * dens, 58);
            break;

        case DrumStyle::Count:
            break;
    }

    juce::ignoreUnused(seed);
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

void renderDrums(const GrooveParams& g, double startPpq, double endPpq, std::vector<Event>& out)
{
    out.clear();
    if (!(endPpq > startPpq) || !g.drumsEnabled) return;

    const int phraseBars = generatorLengthBars(g.drumLength());
    const std::uint32_t seed = (std::uint32_t) (g.drumVariation * 97 + g.drumNonce * 13);
    const GeneratorForm form = resolveForm(g.drumForm(), seed);
    std::vector<DrumHit> hits;
    const int firstBar = (int) std::floor(startPpq / 4.0) - 1;
    const int lastBar = (int) std::floor((endPpq - 1.0e-9) / 4.0) + 1;
    const int sectionBars = juce::jmax(1, phraseBars / 4);

    for (int absBar = firstBar; absBar <= lastBar; ++absBar)
    {
        if (absBar < 0) continue;
        int letter = 0, local = 0;
        const int phraseBar = wrapBar(absBar, phraseBars);
        mapFormBar(phraseBars, form, phraseBar, letter, local);
        // Fill on the last bar of each form cell (and the last bar of the phrase).
        const bool fillBar = (local == sectionBars - 1) || (phraseBar == phraseBars - 1);
        // Letter B/C get a slightly denser/snappier density bias.
        const float dens = (float) g.drumDensity + (letter == 1 ? 8.0f : (letter == 2 ? 12.0f : 0.0f));

        for (int step = 0; step < 16; ++step)
        {
            drumHitsForStep(g.drumStyle(), step, dens, (float) g.drumFill, fillBar, seed ^ (std::uint32_t) letter, hits);
            const double onPpq = absBar * 4.0 + step * 0.25;
            for (const auto& hit : hits)
            {
                if (hit.chance < 0.999f && unit(seed, absBar, step, hit.note + letter * 17) >= hit.chance)
                    continue;
                if (onPpq >= startPpq && onPpq < endPpq)
                    out.push_back({ onPpq, hit.note, hit.velocity });
                const double offPpq = onPpq + 0.2;
                if (offPpq >= startPpq && offPpq < endPpq)
                    out.push_back({ offPpq, hit.note, 0 });
            }
        }
    }
    if (isFullPhraseWindow(startPpq, endPpq, phraseBars))
        musicalQaPass(out, phraseBars, true);
    sortEvents(out);
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
    switch (part)
    {
        case Part::Arp:    bars = generatorLengthBars(g.arpLength()); break;
        case Part::Melody: bars = generatorLengthBars(g.melodyLength()); break;
        case Part::Chords: bars = generatorLengthBars(g.chordLength()); break;
        case Part::Bass:   bars = generatorLengthBars(g.bassLength()); break;
        case Part::Drums:  bars = generatorLengthBars(g.drumLength()); break;
        case Part::Count:  break;
    }
    // Melody phrase length follows form cell size for long clips, else full length.
    if (part == Part::Melody)
    {
        const int cell = bars >= 8 ? juce::jmax(1, bars / 4) : bars;
        switch (cell)
        {
            case 1:  g.melodyPhraseIndex = (int) MelodyPhrase::OneBar; break;
            case 2:  g.melodyPhraseIndex = (int) MelodyPhrase::TwoBars; break;
            case 8:  g.melodyPhraseIndex = (int) MelodyPhrase::EightBars; break;
            case 16: g.melodyPhraseIndex = (int) MelodyPhrase::SixteenBars; break;
            default: g.melodyPhraseIndex = (int) MelodyPhrase::FourBars; break;
        }
    }

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
            kind = ClipKind::Keys;
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

} // namespace gen
} // namespace pflow
