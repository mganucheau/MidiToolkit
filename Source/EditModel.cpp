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
        case ClipKind::Keys:  return "Keys";
        case ClipKind::Lead:  return "Lead";
        case ClipKind::Arp:   return "Arp";
        case ClipKind::Drums: return "Drums";
        case ClipKind::Unsure: return "Unsure";
    }
    return "Unsure";
}

bool isGmDrumPitch(int midi)
{
    return midi >= 35 && midi <= 81;
}

bool isKickC1Pitch(int midi)
{
    // Ableton C1 / GM Bass Drum 1 = 36; Acoustic Bass Drum = 35.
    return midi == 35 || midi == 36;
}

bool fitsGmDrumStructure(const std::vector<RollNote>& notes)
{
    if (notes.empty())
        return false;

    int gm = 0, kick = 0, snare = 0, hat = 0, perc = 0, shortHits = 0;
    double earliest = 1.0e9;
    int earliestPitch = -1;

    for (const auto& n : notes)
    {
        if (n.start < earliest)
        {
            earliest = n.start;
            earliestPitch = n.pitch;
        }
        // RollNote lengths are in 16th-steps; kit hits are short (≤ half-beat).
        if (n.len <= 2.0)
            ++shortHits;

        if (!isGmDrumPitch(n.pitch))
            continue;
        ++gm;

        // Inline voice buckets (same map as classifyDrumVoice below).
        switch (n.pitch)
        {
            case 35: case 36: ++kick; break;
            case 37: case 38: case 40: ++snare; break;
            case 42: case 44: case 46: ++hat; break;
            default: ++perc; break;
        }
    }

    const int total = (int) notes.size();
    // Majority of hits must sit on the GM drum map.
    if (gm * 2 < total)
        return false;
    // Sustained low chords also sit on 36/40/43 — require drum-like lengths.
    if (shortHits * 2 < total)
        return false;
    if (kick < 1)
        return false;

    // Need another kit voice so a mono bassline parked on C2 (MIDI 36) fails.
    if (snare + hat + perc < 1)
        return false;

    // Must start on the kick at C1 (first sounding note is kick near t0).
    if (!isKickC1Pitch(earliestPitch) || earliest > 0.75)
        return false;

    return true;
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

std::optional<ClipKind> kindKeywordFromFileName(const juce::String& nameOrPath)
{
    if (nameOrPath.isEmpty())
        return std::nullopt;

    const auto tokens = fileNameTokens(nameOrPath);
    const auto lower = juce::File(nameOrPath).getFileNameWithoutExtension().toLowerCase();
    // Padded so " ba " / " sn " / " hh " style short tags match token boundaries.
    const auto padded = " " + lower.replaceCharacters("_-.", "   ") + " ";

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
    auto hasPadded = [&](const char* w)
    {
        return padded.containsIgnoreCase(juce::String(" ") + w + " ");
    };

    // Bass first so Deep_Bass_Groove stays Bass (not Drums via "groove").
    // "808" is treated as bass (sub/808 basslines), not kit percussion.
    if (hasToken("bassline") || hasToken("basslin") || hasToken("bass")
        || hasToken("bas") || hasToken("ba") || hasToken("sub") || hasToken("subbass")
        || hasToken("808") || hasTokenPrefix("bass")
        || hasPadded("ba") || hasPadded("bas") || hasPadded("808")
        || lower.contains("bassline") || lower.contains("bass_line") || lower.contains("bass-line")
        || lower.contains("subbass") || lower.contains("sub_bass")
        || lower.contains("bass") || lower.contains("808"))
        return ClipKind::Bass;

    // Drums / kit / percussion lexicon.
    // Keep this strict — words like "groove", "beat", "top", "rhythm", "fill"
    // appear on melodic packs and must not force Drums. Content still has to
    // look like a GM kit (kick at C1) before a drum keyword sticks on parse.
    if (hasToken("drums") || hasToken("drum") || hasToken("drumkit") || hasToken("loopkit")
        || hasToken("kit") || hasToken("drm") || hasToken("dr") || hasToken("perc")
        || hasToken("percussion") || hasToken("break") || hasToken("breaks")
        || hasToken("breakbeat") || hasToken("amen") || hasToken("boomBap") || hasToken("boombap")
        || hasToken("hats") || hasToken("hat") || hasToken("hh") || hasToken("hihat")
        || hasToken("hihats") || hasToken("highhat") || hasToken("kick") || hasToken("snare")
        || hasToken("clap") || hasToken("claps") || hasToken("rim") || hasToken("rimshot")
        || hasToken("conga") || hasToken("congas") || hasToken("djembe") || hasToken("tom")
        || hasToken("toms") || hasToken("ride") || hasToken("crash") || hasToken("cymbal")
        || hasToken("cymbals") || hasToken("shaker") || hasToken("tamb") || hasToken("tambourine")
        || hasToken("sn") || hasToken("kickdrum") || hasToken("snr")
        || hasPadded("sn") || hasPadded("hh") || hasPadded("dr") || hasPadded("drm")
        || hasPadded("kick") || hasPadded("kit")
        || lower.contains("drumkit") || lower.contains("drum_kit") || lower.contains("drum-kit")
        || lower.contains("hi-hat") || lower.contains("hi_hat") || lower.contains("highhat")
        || lower.contains("high-hat") || lower.contains("high_hat")
        || lower.contains("breakbeat") || lower.contains("boom-bap") || lower.contains("boom_bap"))
        return ClipKind::Drums;

    // Arp / sequencer — before Lead so "arp" doesn't fall through as melody.
    if (hasToken("arp") || hasToken("arps") || hasToken("arpeggio") || hasToken("arpeggios")
        || hasToken("arpeggiation") || hasToken("arppegio") || hasToken("arppegiation")
        || hasToken("arpseq") || hasToken("seq") || hasToken("sequencer") || hasToken("sq")
        || hasTokenPrefix("arp")
        || hasPadded("sq") || hasPadded("seq") || hasPadded("arp")
        || lower.contains("arpeggio") || lower.contains("arpeggi") || lower.contains("arppegio"))
        return ClipKind::Arp;

    // Keys / chords (formerly Piano).
    if (hasToken("piano") || hasToken("keys") || hasToken("key") || hasToken("keyz")
        || hasToken("ky") || hasToken("chord") || hasToken("chords") || hasToken("pad")
        || hasToken("pads") || hasToken("rhodes") || hasToken("epiano") || hasToken("ep")
        || hasTokenPrefix("key")
        || hasPadded("ky") || hasPadded("ep")
        || lower.contains("e-piano") || lower.contains("e_piano") || lower.contains("keys_")
        || lower.contains("piano") || lower.contains("chord"))
        return ClipKind::Keys;

    // Lead / synth / melody.
    if (hasToken("lead") || hasToken("ld") || hasToken("melody") || hasToken("melo")
        || hasToken("mel") || hasToken("motif") || hasToken("riff") || hasToken("topline")
        || hasToken("synth") || hasToken("sy") || hasToken("syn") || hasToken("hook")
        || hasToken("pluck") || hasToken("plk")
        || hasTokenPrefix("lead") || hasTokenPrefix("mel") || hasTokenPrefix("synth")
        || hasPadded("ld") || hasPadded("sy") || hasPadded("syn") || hasPadded("plk")
        || lower.contains("synth") || lower.contains("melody") || lower.contains("pluck"))
        return ClipKind::Lead;

    return std::nullopt;
}

ClipKind hintKindFromFileName(const juce::String& nameOrPath)
{
    if (auto k = kindKeywordFromFileName(nameOrPath))
        return *k;
    return ClipKind::Lead;
}

bool applyFilenameKindTag(StepClip& clip)
{
    if (clip.kindPinned)
        return false;
    // Content scoring already decided, including an explicit Unsure.
    if (clip.notesResident && !clip.notes.empty())
        return false;
    if (!clip.notesResident && clip.kindConfidence >= kKindConfidenceThreshold
        && clip.kind != ClipKind::Unsure)
        return false;
    if (clip.kind == ClipKind::Unsure && clip.kindConfidence > 0)
        return false;

    const juce::String src = clip.filePath.isNotEmpty() ? clip.filePath : clip.name;
    auto tagged = kindKeywordFromFileName(src);
    if (!tagged && clip.name.isNotEmpty() && clip.name != src)
        tagged = kindKeywordFromFileName(clip.name);
    if (!tagged)
        return false;

    // Never let a drum keyword overwrite a content-analyzed non-drum kind when
    // resident notes are present but don't look like a GM kit.
    if (*tagged == ClipKind::Drums
        && clip.notesResident
        && !clip.notes.empty()
        && !fitsGmDrumStructure(clip.notes))
        return false;

    // Indexed browse stubs already carry makeStepClip's kind. A drum keyword
    // must not clobber that when notes aren't loaded here.
    if (*tagged == ClipKind::Drums
        && !clip.notesResident
        && clip.kind != ClipKind::Drums
        && clip.kindConfidence >= 70)
        return false;

    clip.kind = *tagged;
    clip.kindConfidence = juce::jmax(clip.kindConfidence, 95);
    if (clip.kind == ClipKind::Drums)
        clip.hasDrumContent = true;
    return true;
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

    // Per-root coverage / certainty / suggested mode (key-chip hover readout).
    {
        double w[12] = {};
        double total = 0.0;
        for (const auto& n : clip.notes)
        {
            const double len = juce::jmax(0.25, n.len);
            w[wrapPc(n.pitch)] += len;
            total += len;
        }

        double bestEvidence[12] = {};
        for (int root = 0; root < 12; ++root)
        {
            double bestCov = -1.0, bestEv = 0.0, bestRank = -1.0;
            int bestMode = (int) Mode::Ionian;
            for (int mi = 0; mi < kNumModes; ++mi)
            {
                const auto& iv = modeIntervals((Mode) mi);
                bool scalePc[12] = {};
                for (int step : iv)
                    scalePc[wrapPc(root + step)] = true;

                double inside = 0.0;
                for (int pc = 0; pc < 12; ++pc)
                    if (scalePc[pc]) inside += w[pc];
                const double cov = total > 0.0 ? inside / total : 0.0;
                const double ev = total > 0.0
                    ? (w[root] + 0.5 * (w[wrapPc(root + iv[2])] + w[wrapPc(root + iv[4])])) / total
                    : 0.0;
                // Same-coverage modes: strongest tonic/third/fifth emphasis, then
                // the common Major / Minor reading.
                const double rank = ev + ((mi == (int) Mode::Ionian || mi == (int) Mode::Aeolian) ? 0.02 : 0.0);
                if (cov > bestCov + 1.0e-9 || (std::abs(cov - bestCov) <= 1.0e-9 && rank > bestRank))
                {
                    bestCov = cov;
                    bestEv = ev;
                    bestRank = rank;
                    bestMode = mi;
                }
            }
            out.rootCoverage[(size_t) root] = juce::jlimit(0, 100, (int) std::lround(100.0 * bestCov));
            out.rootMode[(size_t) root] = (Mode) bestMode;
            bestEvidence[root] = bestEv;
        }

        double maxEv = 0.0;
        for (double e : bestEvidence) maxEv = juce::jmax(maxEv, e);
        for (int root = 0; root < 12; ++root)
        {
            const double cov = out.rootCoverage[(size_t) root] / 100.0;
            const double rel = maxEv > 0.0 ? bestEvidence[root] / maxEv : 1.0;
            out.rootCertainty[(size_t) root] =
                juce::jlimit(0, 100, (int) std::lround(100.0 * cov * (0.5 + 0.5 * rel)));
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

void refitNotesToScale(std::vector<RollNote>& notes, int rootPc, Mode mode,
                       const NoteProcessScope& scope)
{
    if (rootPc < 0 || notes.empty())
        return;
    for (auto& n : notes)
        if (scope.includes(n))
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
    target.pitchInput = locked.pitchInput;
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
    target.drumMap = locked.drumMap;
}

void applyPlaybackLock(const ClipEdit& locked, ClipEdit& target, bool autoTrim, const StepClip& clip)
{
    target.extendMult = locked.extendMult;
    target.reverse = locked.reverse;
    target.invert = locked.invert;
    target.selectedNotesOnly = locked.selectedNotesOnly;
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
        || e.extendMult > 1 || e.reverse || e.invert || e.selectedNotesOnly
        || e.fitScale || e.mapToRoot || e.drumMap || e.hasNoteFilter() || e.hasTrim()
        || e.legacyTrimLead != 0 || e.legacyTrimTail != 0)
        return false;
    if (!e.velocities.empty() || !e.deleted.empty())
        return false;
    for (const auto& [id, mv] : e.moves)
        if (mv.dPitch != 0 || mv.dStep != 0 || mv.dLen != 0.0)
            return false;
    return true;
}

namespace {

bool pitchStampIsAutomatic(const NotePitchSettings& s)
{
    return !s.fitScale && !s.mapToRoot && !s.drumMap
        && s.pitchShift == 0 && s.octave < 0
        && s.pitchMin <= 0 && s.pitchMax >= 127
        && s.noteFilterType == NoteFilterType::Off
        && (s.noteFilterMask & 0x0FFF) == 0x0FFF
        && !s.reverse && !s.invert;
}

bool pitchEditIsUserSet(const ClipEdit& e)
{
    return e.fitScale || e.mapToRoot || e.drumMap || e.hasNoteFilter()
        || e.noteFilterType != NoteFilterType::Off
        || e.pitchShift != 0 || e.octave >= 0 || e.hasPitchRange()
        || e.reverse || e.invert;
}

void clearAutomaticPitchStamps(ClipEdit& e)
{
    bool allAutomatic = !e.notePitch.empty();
    for (const auto& entry : e.notePitch)
    {
        if (!pitchStampIsAutomatic(entry.second))
        {
            allAutomatic = false;
            break;
        }
    }
    if (allAutomatic)
        e.notePitch.clear();
}

} // namespace

void prepareBrowsePitchEdit(ClipEdit& e, const StepClip& clip)
{
    if (pitchEditIsUserSet(e))
        return;

    const int analyzedSpan = analyseClipScale(clip).octaveRange;
    // The old browse seed copied that span into Octave Range, which turns Fold
    // on. A kit wider than the 3-octave cap then wraps hats and cymbals onto
    // kick and snare pitches. Off unless the user picked a different window.
    if (e.octaveRange == 0 || e.octaveRange == analyzedSpan)
        e.octaveRange = 0;

    if (clip.kind == ClipKind::Drums)
    {
        e.octave = -1;
        e.root = -1;
        e.mode = Mode::Ionian;
        clearAutomaticPitchStamps(e);
        return;
    }

    if (e.root < 0)
    {
        const auto analysis = analyseClipScale(clip);
        e.octave = -1;
        e.root = analysis.primaryRoot;
        e.mode = analysis.primaryMode;
    }
    clearAutomaticPitchStamps(e);
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

/** Semitone shift that puts this clip's kick on C1 (MIDI 36). 0 when there is no kick. */
static int drumKickToC1Shift(const std::vector<RollNote>& notes)
{
    if (notes.empty())
        return 0;

    struct Stat
    {
        int count = 0;
        int kickW = 0;
        int snareW = 0;
        double lenSum = 0.0;
    };
    std::map<int, Stat> byPitch;
    for (const auto& n : notes)
    {
        auto& s = byPitch[n.pitch];
        ++s.count;
        s.lenSum += n.len;
        const int slot = ((int) std::lround(n.start) % kStepsPerBar + kStepsPerBar) % kStepsPerBar;
        if (slot == 0) s.kickW += 2;
        else if (slot == 8) s.kickW += 1;
        else if (slot == 4 || slot == 12) s.snareW += 2;
    }

    const int gm35 = byPitch.count(35) ? byPitch[35].count : 0;
    const int gm36 = byPitch.count(36) ? byPitch[36].count : 0;
    // GM Bass Drum. C1 (36) wins a tie so a kit that is already home does not move.
    if (gm35 + gm36 > 0)
    {
        const int kick = (gm36 >= gm35 && gm36 > 0) ? 36 : 35;
        return 36 - kick;
    }

    int busiest = -1;
    int busiestCount = 0;
    int lowest = 127;
    for (const auto& [pitch, s] : byPitch)
    {
        lowest = std::min(lowest, pitch);
        if (s.count > busiestCount)
        {
            busiest = pitch;
            busiestCount = s.count;
        }
    }

    auto hatOrSnare = [&](int pitch, const Stat& s)
    {
        const double avgLen = s.lenSum / (double) std::max(1, s.count);
        const double kickR = (double) s.kickW / (double) std::max(1, s.count);
        const double snareR = (double) s.snareW / (double) std::max(1, s.count);
        const bool gmHat = pitch == 42 || pitch == 44 || pitch == 46;
        const bool gmSnare = pitch == 37 || pitch == 38 || pitch == 39 || pitch == 40;
        const bool hatLike = gmHat
            || (pitch == busiest && avgLen <= 1.5 && s.count >= 4 && kickR < 0.5)
            || (avgLen <= 1.0 && s.count >= 4 && kickR < 0.35 && snareR < 0.35);
        const bool snareLike = gmSnare || (snareR >= 0.5 && snareR > kickR);
        return hatLike || snareLike;
    };

    int bestPitch = -1;
    double best = 0.0;
    for (const auto& [pitch, s] : byPitch)
    {
        if (s.count <= 0 || hatOrSnare(pitch, s))
            continue;
        const double kickR = (double) s.kickW / (double) s.count;
        const double snareR = (double) s.snareW / (double) s.count;
        const double score = kickR - snareR;
        if (score > best)
        {
            best = score;
            bestPitch = pitch;
        }
    }
    if (bestPitch >= 0 && best >= 0.35)
        return 36 - bestPitch;

    // No clear downbeat kick. Use the lowest note only when it is not a snare or hat.
    if (lowest < 127 && byPitch.count(lowest) && !hatOrSnare(lowest, byPitch[lowest]))
        return 36 - lowest;
    return 0;
}

ResolvedClip resolveClip(const StepClip& clip, const ClipEdit& e,
                         const std::set<int>* selectedNoteIds,
                         NoteProcessScope scope)
{
    ResolvedClip out;
    out.notes.reserve(clip.notes.size());
    const int srcOct = clipReferenceOctave(clip);
    const int tgtOct = e.octave >= 0 ? juce::jlimit(0, 6, e.octave) : srcOct;
    const int drumShift = e.drumMap ? drumKickToC1Shift(clip.notes) : 0;

    // Loop brackets live in the post-trim roll. Decide scope with that start,
    // even while the notes below still carry pre-trim positions.
    NoteProcessScope effective = scope;
    if (effective.selectedIds == nullptr && e.selectedNotesOnly)
        effective.selectedIds = selectedNoteIds;
    const auto removed = effectiveRemovedBars(clip, e);
    auto inScope = [&](const RollNote& n, bool alreadyTrimmed)
    {
        RollNote probe = n;
        if (!alreadyTrimmed && !removed.empty())
            probe.start = remapStepAfterRemovingBars(n.start, removed);
        return effective.includes(probe);
    };

    for (const auto& n : clip.notes)
    {
        if (e.deleted.count(n.id) > 0)
            continue;

        NoteMove mv;
        if (auto it = e.moves.find(n.id); it != e.moves.end())
            mv = it->second;

        // Manual drags always apply. Toolkit pitch touches the loop (and the
        // selection, when there is one) until a note has its own settings.
        // After that, only notes that were stamped are affected.
        RollNote probe = n;
        probe.id = n.id;
        probe.start = n.start + mv.dStep;
        const NotePitchSettings* per = nullptr;
        if (!e.notePitch.empty())
        {
            if (const auto it = e.notePitch.find(n.id); it != e.notePitch.end())
                per = &it->second;
        }
        const bool hit = e.notePitch.empty() ? inScope(probe, false) : (per != nullptr);
        const int useShift = per != nullptr ? per->pitchShift : e.pitchShift;
        const int useOct = per != nullptr ? per->octave : e.octave;
        const int useOctRange = per != nullptr ? per->octaveRange : e.octaveRange;
        const bool useMap = per != nullptr ? per->mapToRoot : e.mapToRoot;
        const int useRoot = per != nullptr ? per->root : e.root;
        const bool useFit = per != nullptr ? per->fitScale : e.fitScale;
        const Mode useMode = per != nullptr ? per->mode : e.mode;
        const int useTgt = useOct >= 0 ? juce::jlimit(0, 6, useOct) : srcOct;
        const int useDelta = (useTgt - srcOct) * 12;

        int pitch = n.pitch + mv.dPitch;
        if (e.drumMap)
        {
            // Whole kit moves together. Scale, octave, and the key filter stay off.
            pitch += drumShift;
        }
        else if (hit)
        {
            pitch += useShift;
            // Octave Range owns the register: the window is absolute, so a second
            // transpose would move notes and then wrap them a second time.
            if (useOctRange <= 0)
                pitch += useDelta;

            if (useMap && useRoot >= 0 && clip.root >= 0)
            {
                int d = (useRoot - clip.root) % 12;
                if (d > 6)  d -= 12;
                if (d < -6) d += 12;
                pitch += d;
            }

            if (useFit && useRoot >= 0)
                pitch = fitToScale(pitch, useRoot, useMode);
        }

        pitch = juce::jlimit(0, 127, pitch);

        RollNote r = n;
        r.pitch = pitch;
        r.start = n.start + mv.dStep;
        if (mv.dLen != 0.0)
            r.len = std::max(0.25, n.len + mv.dLen);
        r.moved = (mv.dPitch != 0 || mv.dStep != 0 || mv.dLen != 0.0);
        if (auto vit = e.velocities.find(n.id); vit != e.velocities.end())
            r.fileVelocity = std::clamp(vit->second, 1, 127);
        out.notes.push_back(r);
    }

    // Octave Range: wrap or clamp every note into N octaves starting at the target C.
    if (!e.drumMap && !e.notePitch.empty())
    {
        for (auto& n : out.notes)
        {
            const auto it = e.notePitch.find(n.id);
            if (it == e.notePitch.end() || it->second.octaveRange <= 0)
                continue;
            const int tgt = it->second.octave >= 0 ? juce::jlimit(0, 6, it->second.octave) : srcOct;
            n.pitch = mapPitchToOctaveWindow(n.pitch, tgt, it->second.octaveRange, it->second.outsideRange);
        }
    }
    else if (!e.drumMap && e.octaveRange > 0 && !out.notes.empty())
    {
        for (auto& n : out.notes)
        {
            if (!inScope(n, false))
                continue;
            n.pitch = mapPitchToOctaveWindow(n.pitch, tgtOct, e.octaveRange, e.outsideRange);
        }
    }

    // Pitch Range: wrap notes into [pitchMin, pitchMax] by octave folds.
    if (!e.drumMap && !e.notePitch.empty())
    {
        for (auto& n : out.notes)
        {
            const auto it = e.notePitch.find(n.id);
            if (it == e.notePitch.end())
                continue;
            int lo = juce::jlimit(0, 127, it->second.pitchMin);
            int hi = juce::jlimit(0, 127, it->second.pitchMax);
            if (lo <= 0 && hi >= 127)
                continue;
            if (hi < lo) std::swap(lo, hi);
            const int window = hi - lo + 1;
            int p = n.pitch;
            while (p > hi) p -= 12;
            while (p < lo) p += 12;
            n.pitch = window <= 12 ? juce::jlimit(lo, hi, p)
                                   : (p < lo || p > hi ? juce::jlimit(lo, hi, n.pitch) : p);
        }
    }
    else if (!e.drumMap && e.hasPitchRange() && !out.notes.empty())
    {
        int lo = juce::jlimit(0, 127, e.pitchMin);
        int hi = juce::jlimit(0, 127, e.pitchMax);
        if (hi < lo) std::swap(lo, hi);
        const int window = hi - lo + 1;
        for (auto& n : out.notes)
        {
            if (!inScope(n, false))
                continue;
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
    if (!e.drumMap && !e.notePitch.empty())
    {
        for (auto& n : out.notes)
        {
            const auto it = e.notePitch.find(n.id);
            if (it == e.notePitch.end() || !it->second.fitScale || it->second.root < 0)
                continue;
            n.pitch = juce::jlimit(0, 127, fitToScale(n.pitch, it->second.root, it->second.mode));
        }
    }
    else if (!e.drumMap && e.fitScale && e.root >= 0)
    {
        for (auto& n : out.notes)
            if (inScope(n, false))
                n.pitch = juce::jlimit(0, 127, fitToScale(n.pitch, e.root, e.mode));
    }

    // Note Filter: Off keeps every pitch. Mute drops disabled classes; Fold remaps them.
    if (!e.drumMap && !e.notePitch.empty())
    {
        std::vector<RollNote> kept;
        kept.reserve(out.notes.size());
        for (auto n : out.notes)
        {
            const auto it = e.notePitch.find(n.id);
            if (it == e.notePitch.end()
                || it->second.noteFilterType == NoteFilterType::Off
                || (it->second.noteFilterMask & 0x0FFF) == 0x0FFF)
            {
                kept.push_back(n);
                continue;
            }
            const auto& s = it->second;
            const int pc = ((n.pitch % 12) + 12) % 12;
            const bool enabled = (s.noteFilterMask & (uint16_t) (1u << pc)) != 0;
            if (s.noteFilterType == NoteFilterType::Mute)
            {
                if (enabled)
                    kept.push_back(n);
                continue;
            }
            if (!enabled)
            {
                const int next = foldToEnabledPitchClasses(n.pitch, s.noteFilterMask, s.root, s.mode);
                if (next != n.pitch)
                {
                    n.pitch = next;
                    n.moved = true;
                }
            }
            kept.push_back(n);
        }
        out.notes = std::move(kept);
    }
    else if (!e.drumMap && e.noteFilterType != NoteFilterType::Off && e.hasNoteFilter() && !out.notes.empty())
    {
        if (e.noteFilterType == NoteFilterType::Mute)
        {
            std::vector<RollNote> kept;
            kept.reserve(out.notes.size());
            for (const auto& n : out.notes)
                if (!inScope(n, false) || e.isNoteFilterEnabled(n.pitch))
                    kept.push_back(n);
            out.notes = std::move(kept);
        }
        else
        {
            for (auto& n : out.notes)
            {
                if (!inScope(n, false))
                    continue;
                const int next = foldToEnabledPitchClasses(n.pitch, e.noteFilterMask, e.root, e.mode);
                if (next != n.pitch)
                {
                    n.pitch = next;
                    n.moved = true;
                }
            }
        }
    }

    int bars = clip.bars;
    if (!removed.empty())
    {
        for (auto& r : out.notes)
            r.start = remapStepAfterRemovingBars(r.start, removed);
        bars = clip.bars - (int) removed.size();
    }
    out.bars = std::max(1, bars);

    auto formTargets = [&](const RollNote& n) -> bool
    {
        return inScope(n, true);
    };

    // Invert: mirror pitches around the midpoint of the (targeted) range.
    if (!e.drumMap && !e.notePitch.empty())
    {
        int lo = 127, hi = 0, count = 0;
        for (const auto& n : out.notes)
        {
            const auto it = e.notePitch.find(n.id);
            if (it == e.notePitch.end() || !it->second.invert) continue;
            lo = std::min(lo, n.pitch);
            hi = std::max(hi, n.pitch);
            ++count;
        }
        if (count > 0)
        {
            const int pivot = lo + hi;
            for (auto& n : out.notes)
            {
                const auto it = e.notePitch.find(n.id);
                if (it == e.notePitch.end() || !it->second.invert) continue;
                n.pitch = juce::jlimit(0, 127, pivot - n.pitch);
                n.moved = true;
            }
        }
    }
    else if (!e.drumMap && e.invert && !out.notes.empty())
    {
        int lo = 127, hi = 0;
        int count = 0;
        for (const auto& n : out.notes)
        {
            if (!formTargets(n)) continue;
            lo = std::min(lo, n.pitch);
            hi = std::max(hi, n.pitch);
            ++count;
        }
        if (count > 0)
        {
            const int pivot = lo + hi;
            for (auto& n : out.notes)
            {
                if (!formTargets(n)) continue;
                n.pitch = juce::jlimit(0, 127, pivot - n.pitch);
                n.moved = true;
            }
        }
    }

    // Reverse: flip note starts within the post-trim length.
    if (!e.notePitch.empty())
    {
        const double total = (double) out.bars * (double) kStepsPerBar;
        for (auto& n : out.notes)
        {
            const auto it = e.notePitch.find(n.id);
            if (it == e.notePitch.end() || !it->second.reverse) continue;
            n.start = juce::jmax(0.0, total - n.start - n.len);
            n.moved = true;
        }
    }
    else if (e.reverse && !out.notes.empty())
    {
        const double total = (double) out.bars * (double) kStepsPerBar;
        const bool looped = effective.loopEndStep < 1.0e11;
        const double spanA = looped ? effective.loopStartStep : 0.0;
        const double spanB = looped ? effective.loopEndStep : total;
        for (auto& n : out.notes)
        {
            if (!formTargets(n)) continue;
            n.start = juce::jmax(0.0, (spanA + spanB) - n.start - n.len);
            n.moved = true;
        }
    }

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

    if (e.reverse)
        out.push_back({ "reverse", "Reverse" });

    if (e.invert)
        out.push_back({ "invert", "Invert" });

    if (e.hasPitchRange())
        out.push_back({ "range", pitchName(e.pitchMin) + "-" + pitchName(e.pitchMax) });

    if (e.drumMap)
        out.push_back({ "drums", "Drums → C1" });

    if (e.noteFilterType != NoteFilterType::Off && e.hasNoteFilter())
    {
        int off = 0;
        for (int pc = 0; pc < 12; ++pc)
            if (!e.isNoteFilterEnabled(pc))
                ++off;
        const char* kind = e.noteFilterType == NoteFilterType::Fold ? "Fold"
                          : e.noteFilterType == NoteFilterType::Mute ? "Mute" : "Off";
        out.push_back({ "noteFilter", juce::String(kind) + " " + juce::String(off) });
    }

    if (e.fitScale && e.root >= 0)
        out.push_back({ "scale", juce::String(kNoteNames[(size_t) e.root]) + " " + modeName(e.mode) });

    if (e.mapToRoot && e.root >= 0)
        out.push_back({ "map", juce::String("-> ") + kNoteNames[(size_t) e.root] + " root" });

    int moved = 0;
    for (const auto& [id, mv] : e.moves)
        if (mv.dPitch != 0 || mv.dStep != 0 || mv.dLen != 0.0)
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

/** True when a short pitch cycle repeats and spells at least a triad. */
bool repeatingChordCycle(const std::vector<RollNote>& notes, bool& steadyGrid)
{
    steadyGrid = false;
    if (notes.size() < 6)
        return false;

    std::vector<const RollNote*> ordered;
    ordered.reserve(notes.size());
    for (const auto& n : notes)
        ordered.push_back(&n);
    std::sort(ordered.begin(), ordered.end(), [](const RollNote* a, const RollNote* b)
    {
        if (a->start != b->start) return a->start < b->start;
        return a->pitch < b->pitch;
    });

    std::vector<double> ioi;
    ioi.reserve(ordered.size());
    for (size_t i = 1; i < ordered.size(); ++i)
    {
        const double d = ordered[i]->start - ordered[i - 1]->start;
        if (d > 0.05)
            ioi.push_back(d);
    }
    if (ioi.size() >= 4)
    {
        double mean = 0.0;
        for (double d : ioi) mean += d;
        mean /= (double) ioi.size();
        double var = 0.0;
        for (double d : ioi) var += (d - mean) * (d - mean);
        var /= (double) ioi.size();
        const double cv = mean > 0.01 ? std::sqrt(var) / mean : 1.0;
        steadyGrid = cv < 0.28;
    }

    for (int period = 2; period <= 6; ++period)
    {
        if ((int) ordered.size() < period * 2)
            continue;
        int matches = 0, compared = 0;
        bool seen[12] = {};
        int pcs = 0;
        for (int i = 0; i < period; ++i)
        {
            const int pc = ((ordered[(size_t) i]->pitch % 12) + 12) % 12;
            if (!seen[pc]) { seen[pc] = true; ++pcs; }
        }
        if (pcs < 3)
            continue;
        for (int i = period; i < (int) ordered.size(); ++i)
        {
            ++compared;
            if (ordered[(size_t) i]->pitch == ordered[(size_t) (i - period)]->pitch)
                ++matches;
        }
        if (compared > 0 && matches * 5 >= compared * 4)
            return true;
    }
    return false;
}

void applyKind(StepClip& s, int meanPitch, int drumHits, int totalHits, const Polyphony& poly,
               int channel10Hits, const std::vector<RollNote>& notes)
{
    const double drumFrac = totalHits > 0 ? (double) drumHits / (double) totalHits : 0.0;
    s.hasDrumContent = channel10Hits > 0 || drumFrac >= 0.10;
    if (notes.empty())
    {
        s.kind = ClipKind::Unsure;
        s.kindConfidence = 0;
        return;
    }

    const bool gmKit = fitsGmDrumStructure(notes);
    const bool mono = poly.maxPoly <= 1 && poly.meanPoly < 1.35;
    const double ch10Frac = totalHits > 0 ? (double) channel10Hits / (double) totalHits : 0.0;

    int gm = 0, kick = 0, snare = 0, hat = 0, shortHits = 0;
    for (const auto& n : notes)
    {
        if (n.len <= 2.0)
            ++shortHits;
        if (!isGmDrumPitch(n.pitch))
            continue;
        ++gm;
        const auto voice = classifyDrumVoice(n.pitch);
        if (voice == DrumVoice::Kick) ++kick;
        else if (voice == DrumVoice::Snare) ++snare;
        else if (voice == DrumVoice::Hat) ++hat;
    }
    const int total = (int) notes.size();
    // Snare or hat, not a tom an octave above the kick. Octave basslines sit on 36 and 48.
    const bool kitVoices = snare + hat >= 1;
    const bool gmCluster = total > 0 && gm * 2 >= total && kick >= 1 && kitVoices
                        && shortHits * 2 >= total;

    int chordOnsets = 0, onsets = 0;
    {
        std::vector<double> starts;
        starts.reserve(notes.size());
        for (const auto& n : notes)
            starts.push_back(n.start);
        std::sort(starts.begin(), starts.end());
        for (size_t i = 0; i < starts.size();)
        {
            const double t = starts[i];
            int stacked = 0;
            while (i < starts.size() && std::abs(starts[i] - t) < 0.35)
            {
                ++stacked;
                ++i;
            }
            ++onsets;
            if (stacked >= 3)
                ++chordOnsets;
        }
    }
    const double chordOnsetFrac = onsets > 0 ? (double) chordOnsets / (double) onsets : 0.0;
    // Chords are notes that start together. Length is not part of the test,
    // so short stabs and long pads both land here.
    const bool together = poly.frequentChords || poly.maxPoly >= 3 || chordOnsetFrac >= 0.45;

    bool steady = false;
    const bool cycle = mono && repeatingChordCycle(notes, steady);

    int stepwise = 0, leaps = 0;
    double lenVar = 0.0;
    {
        std::vector<const RollNote*> ordered;
        for (const auto& n : notes)
            ordered.push_back(&n);
        std::sort(ordered.begin(), ordered.end(), [](const RollNote* a, const RollNote* b)
        {
            return a->start < b->start;
        });
        double lenMean = 0.0;
        for (const auto* n : ordered)
            lenMean += n->len;
        lenMean = ordered.empty() ? 0.0 : lenMean / (double) ordered.size();
        for (size_t i = 0; i < ordered.size(); ++i)
        {
            lenVar += std::abs(ordered[i]->len - lenMean);
            if (i == 0) continue;
            const int iv = std::abs(ordered[i]->pitch - ordered[i - 1]->pitch);
            if (iv > 0 && iv <= 2) ++stepwise;
            if (iv >= 7) ++leaps;
        }
        if (!ordered.empty())
            lenVar /= (double) ordered.size();
    }

    int drums = 8;
    if (kitVoices && gmKit) drums = 94;
    else if (gmCluster) drums = 86;
    else if (ch10Frac >= 0.70 && drumFrac >= 0.70 && kitVoices) drums = 82;

    int keys = 8;
    if (together) keys = (poly.frequentChords || poly.maxPoly >= 3) ? 90 : 78;

    int arp = 8, lead = 8, bass = 8;
    if (mono && cycle && steady)
        arp = 88;
    else if (mono && cycle)
        arp = 74;

    if (mono && !cycle)
    {
        if (meanPitch <= 52)
            bass = meanPitch < 42 ? 86 : 76;
        else if (stepwise >= 2 || lenVar >= 0.75)
            lead = 80;
        else
            lead = 36;
        juce::ignoreUnused(leaps);
    }

    // A repeating chord cycle outranks register, so a low arp is not bass.
    if (cycle && arp >= 74)
        bass = juce::jmin(bass, 40);

    struct Cand { ClipKind kind; int score; };
    Cand cands[] = {
        { ClipKind::Drums, drums },
        { ClipKind::Keys, keys },
        { ClipKind::Arp, arp },
        { ClipKind::Lead, lead },
        { ClipKind::Bass, bass },
    };
    Cand best = cands[0];
    Cand second = cands[1];
    for (const auto& c : cands)
    {
        if (c.score > best.score)
        {
            second = best;
            best = c;
        }
        else if (c.score > second.score)
        {
            second = c;
        }
    }

    const juce::String nameSrc = s.filePath.isNotEmpty() ? s.filePath : s.name;
    if (auto tagged = kindKeywordFromFileName(nameSrc))
    {
        int taggedScore = 0;
        for (const auto& c : cands)
            if (c.kind == *tagged)
                taggedScore = c.score;
        // Filename only breaks a close call. It cannot invent a kind the notes reject.
        if (taggedScore >= 55 && best.score - taggedScore <= 12)
        {
            best.kind = *tagged;
            best.score = juce::jmax(best.score, taggedScore);
        }
    }

    if (best.score < kKindConfidenceThreshold)
    {
        s.kind = ClipKind::Unsure;
        s.kindConfidence = juce::jlimit(0, 100, best.score);
        return;
    }

    s.kind = best.kind;
    int confidence = best.score;
    if (best.score - second.score < 8)
        confidence = juce::jmax(kKindConfidenceThreshold, best.score - 8);
    s.kindConfidence = juce::jlimit(0, 100, confidence);
    if (s.kind == ClipKind::Drums)
        s.hasDrumContent = true;
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

    std::set<int> pitches;
    std::set<int> lengths;
    std::vector<double> onsets;
    onsets.reserve(notes.size());
    int pcCount[12] = {};
    for (const auto& n : notes)
    {
        pitches.insert(n.pitch);
        lengths.insert(juce::jlimit(1, 64, (int) std::lround(n.len * 4.0)));
        onsets.push_back(n.start);
        pcCount[wrapPc(n.pitch)]++;
    }
    std::sort(onsets.begin(), onsets.end());
    int rhythmChanges = 0;
    double prevGap = -1.0;
    for (size_t i = 1; i < onsets.size(); ++i)
    {
        const double gap = onsets[i] - onsets[i - 1];
        if (gap < 0.05)
            continue;
        if (prevGap >= 0.0 && std::abs(gap - prevGap) > 0.24)
            ++rhythmChanges;
        prevGap = gap;
    }
    int rootPc = 0;
    for (int i = 1; i < 12; ++i)
        if (pcCount[i] > pcCount[rootPc])
            rootPc = i;
    int tense = 0;
    for (const auto& n : notes)
    {
        const int rel = (wrapPc(n.pitch) - rootPc + 12) % 12;
        // Stable tones are the triad plus the 4th. Everything else is a tension.
        if (rel != 0 && rel != 3 && rel != 4 && rel != 5 && rel != 7)
            ++tense;
    }
    const double distinctScore = juce::jlimit(0.0, 1.0, (double) pitches.size() / 12.0);
    const double noteCountScore = juce::jlimit(0.0, 1.0,
        std::log2(1.0 + (double) notes.size()) / std::log2(129.0));
    const double lengthChange = juce::jlimit(0.0, 1.0, (double) juce::jmax(0, (int) lengths.size() - 1) / 5.0);
    const double rhythmChangeScore = notes.size() < 3 ? 0.0
        : juce::jlimit(0.0, 1.0, (double) rhythmChanges / (double) juce::jmax(1, (int) notes.size() / 2));
    const double tension = juce::jlimit(0.0, 1.0, (double) tense / (double) notes.size());
    const double shared = 100.0 * (0.16 * noteCountScore
                                  + 0.14 * distinctScore
                                  + 0.16 * syncop
                                  + 0.14 * rhythmChangeScore
                                  + 0.12 * lengthChange
                                  + 0.16 * tension
                                  + 0.12 * density);
    const auto blend = [shared](double kindScore)
    {
        return juce::jlimit(1, 100, (int) std::lround(0.62 * kindScore + 0.38 * shared));
    };

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
        return blend(score);
    }

    if (kind == ClipKind::Keys)
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
        return blend(score);
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
    return blend(score);
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

    if (kind == ClipKind::Keys)
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

    // Bar count follows the last note. Trailing empty measures are not part of the clip.
    const double contentSteps = maxEnd > 0.0 ? maxEnd : clip.lengthBeats * stepsPerBeat;
    s.bars = std::max(1, (int) std::ceil(contentSteps / (double) kStepsPerBar));
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
        // Still tag from the filename when MIDI has no note events.
        if (auto tagged = kindKeywordFromFileName(s.filePath.isNotEmpty() ? s.filePath : s.name))
        {
            s.kind = *tagged;
            s.kindConfidence = 90;
            if (s.kind == ClipKind::Drums)
                s.hasDrumContent = true;
        }
        else
        {
            s.kind = ClipKind::Unsure;
            s.kindConfidence = 0;
        }
        s.complexity = 1;
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
    s.fileBytes = file.getSize();
    s.fileModMs = file.getLastModificationTime().toMilliseconds();
    s.fileMetaKnown = true;
    s.notesResident = false;

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return s;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
        return s;
    const auto timing = inspectMidiTiming(midiFile);
    s.bpm = timing.bpm;
    s.timeSigNum = timing.timeSigNum;
    s.timeSigDen = timing.timeSigDen;

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
            ne.startBeat = midiTimestampToBeats(msg.getTimeStamp(), timing);
            if (holder->noteOffObject != nullptr)
                ne.lengthBeats = midiTimestampToBeats(
                    holder->noteOffObject->message.getTimeStamp() - msg.getTimeStamp(), timing);
            else
                ne.lengthBeats = 0.25;
            if (ne.lengthBeats < 0.0)
                ne.lengthBeats = 0.0;
            spans.push_back(ne);
            maxBeat = std::max(maxBeat, ne.startBeat + ne.lengthBeats);
        }
        maxBeat = std::max(maxBeat, midiTimestampToBeats(track.getEndTime(), timing));
    }

    MidiClip shell;
    shell.bpm = timing.bpm;
    shell.lengthBeats = maxBeat > 0.0 ? maxBeat : 4.0;
    shell.timeSigNum = timing.timeSigNum;
    shell.timeSigDen = timing.timeSigDen;
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
