#include "GeneratorEngines.h"
#include "GenrePacks.h"
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

/** Strong scramble so consecutive Generate presses land far apart in pattern space. */
std::uint32_t partSeed(int variation, int nonce, int genreIndex, int salt = 0)
{
    return mix((std::uint32_t) variation * 0x9E3779B9u
               ^ (std::uint32_t) nonce * 0x85EBCA6Bu
               ^ (std::uint32_t) (genreIndex + 1) * 0xC2B2AE35u
               ^ (std::uint32_t) salt * 0x27D4EB2Du
               ^ 0xA5A5A5A5u);
}

/** Genre feel for non-drum parts: density / syncopation bias. */
struct GenreFeel
{
    float densityMul = 1.0f;
    float syncopation = 0.35f;
    float sustainBias = 0.0f;
    bool preferOffbeats = false;
};

GenreFeel genreFeel(GeneratorGenre g)
{
    const auto& pack = genrePack(g);
    return { pack.densityMul, pack.syncopation, pack.sustainBias, pack.preferOffbeats };
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

// ── Arp (classic grid arpeggiator) ───────────────────────────────────────────
// Sting model: Division (Speed) clocks an even grid; Density picks how many of
// the 16 pattern steps sound (scale 0..1 → 1..16), spaced evenly — never random
// rests. Type walks the chord/scale pool in order (Up / Skip / Pedal / …).

constexpr float kArpGate = 0.70f;
constexpr int kArpPatternSteps = 16;

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
        // Acid lives a register lower (C3) so octave jumps stay in synth-bass range.
        const int root = (g.arpShape() == ArpShape::Acid ? 48 : 60) + juce::jlimit(0, 11, keyRoot);
        int count = 0;
        const int* deg = scaleDegrees(mode, count);
        pool.push_back(root);
        pool.push_back(root + deg[juce::jmin(2, count - 1)]);
        pool.push_back(root + deg[juce::jmin(4, count - 1)]);
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

/** Ordered pitch cycle for one form letter — classic arp walk, not a melody. */
void buildArpOrder(const GrooveParams& g, const std::vector<int>& pool,
                   std::uint32_t seed, int formLetter, std::vector<int>& order)
{
    order.clear();
    if (pool.empty()) return;
    const int n = (int) pool.size();
    auto at = [&](int i) { return pool[(size_t) ((i % n + n) % n)]; };

    switch (g.arpShape())
    {
        case ArpShape::Walk:
            for (int i = 0; i < n; ++i)
                order.push_back(at(i));
            break;
        case ArpShape::Skip:
            for (int i = 0; i < n; i += 2)
                order.push_back(at(i));
            for (int i = 1; i < n; i += 2)
                order.push_back(at(i));
            break;
        case ArpShape::Pedal:
            for (int i = 1; i < n; ++i)
            {
                order.push_back(at(0));
                order.push_back(at(i));
            }
            if (order.empty())
                order.push_back(at(0));
            break;
        case ArpShape::Arch: // Up then down (pendulum), like Sting Fwd/Bwd pendulum
            for (int i = 0; i < n; ++i)
                order.push_back(at(i));
            for (int i = n - 2; i >= 1; --i)
                order.push_back(at(i));
            break;
        case ArpShape::Climb:
            for (int i = 0; i < n; ++i)
                order.push_back(at(i));
            for (int i = 0; i < n; ++i)
                order.push_back(juce::jlimit(0, 127, at(i) + 12));
            break;
        case ArpShape::Rolling:
            // Spiral up through the pool across octaves, then roll back down.
            for (int o = 0; o < juce::jmax(1, 2); ++o)
                for (int i = 0; i < n; ++i)
                    order.push_back(juce::jlimit(0, 127, at(i) + o * 12));
            for (int o = 1; o >= 0; --o)
                for (int i = n - 1; i >= 0; --i)
                    order.push_back(juce::jlimit(0, 127, at(i) + o * 12));
            break;
        case ArpShape::Acid:
            // Root-heavy 303 cycle: 1–1–3–1–5–1–… with seeded octave bumps.
            for (int i = 0; i < juce::jmax(4, n * 2); ++i)
            {
                int deg = 0;
                if ((i % 4) == 2) deg = juce::jmin(2, n - 1);
                else if ((i % 4) == 3) deg = juce::jmin(1, n - 1);
                int note = at(deg);
                if (unit(seed, i, formLetter, 23) < 0.28f)
                    note = juce::jlimit(0, 127, note + 12);
                else if (unit(seed, i, formLetter, 29) < 0.08f)
                    note = juce::jlimit(0, 127, note - 12);
                order.push_back(note);
            }
            break;
        case ArpShape::Count:
            break;
    }
    if (order.empty())
        order.push_back(at(0));

    // Motion flips / stretches the cycle without breaking even timing.
    if (g.arpMotion() == MelodyContour::Fall)
        std::reverse(order.begin(), order.end());
    else if (g.arpMotion() == MelodyContour::Rise)
    {
        for (size_t i = order.size() / 2; i < order.size(); ++i)
            order[i] = juce::jlimit(0, 127, order[i] + 12);
    }
    else if (g.arpMotion() == MelodyContour::Leap)
    {
        for (size_t i = 0; i < order.size(); i += 2)
            order[i] = juce::jlimit(0, 127, order[i] + 12);
    }

    // Form B/C: rotate / invert so AAAB/ABAB changes the line, not the grid.
    if (formLetter == 1)
        std::rotate(order.begin(), order.begin() + (order.size() / 2), order.end());
    else if (formLetter >= 2)
        std::reverse(order.begin(), order.end());

    // Generate: move the same chord tones into other octaves and start the
    // walk on a different one, so the figure is not the previous cycle.
    if (g.arpNonce != 0 && !order.empty())
    {
        bool moved = false;
        for (size_t i = 0; i < order.size(); ++i)
        {
            const float r = unit(seed, (int) i, 90, g.arpNonce);
            const int before = order[i];
            if (r < 0.5f)
                order[i] = juce::jlimit(0, 127, order[i] + 12);
            else if (r > 0.85f)
                order[i] = juce::jlimit(0, 127, order[i] - 12);
            if (order[i] != before)
                moved = true;
        }
        if (!moved)
            order[0] = juce::jlimit(0, 127, order[0] + 12);
        const int rot = (int) (mix(seed ^ (std::uint32_t) g.arpNonce) % (std::uint32_t) order.size());
        if (rot > 0)
            std::rotate(order.begin(), order.begin() + rot, order.end());
    }
}

/** Sting Density: 0% → 1 note, 100% → all 16 pattern steps, spaced evenly. */
void buildEvenDensityMask(int densityPct, bool* active /*[kArpPatternSteps]*/)
{
    const int count = juce::jlimit(1, kArpPatternSteps,
                                   1 + juce::roundToInt((densityPct / 100.0) * (kArpPatternSteps - 1)));
    for (int i = 0; i < kArpPatternSteps; ++i)
        active[i] = false;
    if (count >= kArpPatternSteps)
    {
        for (int i = 0; i < kArpPatternSteps; ++i)
            active[i] = true;
        return;
    }
    for (int i = 0; i < count; ++i)
    {
        const int idx = (i * kArpPatternSteps) / count;
        active[juce::jlimit(0, kArpPatternSteps - 1, idx)] = true;
    }
    active[0] = true; // always land on the first step of the cycle
}

/** Density 100 keeps every step the part already chose. Lower values keep an
    even subset of the 16-step bar so the rhythm stays on the grid. */
bool stepInDensity(int densityPct, int step)
{
    if (densityPct >= 100)
        return true;
    bool active[kArpPatternSteps];
    buildEvenDensityMask(densityPct, active);
    const int s = ((step % kArpPatternSteps) + kArpPatternSteps) % kArpPatternSteps;
    return active[s];
}

void renderArp(const GrooveParams& g, double startPpq, double endPpq,
               const int* chordNotes, int chordCount, int keyRoot, Mode mode,
               std::vector<Event>& out)
{
    out.clear();
    if (!(endPpq > startPpq) || !g.arpEnabled) return;
    if (g.arpInput && (chordNotes == nullptr || chordCount <= 0)) return;

    const std::uint32_t seed = partSeed(g.arpVariation, g.arpNonce, g.arpGenreIndex, 11);
    std::vector<int> pool;
    buildPitchPool(g, chordNotes, chordCount, keyRoot, mode, pool);
    if (pool.empty()) return;

    const double stepLength = juce::jmax(1.0 / 48.0, delayPeriodBeats(g.arpRate()));
    const bool acid = g.arpShape() == ArpShape::Acid;
    const bool rolling = g.arpShape() == ArpShape::Rolling;
    const int phraseBars = generatorLengthBars(g.arpLength());
    const GeneratorForm form = resolveForm(g.arpForm(), seed);

    bool active[kArpPatternSteps];
    buildEvenDensityMask(g.arpDensity, active);
    // Genre colours the line through the seed. It does not punch holes in the
    // Speed grid — an arpeggio stays even.

    // Prebuild orders for form letters A/B/C.
    std::vector<int> orders[3];
    for (int letter = 0; letter < 3; ++letter)
        buildArpOrder(g, pool, seed ^ (std::uint32_t) (letter * 0x9E3779B9u), letter, orders[letter]);

    // Style only colours velocity accents — never timing.
    auto accentVel = [&](int patternStep) -> int
    {
        const bool beat = (patternStep % 4) == 0;
        int base = beat ? 108 : 88;
        switch (g.arpStyle())
        {
            case MelodyStyle::March: base = beat ? 112 : 82; break;
            case MelodyStyle::Hold:  base = beat ? 118 : 70; break;
            case MelodyStyle::Drift: base = 90 + (patternStep % 3) * 6; break;
            case MelodyStyle::Turn:  base = (patternStep % 8) >= 6 ? 110 : 86; break;
            case MelodyStyle::Call:  base = patternStep < 8 ? 108 : 78; break;
            case MelodyStyle::Arch:  base = 84 + std::abs(8 - patternStep) * 3; break;
            case MelodyStyle::Count: break;
        }
        // Genre: slight accent bias only.
        if (g.arpGenre() == GeneratorGenre::Funk && !beat)
            base = juce::jmax(1, base - 12);
        else if (g.arpGenre() == GeneratorGenre::Techno)
            base = juce::jmin(127, base + 4);
        return juce::jlimit(1, 127, base);
    };

    const int firstStep = (int) std::floor(startPpq / stepLength + 1.0e-9);
    const int lastStep = (int) std::ceil(endPpq / stepLength - 1.0e-9);
    // Cursor advances only on sounding steps so the walk stays continuous.
    int noteCursor = 0;
    // Count sounding steps before the window so live/offline stay phase-aligned.
    {
        const int pre = juce::jmax(0, firstStep);
        for (int k = 0; k < pre; ++k)
            if (active[((k % kArpPatternSteps) + kArpPatternSteps) % kArpPatternSteps])
                ++noteCursor;
    }

    for (int k = firstStep; k < lastStep; ++k)
    {
        if (k < 0) continue;
        const double onPpq = (double) k * stepLength;
        if (onPpq < startPpq || onPpq >= endPpq)
            continue;

        const int patternStep = ((k % kArpPatternSteps) + kArpPatternSteps) % kArpPatternSteps;
        if (!active[patternStep])
            continue;

        const int absBar = (int) std::floor(onPpq / 4.0);
        int letter = 0, local = 0;
        mapFormBar(phraseBars, form, wrapBar(juce::jmax(0, absBar), phraseBars), letter, local);
        juce::ignoreUnused(local);
        const auto& order = orders[juce::jlimit(0, 2, letter)];
        if (order.empty()) continue;

        const int note = order[(size_t) (noteCursor % (int) order.size())];
        ++noteCursor;

        const int velocity = accentVel(patternStep);
        // Even gate on the rate grid. Acid/Rolling may overlap into the next step.
        const bool slide = (acid && unit(seed, k, patternStep, 31) < 0.28f)
                        || rolling;
        const double gateMul = rolling ? 1.35 : (slide ? 1.08 : (double) kArpGate);
        const double offPpq = onPpq + stepLength * gateMul;

        out.push_back({ onPpq, note, velocity });
        // Always emit the off inside or just past the window so live chunks
        // never leave a note hanging without a matching note-off.
        out.push_back({ offPpq, note, 0 });
    }
    sortEvents(out);
}

// ── Melody ───────────────────────────────────────────────────────────────────

static bool melodyCellStep(std::uint32_t seed, int styleIndex, int step)
{
    // Distinct 16-step rhythms. Bit 0 is step 0. Generate picks one of these
    // instead of nudging the same downbeat grid.
    static constexpr std::uint16_t kCells[] = {
        (1u << 0) | (1u << 3) | (1u << 6) | (1u << 8) | (1u << 11) | (1u << 14),
        (1u << 0) | (1u << 2) | (1u << 5) | (1u << 8) | (1u << 10) | (1u << 13),
        (1u << 1) | (1u << 4) | (1u << 7) | (1u << 9) | (1u << 12) | (1u << 15),
        (1u << 0) | (1u << 4) | (1u << 7) | (1u << 8) | (1u << 12) | (1u << 15),
        (1u << 2) | (1u << 6) | (1u << 10) | (1u << 12) | (1u << 14),
        (1u << 0) | (1u << 3) | (1u << 4) | (1u << 8) | (1u << 10) | (1u << 12) | (1u << 15)
    };
    const int idx = (int) ((mix(seed) + (std::uint32_t) styleIndex) % (std::uint32_t) (sizeof kCells / sizeof kCells[0]));
    return (kCells[idx] & (std::uint16_t) (1u << (step & 15))) != 0;
}

bool melodyStepPlays(const GrooveParams& g, int phraseBar, int step)
{
    const auto style = g.melodyStyle();
    const auto speed = g.melodySpeed();
    const std::uint32_t seed = partSeed(g.melodyVariation, g.melodyNonce, g.melodyGenreIndex, 21);
    const auto feel = genreFeel(g.melodyGenre());
    const float roll = unit(seed, phraseBar, step, 8);

    if (g.melodyNonce != 0)
    {
        if (!melodyCellStep(seed, g.melodyStyleIndex, step))
            return false;
        if (speed == MelodySpeed::Slow && (step % 2) == 1) return false;
        if (speed == MelodySpeed::Medium && (step % 4) == 1) return false;
        return true;
    }

    if (speed == MelodySpeed::Slow && (step % 2) == 1) return false;
    if (speed == MelodySpeed::Medium && (step % 4) == 1) return false;
    if (feel.densityMul < 0.7f && !feel.preferOffbeats && (step % 4) == 2
        && roll > feel.densityMul)
        return false;

    const bool downbeat = step == 0 || step == 8;
    switch (style)
    {
        case MelodyStyle::March:
            return step == 0 || step == 4 || step == 8 || step == 12
                || ((step % 2) == 0 && roll < 0.35f * feel.densityMul)
                || (feel.preferOffbeats && (step % 2) == 1 && roll < feel.syncopation * 0.5f);
        case MelodyStyle::Arch:
        {
            const float mid = 1.0f - std::abs(step - 8) / 8.0f;
            return downbeat || roll < mid * 0.7f * feel.densityMul;
        }
        case MelodyStyle::Call:
            return phraseBar % 2 == 0
                ? (downbeat || step == 4 || roll < 0.45f * feel.densityMul)
                : (downbeat || roll < 0.2f * feel.densityMul);
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
    const std::uint32_t seed = partSeed(g.melodyVariation, g.melodyNonce, g.melodyGenreIndex, 22);

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

    if (g.melodyNonce != 0)
    {
        int index = 0;
        for (int i = 0; i < count; ++i)
            if ((keyRoot + deg[i]) % 12 == harmony.root) { index = i; break; }
        const float amount = unit(seed, phraseBar, step, 13);
        const int hops = 1 + (int) (amount * 6.0f);
        const int direction = unit(seed, phraseBar, step, 17) < 0.5f ? -1 : 1;
        int at = index + direction * hops;
        int octShift = unit(seed, phraseBar, step, 19) < 0.4f ? (direction > 0 ? 1 : -1) : 0;
        while (at < 0) { at += count; --octShift; }
        while (at >= count) { at -= count; ++octShift; }
        const int pc = (keyRoot + deg[at]) % 12;
        return pitchInRange(pc, 48, 96, center + octShift * 12);
    }

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
    const std::uint32_t seed = partSeed(g.melodyVariation, g.melodyNonce, g.melodyGenreIndex, 23);
    const GeneratorForm form = resolveForm(g.melodyForm(), seed);
    const int sectionBars = juce::jmax(1, phraseBars / 4);
    const int first = std::max(0, (int) std::floor((startPpq - 1.0) / 0.25) - 1);
    const int last = (int) std::ceil((endPpq + 0.25) / 0.25) + 1;

    for (int index = first; index <= last; ++index)
    {
        const int bar = index / 16;
        const int step = index % 16;
        if (bar < 0) continue;
        if (!stepInDensity(g.melodyDensity, step)) continue;

        int letter = 0, localBar = 0;
        mapFormBar(phraseBars, form, wrapBar(bar, phraseBars), letter, localBar);
        GrooveParams motif = g;
        motif.melodyVariation = juce::jlimit(1, 16, g.melodyVariation + letter);
        const int phraseBar = localBar % juce::jmax(1, sectionBars);
        if (!melodyStepPlays(motif, phraseBar, step)) continue;

        const Harmony harmony = g.melodyInput
            ? harmonyFromHeld(chordNotes, chordCount, keyRoot, mode, bar)
            : progressionHarmony(keyRoot, mode, bar);
        int note = melodyNote(motif, harmony, phraseBar, step, keyRoot, mode);
        note = fitToScale(note, juce::jlimit(0, 11, keyRoot), mode);
        if (g.melodyType() == PhraseType::Pentatonic || g.melodyType() == PhraseType::Chord)
        {
            const int root = ((keyRoot % 12) + 12) % 12;
            const int rel = ((note % 12) - root + 12) % 12;
            const bool minor = mode == Mode::Aeolian || mode == Mode::Dorian
                            || mode == Mode::Phrygian || mode == Mode::Locrian;
            const int targets[5] = {
                0,
                g.melodyType() == PhraseType::Chord ? (minor ? 3 : 4) : 2,
                g.melodyType() == PhraseType::Chord ? 7 : 4,
                7,
                9
            };
            const int nTargets = g.melodyType() == PhraseType::Chord ? 3 : 5;
            int best = 0, dist = 99;
            for (int i = 0; i < nTargets; ++i)
            {
                const int d = std::min(std::abs(rel - targets[i]), 12 - std::abs(rel - targets[i]));
                if (d < dist) { dist = d; best = targets[i]; }
            }
            int delta = best - rel;
            if (delta > 6) delta -= 12;
            if (delta < -6) delta += 12;
            note = juce::jlimit(0, 127, note + delta);
        }
        else if (g.melodyType() == PhraseType::Chromatic && (step % 3) == 2)
            note = juce::jlimit(0, 127, note + 1);
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
    if (g.chordType() == PhraseType::Pentatonic && pitches.size() > 2)
        pitches.resize(2);
    else if (g.chordType() == PhraseType::Chromatic && !pitches.empty())
        pitches.push_back(juce::jlimit(0, 127, pitches.back() + 1));
    if (g.chordMotion() == MelodyContour::Rise && !pitches.empty())
        pitches.back() = juce::jlimit(0, 127, pitches.back() + 12);
    else if (g.chordMotion() == MelodyContour::Fall && !pitches.empty())
        pitches.front() = juce::jlimit(0, 127, pitches.front() - 12);
    else if (g.chordMotion() == MelodyContour::Leap && pitches.size() > 1)
        pitches.back() = juce::jlimit(0, 127, pitches.back() + 7);
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

static bool chordCellFires(const GrooveParams& g, std::uint32_t seed, int step)
{
    static constexpr std::uint16_t kBlock[] = {
        (1u << 0) | (1u << 6) | (1u << 8) | (1u << 14),
        (1u << 2) | (1u << 8) | (1u << 12),
        (1u << 0) | (1u << 7) | (1u << 8) | (1u << 15),
        (1u << 0) | (1u << 4) | (1u << 8) | (1u << 11)
    };
    static constexpr std::uint16_t kPulse[] = {
        (1u << 0) | (1u << 3) | (1u << 6) | (1u << 8) | (1u << 11) | (1u << 14),
        (1u << 2) | (1u << 6) | (1u << 10) | (1u << 14),
        (1u << 0) | (1u << 4) | (1u << 7) | (1u << 8) | (1u << 12) | (1u << 15),
        (1u << 0) | (1u << 5) | (1u << 8) | (1u << 10) | (1u << 13)
    };
    static constexpr std::uint16_t kStrum[] = {
        (1u << 0) | (1u << 6) | (1u << 8) | (1u << 14),
        (1u << 2) | (1u << 8) | (1u << 12),
        (1u << 0) | (1u << 4) | (1u << 8) | (1u << 11),
        (1u << 4) | (1u << 6) | (1u << 12) | (1u << 14)
    };
    const std::uint16_t* bank = kBlock;
    int count = 4;
    switch (g.chordRhythm())
    {
        case ChordRhythm::Pulse: bank = kPulse; break;
        case ChordRhythm::Strum: bank = kStrum; break;
        case ChordRhythm::Block:
        case ChordRhythm::Count: break;
    }
    const int idx = (int) ((mix(seed) + (std::uint32_t) g.chordGenreIndex) % (std::uint32_t) count);
    return (bank[idx] & (std::uint16_t) (1u << (step & 15))) != 0;
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
    const std::uint32_t seed = partSeed(g.chordVariation, g.chordNonce, g.chordGenreIndex, 31);
    const auto feel = genreFeel(g.chordGenre());
    const auto& chordPack = genrePack(g.chordGenre());
    const bool packedChords = chordPack.anyChords();
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
        double tick = 0.0;
        if (packedChords)
        {
            const auto& st = chordPack.chords[(size_t) step];
            fire = st.velocity > 0;
            length = chordPack.chordGate;
            velocity = st.velocity;
            tick = st.ticks / 480.0;
        }
        else switch (g.chordRhythm())
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
        if (!packedChords)
        {
            if (g.chordSpeed() == MelodySpeed::Slow)
                fire = step == 0 || step == 8;
            else if (g.chordSpeed() == MelodySpeed::Fast && (step % 4) == 2)
                fire = true;
            if (fire && feel.densityMul < 0.7f && step != 0
                && unit(motifSeed, bar, step, 11) > feel.densityMul)
                fire = false;
            if (!fire && feel.preferOffbeats && (step == 2 || step == 6 || step == 10 || step == 14)
                && unit(motifSeed, bar, step, 13) < feel.syncopation * 0.55f)
            {
                fire = true;
                length = 0.45;
            }
            length = juce::jmax(0.2, length + (double) feel.sustainBias * 0.6);
        }
        if (g.chordNonce != 0)
        {
            fire = chordCellFires(g, seed, step);
            if (g.chordSpeed() == MelodySpeed::Slow)
                fire = fire && (step % 4) == 0;
            else if (g.chordSpeed() == MelodySpeed::Fast && (step % 4) == 2)
                fire = true;
            if (g.chordRhythm() == ChordRhythm::Strum)
                length = 0.35;
            else if (g.chordRhythm() == ChordRhythm::Pulse)
                length = 0.55;
            else
                length = 1.1;
        }
        if (fire && !stepInDensity(g.chordDensity, step))
            fire = false;
        if (!fire) continue;

        const Harmony harmony = chordHarmony(g, chordNotes, chordCount, keyRoot, mode, bar + letter);
        std::vector<int> pitches;
        chordVoices(g, harmony, pitches);
        if (g.chordNonce != 0 && !pitches.empty())
        {
            if (pitches.size() > 1)
            {
                const int rot = (int) (mix(motifSeed ^ (std::uint32_t) (step * 17)) % (std::uint32_t) pitches.size());
                std::rotate(pitches.begin(), pitches.begin() + rot, pitches.end());
            }
            const int leap = unit(motifSeed, bar, step, 19) < 0.5f ? 12 : -12;
            pitches.back() = juce::jlimit(0, 127, pitches.back() + leap);
        }
        for (int& p : pitches)
            p = fitToScale(p, juce::jlimit(0, 11, keyRoot), mode);
        const double swing = (step % 4) == 2 ? chordPack.swing * 0.25 : 0.0;
        const double onBase = bar * 4.0 + step * 0.25 + tick + swing;
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
    if (!packedChords && isFullPhraseWindow(startPpq, endPpq, phraseBars))
        musicalQaPass(out, phraseBars, false);
    sortEvents(out);
}

// ── Bass ─────────────────────────────────────────────────────────────────────

static bool bassCellStep(const GrooveParams& g, std::uint32_t seed, int step,
                          int& degree, int& prefer, double& len)
{
    static constexpr std::uint16_t kPulse[] = {
        (1u << 0) | (1u << 3) | (1u << 6) | (1u << 8) | (1u << 10) | (1u << 14),
        (1u << 0) | (1u << 4) | (1u << 7) | (1u << 8) | (1u << 12) | (1u << 15),
        (1u << 2) | (1u << 6) | (1u << 8) | (1u << 11) | (1u << 14),
        (1u << 0) | (1u << 5) | (1u << 8) | (1u << 10) | (1u << 13)
    };
    static constexpr std::uint16_t kHalf[] = {
        (1u << 0) | (1u << 10),
        (1u << 4) | (1u << 12),
        (1u << 0) | (1u << 6) | (1u << 12),
        (1u << 2) | (1u << 8)
    };
    static constexpr std::uint16_t kShuffle[] = {
        (1u << 0) | (1u << 3) | (1u << 6) | (1u << 10) | (1u << 12),
        (1u << 0) | (1u << 5) | (1u << 8) | (1u << 11) | (1u << 14),
        (1u << 2) | (1u << 6) | (1u << 8) | (1u << 10) | (1u << 15)
    };
    static constexpr std::uint16_t kSparse[] = {
        (1u << 0) | (1u << 7),
        (1u << 4) | (1u << 14),
        (1u << 2) | (1u << 8) | (1u << 12),
        (1u << 0) | (1u << 11)
    };
    const std::uint16_t* bank = kPulse;
    int count = 4;
    len = 0.45;
    switch (g.bassMotion())
    {
        case BassMotion::Half:    bank = kHalf; len = 1.2; break;
        case BassMotion::Shuffle: bank = kShuffle; count = 3; len = 0.35; break;
        case BassMotion::Sparse:  bank = kSparse; len = 0.9; break;
        case BassMotion::Pulse:
        case BassMotion::Count:   break;
    }
    const int idx = (int) ((mix(seed) + (std::uint32_t) g.bassGenreIndex) % (std::uint32_t) count);
    if ((bank[idx] & (std::uint16_t) (1u << (step & 15))) == 0)
        return false;
    const int pick = (step + idx * 3) % 5;
    degree = pick == 1 ? 2 : (pick == 2 ? 1 : (pick == 4 ? 4 : 0));
    prefer = 36 + ((idx + step / 4) % 3) * 6;
    return true;
}

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
    const std::uint32_t seed = partSeed(g.bassVariation, g.bassNonce, g.bassGenreIndex, 41);
    const auto feel = genreFeel(g.bassGenre());
    const auto& bassPack = genrePack(g.bassGenre());
    const bool packedBass = bassPack.anyBass();
    const GeneratorForm form = resolveForm(g.bassForm(), seed);
    const int bassBias = (int) (mix(seed) >> 9) % 5;
    static constexpr int bassShift[] = { 0, 3, 7, 10, 12 };
    const int preferShift = bassShift[bassBias];
    const bool color = ((mix(seed) >> 20) & 1u) != 0;

    auto emit = [&](int bar, int step, int letter, int degree, int prefer, float chance, int vel, double len)
    {
        const std::uint32_t motifSeed = seed ^ (std::uint32_t) (letter * 29u);
        if (!stepInDensity(g.bassDensity, step))
            return;
        if (chance < 0.999f && unit(motifSeed, bar, step, degree + 20) >= chance)
            return;
        const Harmony harmony = g.bassInput
            ? harmonyFromHeld(chordNotes, chordCount, keyRoot, mode, bar)
            : progressionHarmony(keyRoot, mode, bar + letter);
        const int note = fitToScale(pitchInRange(toneOf(harmony, degree), 28, 52, prefer + preferShift),
                                    juce::jlimit(0, 11, keyRoot), mode);
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
        if (g.bassNonce != 0)
        {
            int degree = 0, prefer = 40;
            double len = 0.45;
            if (bassCellStep(g, seed, step, degree, prefer, len))
                emit(bar, step, letter, degree, prefer, 1.0f, 104, len);
            continue;
        }
        if (packedBass)
        {
            const auto& st = bassPack.bass[(size_t) step];
            if (st.velocity <= 0)
                continue;
            if (!stepInDensity(g.bassDensity, step))
                continue;
            const Harmony harmony = g.bassInput
                ? harmonyFromHeld(chordNotes, chordCount, keyRoot, mode, bar)
                : progressionHarmony(keyRoot, mode, bar + letter);
            const int degree = 0;
            const int note = fitToScale(pitchInRange(toneOf(harmony, degree), 28, 52, 40),
                                        juce::jlimit(0, 11, keyRoot), mode);
            const double swing = (step % 4) == 2 ? bassPack.swing * 0.25 : 0.0;
            const double onPpq = bar * 4.0 + step * 0.25 + st.ticks / 480.0 + swing;
            const double offPpq = onPpq + bassPack.bassGate;
            if (onPpq >= startPpq && onPpq < endPpq)
                out.push_back({ onPpq, note, st.velocity });
            if (offPpq >= startPpq && offPpq < endPpq)
                out.push_back({ offPpq, note, 0 });
            continue;
        }
        float densBoost = letter == 0 ? 0.0f : (letter == 1 ? 0.12f : 0.2f);
        densBoost += (feel.densityMul - 1.0f) * 0.5f;
        if (g.bassSpeed() == MelodySpeed::Slow && (step % 4) != 0)
            continue;
        if (g.bassSpeed() == MelodySpeed::Fast)
            densBoost += 0.35f;
        if (feel.densityMul < 0.7f && step != 0 && step != 8
            && unit(seed, bar, step, 71) > feel.densityMul + densBoost)
            continue;
        if (g.bassStyle() == MelodyStyle::Hold && step != 0 && step != 8)
            continue;

        int degree = 0;
        if (g.bassType() == PhraseType::Chord)
            degree = (step / 4) % 3 == 1 ? 2 : 0;
        else if (g.bassType() == PhraseType::Pentatonic)
            degree = (step / 4) % 2 == 1 ? 4 : 0;
        else if (g.bassType() == PhraseType::Chromatic && (step % 4) == 2)
            degree = 1;

        if (degree != 0 && (step == 0 || step == 8))
            emit(bar, step, letter, degree, 40, 1.0f, 96, 0.35);

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
    if (!packedBass && isFullPhraseWindow(startPpq, endPpq, phraseBars))
        musicalQaPass(out, phraseBars, false);
    sortEvents(out);
}

// ── Drums (genre styles) ─────────────────────────────────────────────────────
// GM: 36 kick, 38 snare, 37 side stick, 39 clap, 42 closed hat, 46 open hat,
// 43 low tom, 47 mid tom, 50 high tom.

struct DrumHit { int note; float chance; int velocity; int ticks = 0; };

// One bar of a genre groove. Bits: kick, snare, closed hat, open hat, clap, ghost, rim.
static constexpr std::uint8_t kDk = 1, kDs = 2, kDh = 4, kDo = 8, kDc = 16, kDg = 32, kDr = 64;

static const std::uint8_t* drumGrooveCell(DrumStyle style, std::uint32_t seed)
{
    static constexpr std::uint8_t kHouse[4][16] = {
        { kDk, 0, kDh, 0, kDk|kDc, 0, kDh, 0, kDk, 0, kDh, 0, kDk|kDc, 0, kDo, 0 },
        { kDk|kDh, kDh, kDh, kDh, kDk|kDc|kDh, kDh, kDh|kDo, kDh, kDk|kDh, kDh, kDh, kDh, kDk|kDc|kDh, kDh, kDh, kDh },
        { kDk|kDh, 0, kDh, 0, kDk|kDr|kDh, 0, kDh, 0, kDk|kDh, 0, kDh|kDo, 0, kDk|kDr|kDh, 0, kDh, 0 },
        { kDk, 0, kDh, 0, kDk|kDc, 0, kDo, 0, kDk, 0, kDh, 0, kDk|kDc, 0, kDh, kDk }
    };
    static constexpr std::uint8_t kTechno[4][16] = {
        { kDk, 0, kDh, 0, kDk, 0, kDh, 0, kDk, 0, kDh, 0, kDk|kDc, 0, kDo, 0 },
        { kDk|kDh, kDh, kDh, kDh, kDk|kDc|kDh, kDh, kDh, kDh, kDk|kDh, kDh, kDh, kDh, kDk|kDc|kDh, kDh, kDh, kDh },
        { kDk, 0, kDh, 0, kDk, 0, kDo, 0, kDk, 0, kDh, 0, kDk, 0, kDo, 0 },
        { kDk|kDh, kDh, kDh, kDh, kDk|kDh, kDh, kDh, kDh, kDk|kDh, kDh, kDh, kDh, kDk|kDc|kDh, kDh, kDh, kDk|kDh }
    };
    static constexpr std::uint8_t kDubTechno[4][16] = {
        { kDk, 0, 0, 0, 0, 0, 0, 0, kDk, 0, kDh, 0, kDr, 0, 0, 0 },
        { kDk, 0, 0, 0, 0, 0, kDk, 0, 0, 0, kDr, 0, 0, 0, 0, 0 },
        { kDk, 0, 0, 0, 0, 0, 0, 0, kDk, 0, 0, 0, 0, kDr, 0, 0 },
        { kDk, 0, 0, 0, kDh, 0, 0, 0, 0, 0, kDk, 0, 0, 0, kDr, 0 }
    };
    static constexpr std::uint8_t kReggae[4][16] = {
        { 0, 0, kDh, 0, 0, 0, kDh, 0, kDk|kDs, 0, kDh, 0, 0, 0, kDh, 0 },
        { kDk|kDh, 0, kDh, 0, kDk|kDh, 0, kDh, 0, kDk|kDs|kDh, 0, kDh, 0, kDk|kDh, 0, kDh, 0 },
        { kDk, 0, kDh, 0, kDs, 0, kDh, 0, kDk, 0, kDh, 0, kDs, 0, kDh, 0 },
        { 0, 0, 0, 0, kDh, 0, 0, 0, kDk|kDs, 0, 0, 0, kDh, 0, 0, 0 }
    };
    static constexpr std::uint8_t kDub[4][16] = {
        { kDk, 0, 0, 0, 0, 0, 0, 0, kDs, 0, kDh, 0, 0, 0, 0, 0 },
        { kDk, 0, 0, 0, 0, 0, 0, 0, 0, 0, kDs, 0, 0, 0, kDh, 0 },
        { kDk, 0, 0, 0, 0, 0, kDo, 0, kDs, 0, 0, 0, 0, 0, 0, 0 },
        { kDk, 0, 0, 0, 0, 0, 0, 0, 0, kDs, 0, 0, 0, 0, 0, 0 }
    };
    static constexpr std::uint8_t kFunk[3][16] = {
        { kDk|kDh, kDh, kDh, kDg, kDs|kDh, kDh, kDk|kDh, kDh, kDh, kDh, kDh, kDg, kDs|kDh, kDh, kDh, kDh },
        { kDk|kDh, kDh, kDh, kDh, kDs|kDh, kDh, kDk|kDh, kDg, kDh, kDh, kDh, kDh, kDs|kDh, kDh, kDk|kDh, kDg },
        { kDk|kDh, kDh, kDh, kDk, kDs|kDh, kDh, kDk|kDh, kDh, kDh, kDg, kDk|kDh, kDh, kDs|kDh, kDh, kDh, kDg }
    };
    static constexpr std::uint8_t kBreak[4][16] = {
        { kDk, 0, kDh, 0, kDs, 0, kDh, 0, 0, 0, kDk|kDh, 0, kDs, 0, kDh, 0 },
        { kDk|kDh, 0, kDh, 0, kDs|kDh, 0, kDh, 0, kDk|kDh, 0, kDh, kDk, kDh, kDs, kDh, 0 },
        { kDk|kDh, 0, kDh, 0, kDs|kDh, 0, kDk|kDh, 0, kDh, kDk, kDh, 0, kDs|kDh, 0, kDh, kDs },
        { kDk, 0, kDk|kDh, 0, kDs, 0, kDh, kDs, 0, 0, kDk|kDh, 0, kDs, 0, kDh, 0 }
    };
    static constexpr std::uint8_t kDnb[4][16] = {
        { kDk, 0, kDh, 0, kDs, 0, kDh, 0, 0, 0, kDk|kDh, 0, kDs, 0, kDh, 0 },
        { kDk|kDh, kDh, kDh, kDk|kDh, kDs|kDh, kDh, kDh, kDh, kDk|kDh, kDh, kDh, kDk|kDh, kDs|kDh, kDh, kDh, kDh },
        { kDk, 0, kDh, 0, kDs, 0, kDh, kDk, 0, 0, kDh, 0, kDs, 0, kDh|kDg, 0 },
        { kDk|kDh, 0, kDh, 0, kDs|kDh, 0, kDk|kDh, 0, kDh, 0, kDk|kDh, 0, kDs|kDh, 0, kDo, 0 }
    };

    const std::uint8_t* bank = kHouse[0];
    int count = 4;
    switch (style)
    {
        case DrumStyle::House:      bank = kHouse[0]; break;
        case DrumStyle::Techno:     bank = kTechno[0]; break;
        case DrumStyle::DubTechno:  bank = kDubTechno[0]; break;
        case DrumStyle::Reggae:     bank = kReggae[0]; break;
        case DrumStyle::Dub:        bank = kDub[0]; break;
        case DrumStyle::Funk:       bank = kFunk[0]; count = 3; break;
        case DrumStyle::Breakbeat:  bank = kBreak[0]; break;
        case DrumStyle::Dnb:        bank = kDnb[0]; break;
        case DrumStyle::Count:      break;
    }
    const int idx = (int) (mix(seed) % (std::uint32_t) count);
    return bank + idx * 16;
}

static void hitsFromDrumCell(std::uint8_t mask, std::vector<DrumHit>& hits)
{
    auto put = [&](std::uint8_t bit, int note, int velocity)
    {
        if ((mask & bit) != 0)
            hits.push_back({ note, 1.0f, velocity, 0 });
    };
    put(kDk, 36, 112);
    put(kDs, 38, 116);
    put(kDh, 42, 78);
    put(kDo, 46, 96);
    put(kDc, 39, 104);
    put(kDg, 38, 40);
    put(kDr, 37, 88);
}

void drumHitsForStep(DrumStyle style, int step, float /*density*/, float fill,
                     bool fillBar, std::uint32_t /*seed*/, int /*formLetter*/,
                     std::vector<DrumHit>& hits)
{
    hits.clear();
    const auto& pack = genrePack(style);
    auto add = [&](int note, const PackStep& hit)
    {
        if (hit.velocity > 0)
            hits.push_back({ note, 1.0f, hit.velocity, hit.ticks });
    };
    const int s = step & 15;
    add(36, pack.kick[(size_t) s]);
    add(38, pack.snare[(size_t) s]);
    add(42, pack.hat[(size_t) s]);
    add(38, pack.ghost[(size_t) s]);

    const float fillAmt = clampf(fill / 100.0f, 0.0f, 1.0f);
    if (fillBar && fillAmt > 0.05f && s >= 12)
    {
        if (s == 12 || s == 14)
            hits.push_back({ 38, 0.45f * fillAmt, 96, 0 });
        if (s == 13 || s == 15)
            hits.push_back({ s == 15 ? 50 : 47, 0.4f * fillAmt, 84, 0 });
    }
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
    const std::uint32_t seed = partSeed(g.drumVariation, g.drumNonce, g.drumStyleIndex, 51);
    const GeneratorForm form = resolveForm(g.drumForm(), seed);
    const auto drumSpeed = g.drumSpeed();
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
        // Fill only on the last bar of a multi-bar section (never when sectionBars==1,
        // or every bar of a 4-bar loop would become a fill and kill four-on-the-floor).
        const bool fillBar = (phraseBar == phraseBars - 1)
            || (sectionBars > 1 && local == sectionBars - 1);
        // Letter B/C: denser hats / ghosts; skeleton stays solid.
        const float dens = (float) g.drumDensity + (letter == 1 ? 10.0f : (letter == 2 ? 18.0f : 0.0f));
        // Salt pattern bank by form letter so ABAB actually swaps motifs.
        const std::uint32_t motifSeed = seed ^ (std::uint32_t) (letter * 0x9E3779B9u);

        for (int step = 0; step < 16; ++step)
        {
            if (drumSpeed == MelodySpeed::Slow && (step % 2) != 0)
                continue;
            drumHitsForStep(g.drumStyle(), step, dens, (float) g.drumFill, fillBar, motifSeed,
                            letter, hits);
            if (g.drumNonce != 0)
            {
                hits.clear();
                hitsFromDrumCell(drumGrooveCell(g.drumStyle(), motifSeed)[step], hits);
                const float fillAmt = clampf((float) g.drumFill / 100.0f, 0.0f, 1.0f);
                if (fillBar && fillAmt > 0.05f && step >= 12)
                {
                    if (step == 12 || step == 14)
                        hits.push_back({ 38, 0.45f * fillAmt, 96, 0 });
                    if (step == 13 || step == 15)
                        hits.push_back({ step == 15 ? 50 : 47, 0.4f * fillAmt, 84, 0 });
                }
            }
            if (drumSpeed == MelodySpeed::Fast && (step % 2) == 1)
            {
                bool hasHat = false;
                for (const auto& hit : hits)
                    if (hit.note == 42)
                        hasHat = true;
                if (!hasHat)
                    hits.push_back({ 42, 0.85f, 68 });
            }
            const auto& drumPack = genrePack(g.drumStyle());
            const double swing = (step % 4) == 2 ? drumPack.swing * 0.25 : 0.0;
            for (const auto& hit : hits)
            {
                if (hit.chance < 0.999f && unit(seed, absBar, step, hit.note + letter * 17) >= hit.chance)
                    continue;
                const double onPpq = absBar * 4.0 + step * 0.25 + hit.ticks / 480.0 + swing;
                if (onPpq >= startPpq && onPpq < endPpq)
                    out.push_back({ onPpq, hit.note, hit.velocity });
                const double offPpq = onPpq + 0.2;
                if (offPpq >= startPpq && offPpq < endPpq)
                    out.push_back({ offPpq, hit.note, 0 });
            }
        }
    }
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
