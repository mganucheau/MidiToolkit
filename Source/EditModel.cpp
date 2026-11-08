#include "EditModel.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace pflow {

const std::array<const char*, 12> kNoteNames {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const char* clipKindName(ClipKind k)
{
    switch (k)
    {
        case ClipKind::Bass:  return "Bass";
        case ClipKind::Piano: return "Piano";
        case ClipKind::Lead:  return "Lead";
        case ClipKind::Drums: return "Drums";
    }
    return "Lead";
}

bool isGmDrumPitch(int midi)
{
    return midi >= 35 && midi <= 81;
}

namespace {

bool tokenEquals(const juce::String& tok, const char* word)
{
    return tok.equalsIgnoreCase(word);
}

bool tokenStartsWith(const juce::String& tok, const char* prefix)
{
    return tok.startsWithIgnoreCase(prefix);
}

/** Split a file name into alphanumeric tokens (ba_bassline_01 → ba, bassline, 01). */
juce::StringArray fileNameTokens(const juce::String& nameOrPath)
{
    juce::String leaf = juce::File(nameOrPath).getFileNameWithoutExtension();
    if (leaf.isEmpty())
        leaf = nameOrPath;
    juce::String cleaned;
    cleaned.preallocateBytes((size_t) leaf.length());
    for (auto c : leaf)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(c))
            cleaned += c;
        else
            cleaned += ' ';
    }
    return juce::StringArray::fromTokens(cleaned, " ", "");
}

} // namespace

ClipKind hintKindFromFileName(const juce::String& nameOrPath)
{
    // Default Lead means "no useful hint" for callers that treat it as optional.
    if (nameOrPath.isEmpty())
        return ClipKind::Lead;

    const auto tokens = fileNameTokens(nameOrPath);
    const auto lower = nameOrPath.toLowerCase();

    auto hasToken = [&](const char* w)
    {
        for (const auto& t : tokens)
            if (tokenEquals(t, w))
                return true;
        return false;
    };
    auto hasTokenPrefix = [&](const char* p)
    {
        const int plen = juce::String(p).length();
        for (const auto& t : tokens)
            if (tokenStartsWith(t, p) && t.length() <= plen + 3)
                return true;
        return false;
    };

    // Drums / kit — check before bass so "drum bass" style names still hit drums first
    // only when an explicit drum token is present.
    if (hasToken("drums") || hasToken("drum") || hasToken("drumkit")
        || hasToken("kit") || hasToken("perc") || hasToken("percussion")
        || hasToken("break") || hasToken("breaks") || hasToken("breakbeat")
        || hasToken("hats") || hasToken("hat") || hasToken("kick") || hasToken("snare")
        || hasToken("beat") || hasToken("beats") || hasToken("loopkit")
        || lower.contains("drumkit") || lower.contains("drum_kit") || lower.contains("drum-kit"))
        return ClipKind::Drums;

    // Bass — full words plus short tokens BA / BAS used in pack naming.
    if (hasToken("bassline") || hasToken("basslin") || hasToken("bass")
        || hasToken("bas") || hasToken("ba")
        || hasTokenPrefix("bass")
        || lower.contains("bassline") || lower.contains("bass_line") || lower.contains("bass-line"))
        return ClipKind::Bass;

    // Keys / piano / pads / chords.
    if (hasToken("piano") || hasToken("keys") || hasToken("key") || hasToken("keyz")
        || hasToken("chord") || hasToken("chords") || hasToken("pad") || hasToken("pads")
        || hasToken("rhodes") || hasToken("epiano") || hasToken("ep")
        || lower.contains("e-piano") || lower.contains("e_piano") || lower.contains("keys_"))
        return ClipKind::Piano;

    // Lead / melody.
    if (hasToken("lead") || hasToken("melody") || hasToken("melo") || hasToken("mel")
        || hasToken("topline") || hasToken("synth") || hasTokenPrefix("lead"))
        return ClipKind::Lead;

    return ClipKind::Lead;
}

namespace {

const std::array<std::array<int, 7>, kNumModes> kModeIntervals {{
    {{ 0, 2, 4, 5, 7, 9, 11 }},  // Ionian
    {{ 0, 2, 3, 5, 7, 9, 10 }},  // Dorian
    {{ 0, 1, 3, 5, 7, 8, 10 }},  // Phrygian
    {{ 0, 2, 4, 6, 7, 9, 11 }},  // Lydian
    {{ 0, 2, 4, 5, 7, 9, 10 }},  // Mixolydian
    {{ 0, 2, 3, 5, 7, 8, 10 }},  // Aeolian
    {{ 0, 1, 3, 5, 6, 8, 10 }},  // Locrian
}};

// Friendly display names: Ionian/Aeolian read as Major/Minor.
const std::array<const char*, kNumModes> kModeNames {
    "Major", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Minor", "Locrian"
};

int wrapPc(int v) { return ((v % 12) + 12) % 12; }

} // namespace

const char* modeName(Mode m) { return kModeNames[(size_t) m]; }

const std::array<int, 7>& modeIntervals(Mode m) { return kModeIntervals[(size_t) m]; }

Mode auditionMode(Mode m)
{
    // Only flatten "bright major-ish" modes that preserve accidentals users
    // usually want Fit-to-Scale to remove (#4 in Lydian, b7 in Mixolydian).
    // Leave Dorian/Phrygian/etc. alone — same-root Minor would be a different scale.
    switch (m)
    {
        case Mode::Lydian:
        case Mode::Mixolydian:
            return Mode::Ionian;
        default:
            return m;
    }
}

juce::String pitchName(int midi)
{
    const int oct = (int) std::floor(midi / 12.0) - 1;   // MIDI 60 = C4
    return juce::String(kNoteNames[(size_t) wrapPc(midi)]) + juce::String(oct);
}

bool isBlackKeyPitch(int midi)
{
    const int pc = wrapPc(midi);
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

int scientificOctave(int midi)
{
    return (int) std::floor((double) midi / 12.0) - 1;
}

int clipReferenceOctave(const StepClip& clip)
{
    if (clip.notes.empty())
        return 4;
    int lo = 127;
    for (const auto& n : clip.notes)
        lo = juce::jmin(lo, n.pitch);
    return juce::jlimit(0, 6, scientificOctave(lo));
}

ScaleAnalysis analyseClipScale(const StepClip& clip)
{
    ScaleAnalysis out;
    if (clip.notes.empty())
        return out;

    uint16_t pcs = 0;
    int lo = 127, hi = 0;
    int pcWeight[12] = {};
    for (const auto& n : clip.notes)
    {
        const int pc = wrapPc(n.pitch);
        pcs = (uint16_t) (pcs | (uint16_t) (1u << pc));
        pcWeight[pc] += juce::jmax(1, (int) std::lround(n.len));
        lo = juce::jmin(lo, n.pitch);
        hi = juce::jmax(hi, n.pitch);
    }

    const int octSpan = scientificOctave(hi) - scientificOctave(lo) + 1;
    out.octaveRange = juce::jlimit(0, 3, octSpan > 1 ? octSpan : 0);

    int bestScore = -1;
    for (int root = 0; root < 12; ++root)
    {
        for (int mi = 0; mi < kNumModes; ++mi)
        {
            bool scalePc[12] = {};
            for (int iv : modeIntervals((Mode) mi))
                scalePc[wrapPc(root + iv)] = true;

            bool covers = true;
            int used = 0;
            for (int pc = 0; pc < 12; ++pc)
            {
                if ((pcs & (uint16_t) (1u << pc)) == 0) continue;
                if (!scalePc[pc]) { covers = false; break; }
                ++used;
            }
            if (!covers) continue;

            out.keyMask = (uint16_t) (out.keyMask | (uint16_t) (1u << root));
            out.modeMask = (uint16_t) (out.modeMask | (uint16_t) (1u << mi));

            // Prefer denser coverage of scale tones + weight on the root PC.
            int score = used * 100 + pcWeight[root];
            if (clip.root == root) score += 50;
            if (mi == (int) Mode::Ionian || mi == (int) Mode::Aeolian) score += 5;
            if (score > bestScore)
            {
                bestScore = score;
                out.primaryRoot = root;
                // Seed Major/Minor so Fit-to-Scale auditions the common key,
                // not a covering mode that preserves accidentals (e.g. Lydian
                // keeping F# when the user expects C major).
                out.primaryMode = auditionMode((Mode) mi);
            }
        }
    }

    // Fallback: estimated root + Major if no diatonic cover (e.g. chromatic).
    if (out.primaryRoot < 0)
    {
        out.primaryRoot = clip.root >= 0 ? clip.root : 0;
        out.primaryMode = Mode::Ionian;
        out.keyMask = (uint16_t) (1u << out.primaryRoot);
        out.modeMask = (uint16_t) (1u << (int) Mode::Ionian);
    }
    return out;
}

bool pitchInScale(int midi, int rootPc, Mode mode)
{
    bool inScale[12] = {};
    for (int iv : modeIntervals(mode))
        inScale[wrapPc(rootPc + iv)] = true;
    return inScale[wrapPc(midi)];
}

int noteNameIndex(const juce::String& name)
{
    for (int i = 0; i < 12; ++i)
        if (name == kNoteNames[(size_t) i])
            return i;
    return -1;
}

void refitNotesToScale(std::vector<RollNote>& notes, int rootPc, Mode mode)
{
    if (rootPc < 0 || notes.empty())
        return;
    for (auto& n : notes)
        n.pitch = juce::jlimit(0, 127, fitToScale(n.pitch, rootPc, mode));
}

int fitToScale(int midi, int rootPc, Mode mode)
{
    bool inScale[12] = {};
    for (int iv : modeIntervals(mode))
        inScale[wrapPc(rootPc + iv)] = true;

    if (inScale[wrapPc(midi)])
        return midi;

    int best = midi, bestDist = 99;
    for (int d = -6; d <= 6; ++d)
    {
        if (inScale[wrapPc(midi + d)] && std::abs(d) < bestDist)
        {
            bestDist = std::abs(d);
            best = midi + d;
        }
    }
    return best;
}

int foldToEnabledPitchClasses(int midi, uint16_t enabledMask, int rootPc, Mode mode)
{
    enabledMask = (uint16_t) (enabledMask & 0x0FFF);
    if (enabledMask == 0)
        return midi;

    const int pc = wrapPc(midi);
    if ((enabledMask & (uint16_t) (1u << pc)) != 0)
        return midi;

    bool preferScale = rootPc >= 0;
    bool scalePc[12] = {};
    if (preferScale)
        for (int iv : modeIntervals(mode))
            scalePc[wrapPc(rootPc + iv)] = true;

    int best = midi;
    int bestDist = 99;
    int bestScore = -1; // higher is better: in-scale preferred on ties
    for (int d = -6; d <= 6; ++d)
    {
        const int cand = midi + d;
        if (cand < 0 || cand > 127) continue;
        const int cpc = wrapPc(cand);
        if ((enabledMask & (uint16_t) (1u << cpc)) == 0) continue;
        const int dist = std::abs(d);
        const int score = (preferScale && scalePc[cpc]) ? 1 : 0;
        if (dist < bestDist || (dist == bestDist && score > bestScore)
            || (dist == bestDist && score == bestScore && cand < best))
        {
            bestDist = dist;
            bestScore = score;
            best = cand;
        }
    }
    return best;
}

juce::String scaleDegreeLabel(int midi, int rootPc, Mode mode)
{
    if (rootPc < 0) return {};
    static const char* kOrd[] { "Root", "2nd", "3rd", "4th", "5th", "6th", "7th" };
    const int pc = wrapPc(midi);
    const auto& iv = modeIntervals(mode);
    for (int deg = 0; deg < 7; ++deg)
        if (wrapPc(rootPc + iv[(size_t) deg]) == pc)
            return kOrd[deg];
    return {};
}

juce::String pitchScaleAnnotation(int midi, int rootPc, Mode mode)
{
    const auto note = pitchName(midi);
    if (rootPc < 0) return note;
    const auto deg = scaleDegreeLabel(midi, rootPc, mode);
    return deg.isNotEmpty() ? note + " (" + deg + ")" : note;
}

void applyPitchLock(const ClipEdit& locked, ClipEdit& target)
{
    target.octave = locked.octave;
    target.pitchShift = locked.pitchShift;
    target.octaveRange = locked.octaveRange;
    target.outsideRange = locked.outsideRange;
    target.pitchMin = locked.pitchMin;
    target.pitchMax = locked.pitchMax;
    target.fitScale = locked.fitScale;
    target.mapToRoot = locked.mapToRoot;
    target.root = locked.root;
    target.mode = locked.mode;
    target.noteFilterMask = locked.noteFilterMask;
    target.noteFilterType = locked.noteFilterType;
}

void applyPlaybackLock(const ClipEdit& locked, ClipEdit& target, bool autoTrim, const StepClip& clip)
{
    target.extendMult = locked.extendMult;
    if (!autoTrim)
    {
        target.clearTrim();
        return;
    }

    ClipEdit probe = target;
    probe.clearTrim();
    const auto preTrim = resolveClip(clip, probe);
    target.clearTrim();
    target.removedBars = emptyBars(preTrim.notes, clip.bars);
}

bool editIsClean(const ClipEdit& e)
{
    if (e.octave >= 0 || e.pitchShift != 0 || e.octaveRange != 0 || e.hasPitchRange()
        || e.extendMult > 1
        || e.fitScale || e.mapToRoot || e.hasNoteFilter() || e.hasTrim()
        || e.legacyTrimLead != 0 || e.legacyTrimTail != 0)
        return false;
    if (!e.velocities.empty() || !e.deleted.empty())
        return false;
    for (const auto& [id, mv] : e.moves)
        if (mv.dPitch != 0 || mv.dStep != 0)
            return false;
    return true;
}

static std::vector<int> effectiveRemovedBars(const StepClip& clip, const ClipEdit& e)
{
    if (!e.removedBars.empty())
        return e.removedBars;

    std::vector<int> removed;
    if (e.legacyTrimLead <= 0 && e.legacyTrimTail <= 0)
        return removed;

    for (int i = 0; i < e.legacyTrimLead && i < clip.bars; ++i)
        removed.push_back(i);
    const int firstTail = clip.bars - e.legacyTrimTail;
    for (int i = std::max(firstTail, e.legacyTrimLead); i < clip.bars; ++i)
        removed.push_back(i);
    return removed;
}

ResolvedClip resolveClip(const StepClip& clip, const ClipEdit& e)
{
    ResolvedClip out;
    out.notes.reserve(clip.notes.size());
    const int srcOct = clipReferenceOctave(clip);
    const int tgtOct = e.octave >= 0 ? juce::jlimit(0, 6, e.octave) : srcOct;
    const int octDelta = (tgtOct - srcOct) * 12;

    for (const auto& n : clip.notes)
    {
        if (e.deleted.count(n.id) > 0)
            continue;

        NoteMove mv;
        if (auto it = e.moves.find(n.id); it != e.moves.end())
            mv = it->second;

        // Octave Range owns the register: the window is absolute, so a second
        // transpose would move notes and then wrap them a second time.
        int pitch = n.pitch + mv.dPitch + e.pitchShift;
        if (e.octaveRange <= 0)
            pitch += octDelta;

        if (e.mapToRoot && e.root >= 0 && clip.root >= 0)
        {
            // Move the clip's scale root to the target root: a transpose, not
            // a snap, wrapped into [-6, 6] so it goes the nearest direction.
            int d = (e.root - clip.root) % 12;
            if (d > 6)  d -= 12;
            if (d < -6) d += 12;
            pitch += d;
        }

        if (e.fitScale && e.root >= 0)
            pitch = fitToScale(pitch, e.root, e.mode);

        pitch = juce::jlimit(0, 127, pitch);

        RollNote r = n;
        r.pitch = pitch;
        r.start = n.start + mv.dStep;
        r.moved = (mv.dPitch != 0 || mv.dStep != 0);
        if (auto vit = e.velocities.find(n.id); vit != e.velocities.end())
            r.fileVelocity = std::clamp(vit->second, 1, 127);
        out.notes.push_back(r);
    }

    // Octave Range: wrap or clamp every note into N octaves starting at the target C.
    if (e.octaveRange > 0 && !out.notes.empty())
    {
        for (auto& n : out.notes)
            n.pitch = mapPitchToOctaveWindow(n.pitch, tgtOct, e.octaveRange, e.outsideRange);
    }

    // Pitch Range: wrap notes into [pitchMin, pitchMax] by octave folds.
    if (e.hasPitchRange() && !out.notes.empty())
    {
        int lo = juce::jlimit(0, 127, e.pitchMin);
        int hi = juce::jlimit(0, 127, e.pitchMax);
        if (hi < lo) std::swap(lo, hi);
        const int window = hi - lo + 1;
        for (auto& n : out.notes)
        {
            if (window <= 12)
            {
                // Narrow window: keep pitch class, snap into range.
                int p = n.pitch;
                while (p > hi) p -= 12;
                while (p < lo) p += 12;
                n.pitch = juce::jlimit(lo, hi, p);
            }
            else
            {
                int p = n.pitch;
                while (p > hi) p -= 12;
                while (p < lo) p += 12;
                if (p < lo || p > hi)
                    p = juce::jlimit(lo, hi, n.pitch);
                n.pitch = p;
            }
        }
    }

    // Octave Range remaps chromatically and can pull notes out of scale — re-snap.
    if (e.fitScale && e.root >= 0)
        refitNotesToScale(out.notes, e.root, e.mode);

    // Note Filter: Mute drops disabled pitch-classes; Fold remaps to nearest kept.
    if (e.hasNoteFilter() && !out.notes.empty())
    {
        if (e.noteFilterType == NoteFilterType::Mute)
        {
            std::vector<RollNote> kept;
            kept.reserve(out.notes.size());
            for (const auto& n : out.notes)
                if (e.isNoteFilterEnabled(n.pitch))
                    kept.push_back(n);
            out.notes = std::move(kept);
        }
        else
        {
            for (auto& n : out.notes)
            {
                const int next = foldToEnabledPitchClasses(n.pitch, e.noteFilterMask, e.root, e.mode);
                if (next != n.pitch)
                {
                    n.pitch = next;
                    n.moved = true;
                }
            }
        }
    }

    const auto removed = effectiveRemovedBars(clip, e);
    int bars = clip.bars;
    if (!removed.empty())
    {
        for (auto& r : out.notes)
            r.start = remapStepAfterRemovingBars(r.start, removed);
        bars = clip.bars - (int) removed.size();
    }
    out.bars = std::max(1, bars);

    // Extend: tile the resolved clip so delays can evolve past the file length.
    const int mult = (e.extendMult == 2 || e.extendMult == 4 || e.extendMult == 8)
                   ? e.extendMult : 1;
    if (mult > 1 && !out.notes.empty())
    {
        const double loopSteps = (double) out.bars * (double) kStepsPerBar;
        int maxId = 0;
        for (const auto& n : out.notes)
            maxId = std::max(maxId, n.id);
        std::vector<RollNote> tiled = out.notes;
        tiled.reserve(out.notes.size() * (size_t) mult);
        for (int rep = 1; rep < mult; ++rep)
        {
            for (const auto& n : out.notes)
            {
                RollNote c = n;
                c.id = ++maxId;
                c.start = n.start + loopSteps * (double) rep;
                c.moved = true;
                tiled.push_back(c);
            }
        }
        out.notes = std::move(tiled);
        out.bars *= mult;
    }

    return out;
}

double remapStepAfterRemovingBars(double step, const std::vector<int>& removedBars)
{
    if (removedBars.empty())
        return step;

    const double barF = step / (double) kStepsPerBar;
    int removedBefore = 0;
    for (int b : removedBars)
    {
        if ((double) b < barF)
            ++removedBefore;
        else
            break;
    }
    return step - (double) removedBefore * (double) kStepsPerBar;
}

EdgeBars emptyEdgeBars(const std::vector<RollNote>& notes, int bars)
{
    if (notes.empty())
        return {};

    double minStep = std::numeric_limits<double>::infinity();
    double maxStep = -std::numeric_limits<double>::infinity();
    for (const auto& n : notes)
    {
        minStep = std::min(minStep, n.start);
        maxStep = std::max(maxStep, n.start + n.len);
    }

    EdgeBars e;
    e.lead = std::max(0, (int) std::floor(minStep / kStepsPerBar));
    e.tail = std::max(0, bars - (int) std::ceil(maxStep / kStepsPerBar));
    return e;
}

std::vector<int> emptyBars(const std::vector<RollNote>& notes, int bars)
{
    std::vector<int> out;
    if (bars <= 0)
        return out;

    for (int b = 0; b < bars; ++b)
    {
        const double ms = (double) b * (double) kStepsPerBar;
        const double me = (double) (b + 1) * (double) kStepsPerBar;
        bool hasNote = false;
        for (const auto& n : notes)
        {
            if (n.start < me && n.start + n.len > ms)
            {
                hasNote = true;
                break;
            }
        }
        if (!hasNote)
            out.push_back(b);
    }
    return out;
}

std::vector<EditBadge> editBadges(const StepClip& clip, const ClipEdit& e)
{
    std::vector<EditBadge> out;

    if (e.octave >= 0)
        out.push_back({ "oct", "Oct " + juce::String(e.octave) });

    if (e.octaveRange > 0)
        out.push_back({ "octRange", juce::String(e.octaveRange) + " oct "
                                    + (e.outsideRange == OutsideRangeMode::Clip ? "clip" : "fold") });

    if (e.extendMult > 1)
        out.push_back({ "extend", "x" + juce::String(e.extendMult) });

    if (e.hasPitchRange())
        out.push_back({ "range", pitchName(e.pitchMin) + "-" + pitchName(e.pitchMax) });

    if (e.hasNoteFilter())
    {
        int off = 0;
        for (int pc = 0; pc < 12; ++pc)
            if (!e.isNoteFilterEnabled(pc))
                ++off;
        const char* kind = e.noteFilterType == NoteFilterType::Fold ? "Fold" : "Mute";
        out.push_back({ "noteFilter", juce::String(kind) + " " + juce::String(off) });
    }

    if (e.fitScale && e.root >= 0)
        out.push_back({ "scale", juce::String(kNoteNames[(size_t) e.root]) + " " + modeName(e.mode) });

    if (e.mapToRoot && e.root >= 0)
        out.push_back({ "map", juce::String("-> ") + kNoteNames[(size_t) e.root] + " root" });

    int moved = 0;
    for (const auto& [id, mv] : e.moves)
        if (mv.dPitch != 0 || mv.dStep != 0)
            ++moved;
    if (moved > 0)
        out.push_back({ "moves", juce::String(moved) + (moved > 1 ? " notes moved" : " note moved") });

    const int trimBars = (int) effectiveRemovedBars(clip, e).size();
    if (trimBars > 0)
        out.push_back({ "trim", juce::String("Trim -") + juce::String(trimBars)
                                    + (trimBars > 1 ? " bars" : " bar") });

    if (!e.velocities.empty())
        out.push_back({ "vel", juce::String((int) e.velocities.size()) + " vel" });

    if (!e.deleted.empty())
        out.push_back({ "del", juce::String((int) e.deleted.size()) + " deleted" });

    return out;
}

namespace {

struct Polyphony
{
    int maxPoly = 1;
    double meanPoly = 1.0;
    bool frequentChords = false;
};

Polyphony measurePolyphony(const std::vector<NoteEvent>& notes)
{
    Polyphony out;
    if (notes.empty())
        return out;

    struct Ev
    {
        double t = 0.0;
        int delta = 0; // ends (-1) sort before starts (+1) at the same time
    };
    std::vector<Ev> evs;
    evs.reserve(notes.size() * 2);
    for (const auto& n : notes)
    {
        evs.push_back({ n.startBeat, 1 });
        evs.push_back({ n.startBeat + n.lengthBeats, -1 });
    }
    std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b)
    {
        if (a.t < b.t) return true;
        if (b.t < a.t) return false;
        return a.delta < b.delta;
    });

    int active = 0;
    int maxPoly = 1;
    double polySum = 0.0;
    int samples = 0;
    int chord3 = 0;
    // Sample once per note-on, after every note that starts at that instant is
    // active. That matches the old per-note count (a chord of 3 samples as 3, 3, 3).
    for (size_t i = 0; i < evs.size();)
    {
        const double t = evs[i].t;
        while (i < evs.size() && evs[i].t == t && evs[i].delta < 0)
        {
            active += evs[i].delta;
            ++i;
        }
        int starts = 0;
        while (i < evs.size() && evs[i].t == t && evs[i].delta > 0)
        {
            active += evs[i].delta;
            ++starts;
            ++i;
        }
        for (int s = 0; s < starts; ++s)
        {
            maxPoly = std::max(maxPoly, active);
            polySum += (double) active;
            ++samples;
            if (active >= 3)
                ++chord3;
        }
    }
    out.maxPoly = maxPoly;
    out.meanPoly = samples > 0 ? polySum / (double) samples : 1.0;
    out.frequentChords = samples > 0 && chord3 * 4 >= samples;
    return out;
}

enum class DrumVoice { Kick, Snare, Hat, Perc, Other };

DrumVoice classifyDrumVoice(int pitch)
{
    switch (pitch)
    {
        case 35: case 36: return DrumVoice::Kick;
        case 37: case 38: case 40: return DrumVoice::Snare;
        case 42: case 44: case 46: return DrumVoice::Hat;
        default:
            if (isGmDrumPitch(pitch))
                return DrumVoice::Perc;
            return DrumVoice::Other;
    }
}

/** Ableton/GM "C1" kick seat (MIDI 36). Also accept Acoustic Bass Drum 35. */
bool isKickC1Pitch(int midi)
{
    return midi == 35 || midi == 36;
}

/** Kick-like pattern: repeated short C1 hits landing on beats 1 and/or 3. */
bool hasKickPatternOnC1(const std::vector<RollNote>& notes)
{
    if (notes.empty())
        return false;

    int c1Hits = 0;
    int c1Downbeats = 0;
    int c1Short = 0;
    double firstC1 = -1.0;
    for (const auto& n : notes)
    {
        if (!isKickC1Pitch(n.pitch))
            continue;
        ++c1Hits;
        if (n.len <= 1.0)
            ++c1Short;
        if (firstC1 < 0.0)
            firstC1 = n.start;
        int slot = ((int) std::lround(n.start)) % kStepsPerBar;
        if (slot < 0) slot += kStepsPerBar;
        if (slot == 0 || slot == 8)
            ++c1Downbeats;
    }
    if (c1Hits < 2)
        return false;

    const double frac = (double) c1Hits / (double) notes.size();
    // Strong: several short C1 hits on the backbeat grid.
    if (c1Short >= 2 && c1Downbeats >= 1 && (frac >= 0.08 || c1Hits >= 3))
        return true;
    // Weaker: clip opens on C1 and keeps returning there rhythmically.
    if (firstC1 >= 0.0 && firstC1 < 0.75 && c1Downbeats >= 2 && c1Short >= 2)
        return true;
    return false;
}

void applyKind(StepClip& s, int meanPitch, int drumHits, int totalHits, const Polyphony& poly,
               int channel10Hits, const std::vector<RollNote>& notes)
{
    const double drumFrac = totalHits > 0 ? (double) drumHits / (double) totalHits : 0.0;
    s.hasDrumContent = channel10Hits > 0 || drumFrac >= 0.10;

    const ClipKind fileHint = hintKindFromFileName(
        s.filePath.isNotEmpty() ? s.filePath : s.name);
    const bool fileHinted = [&]()
    {
        // hintKindFromFileName returns Lead both for "lead" and for "no match".
        // Treat Lead as a real hint only when the name clearly says so.
        if (fileHint == ClipKind::Lead)
        {
            const auto tokens = fileNameTokens(
                s.filePath.isNotEmpty() ? s.filePath : s.name);
            for (const auto& t : tokens)
                if (tokenEquals(t, "lead") || tokenEquals(t, "melody") || tokenEquals(t, "melo")
                    || tokenEquals(t, "mel") || tokenEquals(t, "topline") || tokenEquals(t, "synth")
                    || tokenStartsWith(t, "lead"))
                    return true;
            return false;
        }
        return true;
    }();

    const bool chords = poly.frequentChords || poly.meanPoly >= 2.2 || poly.maxPoly >= 3;
    const bool mono = poly.maxPoly <= 1 && poly.meanPoly < 1.35;
    const bool kickC1 = hasKickPatternOnC1(notes);
    // Bass register roughly C1–C3 (Ableton numbering: 36–48) plus a little headroom.
    const bool bassRegister = meanPitch >= 24 && meanPitch <= 52;

    // 1) Strong musical drum signals, or a drum filename.
    // A bass filename blocks weak drumFrac matches (low notes can look like kicks).
    const bool drumByNotes = kickC1 || (drumFrac >= 0.70 && fileHint != ClipKind::Bass);
    if (drumByNotes || fileHint == ClipKind::Drums)
    {
        s.kind = ClipKind::Drums;
        if (fileHint == ClipKind::Drums && (kickC1 || drumFrac >= 0.40))
            s.kindConfidence = 96;
        else if (kickC1)
            s.kindConfidence = 90;
        else if (fileHint == ClipKind::Drums)
            s.kindConfidence = 88;
        else
            s.kindConfidence = drumFrac >= 0.90 ? 92 : 78;
        return;
    }

    // 2) Chords → keys/piano (filename can reinforce).
    if (chords || fileHint == ClipKind::Piano)
    {
        // Filename says bass but we clearly hear chords — trust the notes.
        if (fileHint == ClipKind::Bass && chords)
        {
            s.kind = ClipKind::Piano;
            s.kindConfidence = 72;
            return;
        }
        s.kind = ClipKind::Piano;
        if (fileHint == ClipKind::Piano && chords)
            s.kindConfidence = 95;
        else if (chords)
            s.kindConfidence = (poly.frequentChords || poly.meanPoly >= 3.0) ? 90 : 75;
        else
            s.kindConfidence = 82; // filename only
        return;
    }

    // 3) Bass: low octave, no chords (filename can reinforce or decide alone).
    if (fileHint == ClipKind::Bass || (mono && bassRegister))
    {
        s.kind = ClipKind::Bass;
        if (fileHint == ClipKind::Bass && mono && bassRegister)
            s.kindConfidence = 95;
        else if (fileHint == ClipKind::Bass)
            s.kindConfidence = 88;
        else if (meanPitch < 42)
            s.kindConfidence = 86;
        else
            s.kindConfidence = 70;
        return;
    }

    // 4) Explicit lead filename, or monophonic mid/high line.
    if (fileHinted || (mono && meanPitch > 52))
    {
        s.kind = ClipKind::Lead;
        s.kindConfidence = fileHinted ? 88 : 75;
        return;
    }

    s.kind = ClipKind::Lead;
    s.kindConfidence = 50;
}

double onsetSlotShare(const std::vector<RollNote>& notes)
{
    if (notes.empty()) return 0.0;
    std::set<int> slots;
    for (const auto& n : notes)
    {
        int slot = ((int) std::lround(n.start)) % kStepsPerBar;
        if (slot < 0) slot += kStepsPerBar;
        slots.insert(slot);
    }
    return (double) slots.size() / (double) kStepsPerBar;
}

double offbeatRatio(const std::vector<RollNote>& notes)
{
    if (notes.empty()) return 0.0;
    int off = 0;
    for (const auto& n : notes)
    {
        int slot = ((int) std::lround(n.start)) % kStepsPerBar;
        if (slot < 0) slot += kStepsPerBar;
        if ((slot % 2) != 0)
            ++off;
    }
    return (double) off / (double) notes.size();
}

int scoreClipComplexity(const std::vector<RollNote>& notes, int bars, double meanPoly, ClipKind kind)
{
    if (notes.empty())
        return 1;

    const double notesPerBar = (double) notes.size() / (double) juce::jmax(1, bars);
    const double density = std::log2(1.0 + notesPerBar) / std::log2(33.0);
    const double rhythm = onsetSlotShare(notes);
    const double syncop = offbeatRatio(notes);

    if (kind == ClipKind::Drums)
    {
        std::set<int> voices;
        int hatHits = 0, ghost = 0, finest = 4;
        for (const auto& n : notes)
        {
            voices.insert((int) classifyDrumVoice(n.pitch));
            if (classifyDrumVoice(n.pitch) == DrumVoice::Hat)
                ++hatHits;
            if (n.fileVelocity < 50)
                ++ghost;
            const int slot = ((int) std::lround(n.start * 2.0)) % (kStepsPerBar * 2);
            if ((slot % 2) != 0)
                finest = 1; // 32nd-ish grid activity
            else if ((slot % 2) == 0 && (((int) std::lround(n.start)) % 2) != 0)
                finest = std::min(finest, 2); // 16ths
        }
        const double voiceScore = juce::jlimit(0.0, 1.0, (double) voices.size() / 4.0);
        const double hatScore = notes.empty() ? 0.0
            : juce::jlimit(0.0, 1.0, (double) hatHits / (double) notes.size() * 2.0);
        const double ghostScore = juce::jlimit(0.0, 1.0, (double) ghost / (double) notes.size() * 3.0);
        const double subdiv = finest <= 1 ? 1.0 : (finest <= 2 ? 0.7 : 0.35);
        const double score = 100.0 * (0.28 * voiceScore
                                     + 0.22 * subdiv
                                     + 0.20 * syncop
                                     + 0.15 * ghostScore
                                     + 0.15 * hatScore);
        return juce::jlimit(1, 100, (int) std::lround(score));
    }

    if (kind == ClipKind::Piano)
    {
        // Chord size, extensions past triad, harmonic rhythm, attack syncopation.
        std::vector<RollNote> sorted = notes;
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const RollNote& a, const RollNote& b) { return a.start < b.start; });
        double chordSizeSum = 0.0;
        int chordGroups = 0;
        int extensionHits = 0;
        int chordChanges = 0;
        int prevBass = -1;
        for (size_t i = 0; i < sorted.size();)
        {
            const double t = sorted[i].start;
            std::vector<int> pcs;
            while (i < sorted.size() && std::abs(sorted[i].start - t) < 0.35)
            {
                pcs.push_back(wrapPc(sorted[i].pitch));
                ++i;
            }
            std::sort(pcs.begin(), pcs.end());
            pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
            const int size = (int) pcs.size();
            chordSizeSum += (double) size;
            ++chordGroups;
            if (size >= 4)
                ++extensionHits;
            if (!pcs.empty())
            {
                const int bass = pcs.front();
                if (prevBass >= 0 && bass != prevBass)
                    ++chordChanges;
                prevBass = bass;
            }
        }
        const double avgChord = chordGroups > 0 ? chordSizeSum / (double) chordGroups : 1.0;
        const double chordScore = juce::jlimit(0.0, 1.0, (avgChord - 1.0) / 4.0);
        const double extScore = chordGroups > 0
            ? juce::jlimit(0.0, 1.0, (double) extensionHits / (double) chordGroups)
            : 0.0;
        const double harmRhythm = juce::jlimit(0.0, 1.0,
            (double) chordChanges / (double) juce::jmax(1, bars * 2));
        const double score = 100.0 * (0.30 * chordScore
                                     + 0.25 * extScore
                                     + 0.25 * harmRhythm
                                     + 0.20 * syncop);
        return juce::jlimit(1, 100, (int) std::lround(score));
    }

    // Bass / Lead: pickups, interval variety, subdivision — not kit size.
    std::vector<const RollNote*> order;
    order.reserve(notes.size());
    for (const auto& n : notes)
        order.push_back(&n);
    std::sort(order.begin(), order.end(), [](const RollNote* a, const RollNote* b)
    {
        if (a->start < b->start) return true;
        if (b->start < a->start) return false;
        return a->pitch < b->pitch;
    });

    double intervalSum = 0.0;
    int intervalN = 0;
    int pickups = 0;
    int prevPitch = -1;
    double prevStart = -1.0e9;
    for (const auto* n : order)
    {
        int slot = ((int) std::lround(n->start)) % kStepsPerBar;
        if (slot < 0) slot += kStepsPerBar;
        if ((slot % 4) == 3 || (slot % 2) != 0)
            ++pickups;
        if (prevPitch >= 0 && n->start > prevStart + 0.01)
        {
            intervalSum += (double) std::abs(n->pitch - prevPitch);
            ++intervalN;
        }
        prevPitch = n->pitch;
        prevStart = n->start;
    }
    const double pickupScore = juce::jlimit(0.0, 1.0, (double) pickups / (double) notes.size());
    const double interval = intervalN > 0
        ? juce::jlimit(0.0, 1.0, (intervalSum / (double) intervalN) / 8.0)
        : 0.0;
    const double poly = juce::jlimit(0.0, 1.0, (meanPoly - 1.0) / 3.5);
    const double score = 100.0 * (0.30 * density
                                 + 0.25 * rhythm
                                 + 0.20 * pickupScore
                                 + 0.15 * interval
                                 + 0.10 * poly);
    return juce::jlimit(1, 100, (int) std::lround(score));
}

uint16_t analyseClipTags(const std::vector<RollNote>& notes, int bars, ClipKind kind)
{
    if (notes.empty())
        return 0;

    uint16_t tags = 0;
    const double notesPerBar = (double) notes.size() / (double) juce::jmax(1, bars);
    if (notesPerBar < 4.0)
        tags |= clipTag::Sparse;

    // Swing: notes clustering near the late half of each 8th pair.
    int swingish = 0, evenPairs = 0;
    for (const auto& n : notes)
    {
        const double inPair = std::fmod(n.start, 2.0);
        if (inPair > 0.55 && inPair < 1.45)
        {
            ++evenPairs;
            if (inPair > 0.85 && inPair < 1.25)
                ++swingish;
        }
    }
    if (evenPairs > 4 && swingish * 2 >= evenPairs)
        tags |= clipTag::Swing;

    if (kind == ClipKind::Drums)
    {
        int hat16 = 0, snare2 = 0, snare4 = 0;
        for (const auto& n : notes)
        {
            const auto v = classifyDrumVoice(n.pitch);
            int slot = ((int) std::lround(n.start)) % kStepsPerBar;
            if (slot < 0) slot += kStepsPerBar;
            if (v == DrumVoice::Hat && (slot % 2) != 0)
                ++hat16;
            if (v == DrumVoice::Snare)
            {
                if (slot == 4) ++snare2;
                if (slot == 12) ++snare4;
            }
        }
        if (hat16 >= juce::jmax(2, bars * 2))
            tags |= clipTag::SixteenthHats;
        if (snare2 > 0 && snare4 > 0)
            tags |= clipTag::Backbeat;
    }

    if (kind == ClipKind::Piano)
    {
        std::vector<RollNote> sorted = notes;
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const RollNote& a, const RollNote& b) { return a.start < b.start; });
        int seventhChords = 0, groups = 0;
        for (size_t i = 0; i < sorted.size();)
        {
            const double t = sorted[i].start;
            std::set<int> pcs;
            while (i < sorted.size() && std::abs(sorted[i].start - t) < 0.35)
            {
                pcs.insert(wrapPc(sorted[i].pitch));
                ++i;
            }
            if (pcs.size() < 2)
                continue;
            ++groups;
            // Four distinct pitch classes ≈ triad + 7th/9th.
            if (pcs.size() >= 4)
                ++seventhChords;
        }
        if (groups > 0 && seventhChords * 3 >= groups)
            tags |= clipTag::SeventhChords;
    }

    return tags;
}

} // namespace

StepClip makeStepClip(const MidiClip& clip)
{
    StepClip s;
    s.name = clip.name;
    s.filePath = clip.filePath;
    s.bpm = clip.bpm;
    s.timeSigNum = juce::jmax(1, clip.timeSigNum);
    s.timeSigDen = juce::jmax(1, clip.timeSigDen);

    constexpr double stepsPerBeat = 4.0;

    double maxEnd = 0.0;
    int pitchSum = 0;
    int drumHits = 0;
    int channel10Hits = 0;
    std::set<int> distinctPitches;

    int nextId = 0;
    s.notes.reserve(clip.notes.size());
    for (const auto& n : clip.notes)
    {
        RollNote r;
        r.id = nextId++;
        r.pitch = n.noteNumber;
        r.start = n.startBeat * stepsPerBeat;
        r.len = std::max(0.25, n.lengthBeats * stepsPerBeat);
        r.fileVelocity = n.velocity;
        r.channel = n.channel;
        s.notes.push_back(r);

        maxEnd = std::max(maxEnd, r.start + r.len);
        if (n.channel == 10)
            ++channel10Hits;
        // Count only clear kit voices (or channel 10). Bass notes also sit in
        // MIDI 36–48, so "any short GM pitch" false-positives basslines as drums.
        {
            const auto voice = classifyDrumVoice(n.noteNumber);
            const bool kitVoice = voice == DrumVoice::Kick
                               || voice == DrumVoice::Snare
                               || voice == DrumVoice::Hat
                               || (voice == DrumVoice::Perc && n.lengthBeats <= 0.35);
            if (n.channel == 10 || kitVoice)
                ++drumHits;
        }
        pitchSum += n.noteNumber;
        distinctPitches.insert(n.noteNumber);
    }

    const double lengthSteps = std::max(clip.lengthBeats * stepsPerBeat, maxEnd);
    s.bars = std::max(1, (int) std::ceil(lengthSteps / kStepsPerBar));
    s.noteCount = (int) s.notes.size();
    s.difNotes = (int) distinctPitches.size();

    if (!clip.notes.empty())
    {
        s.root = estimatePitchClassFromNotes(clip.notes);
        const int meanPitch = pitchSum / (int) clip.notes.size();
        const auto poly = measurePolyphony(clip.notes);
        applyKind(s, meanPitch, drumHits, (int) clip.notes.size(), poly, channel10Hits, s.notes);
        s.complexity = scoreClipComplexity(s.notes, s.bars, poly.meanPoly, s.kind);
        s.tagMask = analyseClipTags(s.notes, s.bars, s.kind);
    }
    else
    {
        s.complexity = 1;
        s.kindConfidence = 0;
        s.tagMask = 0;
    }
    s.notesResident = true;
    return s;
}

StepClip scanMidiBrowseMeta(const juce::File& file)
{
    StepClip s;
    s.name = file.getFileNameWithoutExtension();
    s.filePath = file.getFullPathName();
    s.notesResident = false;

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return s;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
        return s;
    midiFile.convertTimestampTicksToSeconds();

    double bpm = 120.0;
    int timeSigNum = 4;
    int timeSigDen = 4;
    if (midiFile.getNumTracks() > 0)
    {
        if (auto* track = midiFile.getTrack(0))
        {
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                auto& ev = track->getEventPointer(i)->message;
                if (ev.isTempoMetaEvent())
                {
                    bpm = 60.0 / ev.getTempoSecondsPerQuarterNote();
                    break;
                }
            }
        }
        for (int t = 0; t < midiFile.getNumTracks(); ++t)
        {
            auto* tr = midiFile.getTrack(t);
            if (tr == nullptr)
                continue;
            bool found = false;
            for (int i = 0; i < tr->getNumEvents(); ++i)
            {
                auto& ev = tr->getEventPointer(i)->message;
                if (ev.isTimeSignatureMetaEvent())
                {
                    int num = 4, den = 4;
                    ev.getTimeSignatureInfo(num, den);
                    timeSigNum = juce::jmax(1, num);
                    timeSigDen = juce::jmax(1, den);
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
    }

    s.bpm = bpm;
    s.timeSigNum = timeSigNum;
    s.timeSigDen = timeSigDen;
    const double secPerBeat = 60.0 / bpm;

    std::vector<NoteEvent> spans;
    double maxBeat = 0.0;
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        auto* trackPtr = midiFile.getTrack(t);
        if (trackPtr == nullptr)
            continue;
        juce::MidiMessageSequence track(*trackPtr);
        track.updateMatchedPairs();
        for (int i = 0; i < track.getNumEvents(); ++i)
        {
            auto* holder = track.getEventPointer(i);
            auto& msg = holder->message;
            if (!msg.isNoteOn())
                continue;
            NoteEvent ne;
            ne.noteNumber = msg.getNoteNumber();
            ne.channel = msg.getChannel();
            ne.startBeat = msg.getTimeStamp() / secPerBeat;
            if (holder->noteOffObject != nullptr)
                ne.lengthBeats = (holder->noteOffObject->message.getTimeStamp() - msg.getTimeStamp()) / secPerBeat;
            else
                ne.lengthBeats = 0.25;
            if (ne.lengthBeats < 0.0)
                ne.lengthBeats = 0.0;
            spans.push_back(ne);
            maxBeat = std::max(maxBeat, ne.startBeat + ne.lengthBeats);
        }
        maxBeat = std::max(maxBeat, track.getEndTime() / secPerBeat);
    }

    MidiClip shell;
    shell.bpm = bpm;
    shell.lengthBeats = maxBeat > 0.0 ? maxBeat : 4.0;
    shell.timeSigNum = timeSigNum;
    shell.timeSigDen = timeSigDen;
    shell.notes = std::move(spans);
    auto full = makeStepClip(shell);
    full.name = s.name;
    full.filePath = s.filePath;
    full.notes.clear();
    full.notes.shrink_to_fit();
    full.notesResident = false;
    return full;
}

} // namespace pflow
