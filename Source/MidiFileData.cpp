#include "MidiFileData.h"
#include <set>

namespace pflow {

// ── MidiClip helpers ─────────────────────────────────────────────────────────

juce::MidiMessageSequence MidiClip::toMidiSequence(double bpm) const
{
    juce::MidiMessageSequence seq;
    double secPerBeat = 60.0 / bpm;

    for (auto& n : notes)
    {
        double onTime  = n.startBeat * secPerBeat;
        double offTime = (n.startBeat + n.lengthBeats) * secPerBeat;
        int    pitch   = juce::jlimit(0, 127, n.noteNumber + rootNoteOffset);

        seq.addEvent(juce::MidiMessage::noteOn(n.channel, pitch, (juce::uint8)n.velocity), onTime);
        seq.addEvent(juce::MidiMessage::noteOff(n.channel, pitch), offTime);
    }
    for (const auto& entry : automation)
    {
        int cc = entry.first;
        for (const auto& pt : entry.second)
        {
            double t = pt.beat * secPerBeat;
            seq.addEvent(juce::MidiMessage::controllerEvent(1, cc, juce::jlimit(0, 127, pt.value)), t);
        }
    }
    seq.updateMatchedPairs();
    return seq;
}

std::vector<NoteEvent> MidiClip::getNotesForPitch(int noteNumber) const
{
    std::vector<NoteEvent> result;
    for (auto& n : notes)
        if (n.noteNumber == noteNumber)
            result.push_back(n);
    return result;
}

std::vector<int> MidiClip::getDistinctPitches() const
{
    std::set<int> pitches;
    for (auto& n : notes) pitches.insert(n.noteNumber);
    return { pitches.begin(), pitches.end() };
}

// ── MIDI file parser ─────────────────────────────────────────────────────────

MidiClip parseMidiFile(const juce::File& file)
{
    MidiClip clip;
    clip.name     = file.getFileNameWithoutExtension();
    clip.filePath = file.getFullPathName();

    juce::FileInputStream stream(file);
    if (!stream.openedOk()) return clip;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream)) return clip;

    midiFile.convertTimestampTicksToSeconds();

    double bpm = 120.0; // default
    int timeSigNum = 4;
    int timeSigDen = 4;
    // Try to read tempo / time signature from the file
    if (midiFile.getNumTracks() > 0)
    {
        auto* track = midiFile.getTrack(0);
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            auto& ev = track->getEventPointer(i)->message;
            if (ev.isTempoMetaEvent())
            {
                bpm = 60.0 / ev.getTempoSecondsPerQuarterNote();
                break;
            }
        }
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            auto& ev = track->getEventPointer(i)->message;
            if (ev.isTimeSignatureMetaEvent())
            {
                int num = 4, den = 4;
                ev.getTimeSignatureInfo(num, den);
                timeSigNum = juce::jmax(1, num);
                timeSigDen = juce::jmax(1, den);
                break;
            }
        }
        // Also scan other tracks for a time signature if track 0 had none.
        if (timeSigNum == 4 && timeSigDen == 4)
        {
            for (int t = 0; t < midiFile.getNumTracks(); ++t)
            {
                auto* tr = midiFile.getTrack(t);
                if (tr == nullptr) continue;
                for (int i = 0; i < tr->getNumEvents(); ++i)
                {
                    auto& ev = tr->getEventPointer(i)->message;
                    if (ev.isTimeSignatureMetaEvent())
                    {
                        int num = 4, den = 4;
                        ev.getTimeSignatureInfo(num, den);
                        timeSigNum = juce::jmax(1, num);
                        timeSigDen = juce::jmax(1, den);
                        break;
                    }
                }
                if (timeSigNum != 4 || timeSigDen != 4)
                    break;
            }
        }
    }

    clip.bpm = bpm;
    clip.timeSigNum = timeSigNum;
    clip.timeSigDen = timeSigDen;
    double secPerBeat = 60.0 / bpm;
    double maxBeat = 0.0;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        auto* trackPtr = midiFile.getTrack(t);
        if (trackPtr == nullptr) continue;

        // getTrack() returns const* in newer JUCE; copy so we can update pairs
        juce::MidiMessageSequence track(*trackPtr);
        track.updateMatchedPairs();

        for (int i = 0; i < track.getNumEvents(); ++i)
        {
            auto* evHolder = track.getEventPointer(i);
            auto& msg = evHolder->message;

            if (msg.isNoteOn())
            {
                NoteEvent ne;
                ne.noteNumber = msg.getNoteNumber();
                ne.velocity   = msg.getVelocity();
                ne.channel    = msg.getChannel();
                ne.startBeat  = msg.getTimeStamp() / secPerBeat;

                if (evHolder->noteOffObject != nullptr)
                    ne.lengthBeats = (evHolder->noteOffObject->message.getTimeStamp() - msg.getTimeStamp()) / secPerBeat;
                else
                    ne.lengthBeats = 0.25;

                clip.notes.push_back(ne);
                maxBeat = std::max(maxBeat, ne.startBeat + ne.lengthBeats);
            }
        }

        // Honour the file's intended length (end-of-track marker), so loops
        // exported with trailing space keep their trimmable empty bars.
        maxBeat = std::max(maxBeat, track.getEndTime() / secPerBeat);
    }

    // Clip length = actual content extent only (no bar quantisation or minimum bar padding).
    // Empty / note-less files keep a small default length for the UI.
    if (maxBeat > 0.0)
        clip.lengthBeats = maxBeat;
    else
        clip.lengthBeats = 4.0;
    trimTrailingEmptyMeasures(clip);
    clip.notesAtFileLoad = clip.notes;
    return clip;
}

void trimTrailingEmptyMeasures(MidiClip& clip)
{
    if (clip.notes.empty())
        return;
    double last = 0.0;
    for (const auto& n : clip.notes)
        last = std::max(last, n.startBeat + n.lengthBeats);
    const double den = clip.timeSigDen > 0 ? (double) clip.timeSigDen : 4.0;
    const double beatsPerBar = (double) juce::jmax(1, clip.timeSigNum) * (4.0 / den);
    if (beatsPerBar <= 0.0)
        return;
    const int usedBars = juce::jmax(1, (int) std::ceil(last / beatsPerBar - 1.0e-6));
    const double fitted = (double) usedBars * beatsPerBar;
    if (clip.lengthBeats > fitted + 1.0e-4)
        clip.lengthBeats = fitted;
}

void trimEmptyMeasuresInClip(MidiClip& clip, double beatsPerBar)
{
    if (beatsPerBar <= 0.0 || clip.notes.empty()) return;

    bool changed = true;
    int guard = 0;
    while (changed && guard++ < 10000)
    {
        changed = false;
        double clipEnd = 0.0;
        for (const auto& n : clip.notes)
            clipEnd = std::max(clipEnd, n.startBeat + n.lengthBeats);
        clipEnd = std::max(clipEnd, clip.lengthBeats);
        int maxM = juce::jmax(1, (int)std::ceil(clipEnd / beatsPerBar));

        for (int m = 0; m < maxM; ++m)
        {
            double ms = m * beatsPerBar;
            double me = (m + 1) * beatsPerBar;
            bool hasNote = false;
            for (const auto& n : clip.notes)
            {
                if (n.startBeat < me && n.startBeat + n.lengthBeats > ms)
                {
                    hasNote = true;
                    break;
                }
            }
            if (hasNote) continue;

            for (auto& n : clip.notes)
            {
                if (n.startBeat >= me)
                    n.startBeat -= beatsPerBar;
            }
            clip.lengthBeats = std::max(beatsPerBar, clip.lengthBeats - beatsPerBar);
            changed = true;
            break;
        }
    }
}

// ── Write MIDI file ─────────────────────────────────────────────────────────

bool writeMidiFile(const MidiClip& clip, const juce::File& file, double bpm, double maxLengthBeats)
{
    if (clip.notes.empty()) return false;

    juce::MidiFile midiFile;
    int ticksPerBeat = 480;
    midiFile.setTicksPerQuarterNote(ticksPerBeat);

    juce::MidiMessageSequence track;

    // Tempo meta event at tick 0
    track.addEvent(juce::MidiMessage::tempoMetaEvent(
        (int)(60000000.0 / bpm)), 0.0);

    // Track name
    track.addEvent(juce::MidiMessage::textMetaEvent(3, clip.name), 0.0);

    int offset = juce::jlimit(-127, 127, clip.rootNoteOffset);
    for (auto& n : clip.notes)
    {
        double noteEnd = n.startBeat + n.lengthBeats;
        if (maxLengthBeats > 0.0)
        {
            if (n.startBeat >= maxLengthBeats) continue;
            noteEnd = std::min(noteEnd, maxLengthBeats);
            if (noteEnd <= n.startBeat) continue;
        }
        double onTick  = n.startBeat * ticksPerBeat;
        double offTick = noteEnd * ticksPerBeat;
        int pitch = juce::jlimit(0, 127, n.noteNumber + offset);

        track.addEvent(juce::MidiMessage::noteOn(n.channel, pitch, (juce::uint8)n.velocity), onTick);
        track.addEvent(juce::MidiMessage::noteOff(n.channel, pitch), offTick);
    }

    track.updateMatchedPairs();
    midiFile.addTrack(track);

    file.deleteFile();
    juce::FileOutputStream out(file);
    if (!out.openedOk()) return false;

    return midiFile.writeTo(out, 0);
}

// ── Scale helpers ────────────────────────────────────────────────────────────

juce::String scaleTypeName(ScaleType s)
{
    switch (s)
    {
        case ScaleType::Chromatic:       return "Chromatic";
        case ScaleType::Major:           return "Major";
        case ScaleType::NaturalMinor:    return "Natural Minor";
        case ScaleType::HarmonicMinor:   return "Harmonic Minor";
        case ScaleType::MelodicMinor:    return "Melodic Minor";
        case ScaleType::Dorian:          return "Dorian";
        case ScaleType::Phrygian:        return "Phrygian";
        case ScaleType::Lydian:          return "Lydian";
        case ScaleType::Mixolydian:      return "Mixolydian";
        case ScaleType::Locrian:         return "Locrian";
        case ScaleType::PentatonicMajor: return "Pentatonic Major";
        case ScaleType::PentatonicMinor: return "Pentatonic Minor";
        case ScaleType::Blues:           return "Blues";
        case ScaleType::WholeTone:       return "Whole Tone";
        default: return "Chromatic";
    }
}

std::vector<int> scaleIntervals(ScaleType s)
{
    switch (s)
    {
        case ScaleType::Major:           return {0,2,4,5,7,9,11};
        case ScaleType::NaturalMinor:    return {0,2,3,5,7,8,10};
        case ScaleType::HarmonicMinor:   return {0,2,3,5,7,8,11};
        case ScaleType::MelodicMinor:    return {0,2,3,5,7,9,11};
        case ScaleType::Dorian:          return {0,2,3,5,7,9,10};
        case ScaleType::Phrygian:        return {0,1,3,5,7,8,10};
        case ScaleType::Lydian:          return {0,2,4,6,7,9,11};
        case ScaleType::Mixolydian:      return {0,2,4,5,7,9,10};
        case ScaleType::Locrian:         return {0,1,3,5,6,8,10};
        case ScaleType::PentatonicMajor: return {0,2,4,7,9};
        case ScaleType::PentatonicMinor: return {0,3,5,7,10};
        case ScaleType::Blues:           return {0,3,5,6,7,10};
        case ScaleType::WholeTone:       return {0,2,4,6,8,10};
        default:                         return {0,1,2,3,4,5,6,7,8,9,10,11};
    }
}

int estimatePitchClassFromNotes(const std::vector<NoteEvent>& notes)
{
    if (notes.empty()) return 0;
    int weight[12] = {};
    for (const auto& n : notes)
    {
        const int pc = ((n.noteNumber % 12) + 12) % 12;
        const int w = juce::jmax(1, (int)std::lround(n.lengthBeats * 100.0) + 1);
        weight[pc] += w;
    }
    int best = 0;
    for (int i = 1; i < 12; ++i)
        if (weight[i] > weight[best]) best = i;
    return best;
}

int quantiseToScale(int inNote, int scaleRoot, ScaleType scale)
{
    if (scale == ScaleType::Chromatic) return inNote;

    auto intervals = scaleIntervals(scale);
    int octave    = (inNote - scaleRoot) / 12;
    int degree    = (inNote - scaleRoot) % 12;
    if (degree < 0) { degree += 12; octave--; }

    // Find closest scale degree
    int bestDist = 999;
    int bestInterval = 0;
    for (int iv : intervals)
    {
        int dist = std::abs(degree - iv);
        if (dist < bestDist) { bestDist = dist; bestInterval = iv; }
    }
    return scaleRoot + octave * 12 + bestInterval;
}

// ── CompLane helpers ─────────────────────────────────────────────────────────

void CompLane::addClipAtPosition(const MidiClip& clip, double beatPos)
{
    int idx = (int)clips.size();
    clips.push_back(clip);

    CompRegion r;
    r.startBeat = beatPos;
    r.endBeat   = beatPos + clip.lengthBeats;
    r.clipIndex  = idx;
    regions.push_back(r);
}

void CompLane::removeRegion(int index)
{
    if (index >= 0 && index < (int)regions.size())
        regions.erase(regions.begin() + index);
}

} // namespace pflow
