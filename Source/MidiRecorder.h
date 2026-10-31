#pragma once

#include "EditModel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

namespace pflow {

/** End of the last measure that contains a note. Empty input is one measure.
    A note that ends exactly on a bar line belongs to the measure that just closed. */
inline double loopEndBeatForNotes(const std::vector<NoteEvent>& notes, int beatsPerBar = 4)
{
    const double bar = (double) juce::jmax(1, beatsPerBar);
    double last = 0.0;
    bool any = false;
    for (const auto& n : notes)
    {
        const double end = n.startBeat + juce::jmax(0.0, n.lengthBeats);
        last = std::max(last, end);
        any = true;
    }
    if (!any)
        return bar;
    const double measures = std::ceil(last / bar - 1.0e-9);
    return std::max(1.0, measures) * bar;
}

struct RecordingSnapshot
{
    std::vector<NoteEvent> notes;
    double lengthBeats = 0.0;
    bool recording = false;
};

inline StepClip stepClipFromRecording(const std::vector<NoteEvent>& notes, double lengthBeats, double bpm)
{
    const double loopEnd = loopEndBeatForNotes(notes, 4);
    const double clipBeats = std::max(loopEnd, lengthBeats);
    StepClip clip;
    clip.name = "Recording";
    clip.filePath = "recording://take";
    clip.kind = ClipKind::Lead;
    clip.bpm = bpm > 0.0 ? bpm : 120.0;
    clip.bars = juce::jmax(1, (int) std::ceil(clipBeats / 4.0 - 1.0e-9));
    clip.timeSigNum = 4;
    clip.timeSigDen = 4;
    clip.notesResident = true;
    clip.complexity = 1;
    int id = 1;
    for (const auto& n : notes)
    {
        RollNote note;
        note.id = id++;
        note.pitch = juce::jlimit(0, 127, n.noteNumber);
        note.start = juce::jmax(0.0, n.startBeat) * 4.0;
        note.len = juce::jmax(0.05, n.lengthBeats * 4.0);
        note.fileVelocity = juce::jlimit(1, 127, n.velocity);
        note.channel = juce::jlimit(1, 16, n.channel);
        clip.notes.push_back(note);
    }
    clip.noteCount = (int) clip.notes.size();
    return clip;
}

/** Audio-thread MIDI capture. Start and stop on the message thread. */
class MidiRecorder
{
public:
    void start()
    {
        const juce::ScopedLock sl(lock);
        completed.clear();
        completed.reserve(512);
        for (auto& h : held)
            h = {};
        lengthBeats = 0.0;
        recording = true;
    }

    void stop()
    {
        const juce::ScopedLock sl(lock);
        if (!recording)
            return;
        for (int n = 0; n < 128; ++n)
            if (held[(size_t) n].down)
                closeHeld(n, lengthBeats);
        recording = false;
    }

    bool isRecording() const
    {
        const juce::ScopedLock sl(lock);
        return recording;
    }

    void process(const juce::MidiBuffer& midi, int numSamples, double sampleRate, double bpm)
    {
        const juce::ScopedLock sl(lock);
        if (!recording || numSamples <= 0 || sampleRate <= 0.0)
            return;
        if (bpm <= 0.0)
            bpm = 120.0;
        const double beatsPerSample = bpm / (60.0 * sampleRate);
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            const double beat = lengthBeats + (double) meta.samplePosition * beatsPerSample;
            if (msg.isNoteOn())
            {
                const int n = msg.getNoteNumber();
                if (!juce::isPositiveAndBelow(n, 128))
                    continue;
                if (held[(size_t) n].down)
                    closeHeld(n, beat);
                held[(size_t) n].down = true;
                held[(size_t) n].velocity = juce::jlimit(1, 127, (int) msg.getVelocity());
                held[(size_t) n].channel = juce::jlimit(1, 16, (int) msg.getChannel());
                held[(size_t) n].startBeat = beat;
            }
            else if (msg.isNoteOff())
            {
                const int n = msg.getNoteNumber();
                if (juce::isPositiveAndBelow(n, 128) && held[(size_t) n].down)
                    closeHeld(n, beat);
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                for (int n = 0; n < 128; ++n)
                    if (held[(size_t) n].down)
                        closeHeld(n, beat);
            }
        }
        lengthBeats += (double) numSamples * beatsPerSample;
    }

    RecordingSnapshot snapshot() const
    {
        const juce::ScopedLock sl(lock);
        RecordingSnapshot snap;
        snap.recording = recording;
        snap.lengthBeats = lengthBeats;
        snap.notes = completed;
        if (recording)
        {
            for (int n = 0; n < 128; ++n)
            {
                const auto& h = held[(size_t) n];
                if (!h.down)
                    continue;
                NoteEvent ev;
                ev.noteNumber = n;
                ev.velocity = h.velocity;
                ev.channel = h.channel;
                ev.startBeat = h.startBeat;
                ev.lengthBeats = juce::jmax(1.0 / 64.0, lengthBeats - h.startBeat);
                snap.notes.push_back(ev);
            }
        }
        return snap;
    }

private:
    struct Held
    {
        bool down = false;
        int velocity = 100;
        int channel = 1;
        double startBeat = 0.0;
    };

    void closeHeld(int note, double beat)
    {
        auto& h = held[(size_t) note];
        NoteEvent ev;
        ev.noteNumber = note;
        ev.velocity = h.velocity;
        ev.channel = h.channel;
        ev.startBeat = h.startBeat;
        ev.lengthBeats = juce::jmax(1.0 / 64.0, beat - h.startBeat);
        completed.push_back(ev);
        h.down = false;
    }

    mutable juce::CriticalSection lock;
    std::vector<NoteEvent> completed;
    Held held[128] {};
    double lengthBeats = 0.0;
    bool recording = false;
};

} // namespace pflow
