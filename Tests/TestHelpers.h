#pragma once
#include "MidiFileData.h"
#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

namespace pflow::test
{

inline juce::File tempMidiFile(const juce::String& leafName)
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("MidiToolkitTests")
        .getChildFile(leafName);
}

inline void removeTempMidiFile(const juce::File& file)
{
    file.deleteFile();
}

inline MidiClip makeClipWithNotes(std::initializer_list<NoteEvent> notes, double lengthBeats = -1.0)
{
    MidiClip clip;
    clip.name = "Test";
    clip.notes.assign(notes);
    if (lengthBeats < 0.0)
    {
        double end = 0.0;
        for (const auto& n : clip.notes)
            end = std::max(end, n.startBeat + n.lengthBeats);
        clip.lengthBeats = std::max(4.0, end);
    }
    else
    {
        clip.lengthBeats = lengthBeats;
    }
    return clip;
}

inline bool writeTempMidi(const MidiClip& clip, const juce::String& leafName, double bpm = 120.0)
{
    const auto file = tempMidiFile(leafName);
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    return writeMidiFile(clip, file, bpm);
}

inline int countNoteOns(const juce::MidiBuffer& buffer)
{
    int count = 0;
    for (const auto metadata : buffer)
        if (metadata.getMessage().isNoteOn())
            ++count;
    return count;
}

inline int countNoteOffs(const juce::MidiBuffer& buffer)
{
    int count = 0;
    for (const auto metadata : buffer)
        if (metadata.getMessage().isNoteOff())
            ++count;
    return count;
}

inline bool hasAllNotesOff(const juce::MidiBuffer& buffer)
{
    for (const auto metadata : buffer)
    {
        const auto& msg = metadata.getMessage();
        if (msg.isAllNotesOff() || (msg.isController() && msg.getControllerNumber() == 123))
            return true;
    }
    return false;
}

} // namespace pflow::test
