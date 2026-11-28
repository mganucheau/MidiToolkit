#include "AlsSession.h"

namespace pflow {
namespace {

juce::String attributeValue(const juce::XmlElement& el)
{
    return el.getStringAttribute("Value").trim();
}

bool isDisabledClip(const juce::XmlElement& clip)
{
    if (auto* disabled = clip.getChildByName("Disabled"))
    {
        const auto value = attributeValue(*disabled).toLowerCase();
        return value == "true" || value == "1";
    }
    return false;
}

juce::String trackNameOf(const juce::XmlElement& track)
{
    const auto* name = track.getChildByName("Name");
    if (name == nullptr)
        return {};

    juce::String user, effective;
    if (const auto* userEl = name->getChildByName("UserName"))
        user = attributeValue(*userEl);
    if (const auto* effectiveEl = name->getChildByName("EffectiveName"))
        effective = attributeValue(*effectiveEl);
    if (user.isNotEmpty())
        return user;
    return effective;
}

double manualTempo(const juce::XmlElement& el)
{
    if (el.hasTagName("Tempo"))
    {
        if (const auto* manual = el.getChildByName("Manual"))
        {
            const double bpm = manual->getDoubleAttribute("Value", 0.0);
            if (bpm > 0.0)
                return bpm;
        }
    }

    for (auto* child = el.getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        const double bpm = manualTempo(*child);
        if (bpm > 0.0)
            return bpm;
    }
    return 0.0;
}

void readTimeSignature(const juce::XmlElement& el, int& numerator, int& denominator)
{
    if (el.hasTagName("Numerator"))
    {
        const int value = el.getIntAttribute("Value", numerator);
        if (value > 0)
            numerator = value;
    }
    else if (el.hasTagName("Denominator"))
    {
        const int value = el.getIntAttribute("Value", denominator);
        if (value > 0)
            denominator = value;
    }

    for (auto* child = el.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        readTimeSignature(*child, numerator, denominator);
}

void collectKeyTrack(const juce::XmlElement& keyTrack, double loopStart, double loopEnd,
                     bool haveLoop, std::vector<NoteEvent>& notes)
{
    int pitch = 60;
    if (const auto* midiKey = keyTrack.getChildByName("MidiKey"))
        pitch = juce::jlimit(0, 127, midiKey->getIntAttribute("Value", 60));

    const auto* notesEl = keyTrack.getChildByName("Notes");
    if (notesEl == nullptr)
        return;

    for (auto* event = notesEl->getFirstChildElement(); event != nullptr; event = event->getNextElement())
    {
        if (!event->hasTagName("MidiNoteEvent"))
            continue;

        const double time = event->getDoubleAttribute("Time", 0.0);
        const double duration = event->getDoubleAttribute("Duration", 0.0);
        if (!(duration > 0.0))
            continue;
        if (haveLoop && (time < loopStart - 1.0e-8 || time >= loopEnd - 1.0e-8))
            continue;

        double length = duration;
        if (haveLoop)
        {
            length = std::min(time + duration, loopEnd) - time;
            if (!(length > 0.0))
                continue;
        }

        NoteEvent note;
        note.noteNumber = pitch;
        note.velocity = juce::jlimit(1, 127, event->getIntAttribute("Velocity", 100));
        note.startBeat = haveLoop ? time - loopStart : time;
        note.lengthBeats = length;
        note.channel = 1;
        notes.push_back(note);
    }
}

void collectNotes(const juce::XmlElement& el, double loopStart, double loopEnd, bool haveLoop,
                  std::vector<NoteEvent>& notes)
{
    if (el.hasTagName("KeyTrack"))
    {
        collectKeyTrack(el, loopStart, loopEnd, haveLoop, notes);
        return;
    }

    for (auto* child = el.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        collectNotes(*child, loopStart, loopEnd, haveLoop, notes);
}

AlsClip clipFromElement(const juce::XmlElement& clip, const juce::String& track)
{
    AlsClip out;
    out.track = track.isNotEmpty() ? track : juce::String("Track");
    if (const auto* name = clip.getChildByName("Name"))
        out.name = attributeValue(*name);
    if (out.name.isEmpty())
        out.name = "Clip";

    double loopStart = 0.0;
    double loopEnd = 0.0;
    bool haveLoop = false;
    if (const auto* loop = clip.getChildByName("Loop"))
    {
        if (const auto* start = loop->getChildByName("LoopStart"))
            loopStart = start->getDoubleAttribute("Value", 0.0);
        if (const auto* end = loop->getChildByName("LoopEnd"))
            loopEnd = end->getDoubleAttribute("Value", 0.0);
        haveLoop = loopEnd > loopStart;
    }

    if (const auto* timeSig = clip.getChildByName("TimeSignature"))
        readTimeSignature(*timeSig, out.timeSigNum, out.timeSigDen);

    const auto* notesRoot = clip.getChildByName("Notes");
    if (notesRoot != nullptr)
        collectNotes(*notesRoot, loopStart, loopEnd, haveLoop, out.notes);

    std::sort(out.notes.begin(), out.notes.end(), [](const NoteEvent& a, const NoteEvent& b)
    {
        if (a.startBeat < b.startBeat)
            return true;
        if (b.startBeat < a.startBeat)
            return false;
        return a.noteNumber < b.noteNumber;
    });

    if (haveLoop)
        out.lengthBeats = loopEnd - loopStart;
    else
    {
        double end = 0.0;
        for (const auto& note : out.notes)
            end = std::max(end, note.startBeat + note.lengthBeats);
        out.lengthBeats = end;
    }
    return out;
}

void walkSet(const juce::XmlElement& el, const juce::String& track, std::vector<AlsClip>& clips)
{
    juce::String next = track;
    if (el.hasTagName("MidiTrack") || el.hasTagName("GroupTrack"))
    {
        const auto named = trackNameOf(el);
        if (named.isNotEmpty())
            next = named;
    }

    if (el.hasTagName("MidiClip"))
    {
        if (!isDisabledClip(el))
        {
            auto clip = clipFromElement(el, next);
            if (!clip.notes.empty())
                clips.push_back(std::move(clip));
        }
        return;
    }

    for (auto* child = el.getFirstChildElement(); child != nullptr; child = child->getNextElement())
        walkSet(*child, next, clips);
}

juce::String readAlsText(const juce::MemoryBlock& block)
{
    juce::MemoryInputStream raw(block.getData(), block.getSize(), false);
    juce::GZIPDecompressorInputStream gzip(&raw, false, juce::GZIPDecompressorInputStream::gzipFormat);
    auto xml = gzip.readEntireStreamAsString();
    if (xml.contains("LiveSet") || xml.contains("<Ableton"))
        return xml;

    juce::MemoryInputStream plain(block.getData(), block.getSize(), false);
    return plain.readEntireStreamAsString();
}

juce::String legalLeaf(const juce::String& name, const juce::String& fallback)
{
    auto leaf = juce::File::createLegalFileName(name);
    if (leaf.isEmpty() || leaf == "." || leaf == "..")
        leaf = fallback;
    return leaf;
}

} // namespace

juce::File liveSetPathFromLog(const juce::String& logText)
{
    juce::String last;
    const auto marker = juce::String("Loading document \"");
    for (const auto& line : juce::StringArray::fromLines(logText))
    {
        const int at = line.indexOf(marker);
        if (at < 0)
            continue;
        const int start = at + marker.length();
        const int end = line.indexOfChar(start, '"');
        if (end <= start)
            continue;
        const auto path = line.substring(start, end);
        if (path.endsWithIgnoreCase(".als"))
            last = path;
    }
    return last.isEmpty() ? juce::File() : juce::File(last);
}

juce::File findCurrentLiveSet()
{
    const auto prefs = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                           .getChildFile("Library/Preferences/Ableton");
    juce::File newestLog;
    juce::Time newestTime;
    if (prefs.isDirectory())
    {
        for (const auto& entry : juce::RangedDirectoryIterator(prefs, false, "*", juce::File::findDirectories))
        {
            const auto log = entry.getFile().getChildFile("Log.txt");
            if (!log.existsAsFile())
                continue;
            const auto modified = log.getLastModificationTime();
            if (!newestLog.existsAsFile() || modified > newestTime)
            {
                newestLog = log;
                newestTime = modified;
            }
        }
    }
    if (!newestLog.existsAsFile())
        return {};
    return liveSetPathFromLog(newestLog.loadFileAsString());
}

AlsSession parseAlsSession(const juce::File& alsFile)
{
    AlsSession session;
    session.setFile = alsFile;
    session.setName = alsFile.getFileNameWithoutExtension();
    if (!alsFile.existsAsFile())
        return session;

    juce::MemoryBlock block;
    if (!alsFile.loadFileAsData(block) || block.getSize() == 0)
        return session;

    const auto xml = readAlsText(block);
    auto root = juce::XmlDocument::parse(xml);
    if (root == nullptr)
        return session;

    const double bpm = manualTempo(*root);
    if (bpm > 0.0)
        session.bpm = bpm;
    walkSet(*root, {}, session.clips);
    return session;
}

juce::File writeSessionMidiFiles(const AlsSession& session)
{
    if (session.clips.empty())
        return {};

    const auto root = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                          .getChildFile("Library/Caches/MidiToolkit/Session");
    const auto folderName = legalLeaf(session.setName.isNotEmpty() ? session.setName : juce::String("Session"),
                                      "Session");
    const auto finalDir = root.getChildFile(folderName);
    const auto staging = root.getChildFile(folderName + ".incoming");
    if (staging.exists())
        staging.deleteRecursively();
    if (!staging.createDirectory())
        return {};

    const int count = (int) session.clips.size();
    const int width = count >= 100 ? 3 : 2;
    double bpm = session.bpm;
    if (!(bpm >= 20.0 && bpm <= 999.0))
        bpm = 120.0;

    int written = 0;
    for (int i = 0; i < count; ++i)
    {
        const auto& clip = session.clips[(size_t) i];
        MidiClip midi;
        midi.name = clip.name;
        midi.bpm = bpm;
        midi.lengthBeats = clip.lengthBeats;
        midi.timeSigNum = clip.timeSigNum;
        midi.timeSigDen = clip.timeSigDen;
        midi.notes = clip.notes;

        const auto number = juce::String(i + 1).paddedLeft('0', width);
        const auto track = clip.track.isNotEmpty() ? clip.track : juce::String("Track");
        const auto name = clip.name.isNotEmpty() ? clip.name : juce::String("Clip");
        auto leaf = legalLeaf(number + " " + track + " - " + name + ".mid", number + " Clip.mid");
        if (!leaf.endsWithIgnoreCase(".mid"))
            leaf << ".mid";
        if (writeMidiFile(midi, staging.getChildFile(leaf), bpm, clip.lengthBeats))
            ++written;
    }

    if (written == 0)
    {
        staging.deleteRecursively();
        return {};
    }

    if (finalDir.exists())
        finalDir.deleteRecursively();
    if (!staging.moveFileTo(finalDir))
        return {};
    return finalDir;
}

} // namespace pflow
