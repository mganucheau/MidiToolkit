#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Theme.h"
#include "LibraryStore.h"
#include <algorithm>
#include <cmath>

namespace pflow {

MidiToolkitProcessor::MidiToolkitProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    library_.load();
    applyLibraryToMemory();
    applySharedUiSessionFromLibrary();
}

MidiToolkitProcessor::~MidiToolkitProcessor()
{
    // Favorites + last workspace live in Application Support — flush on teardown.
    persistSharedUiSession();
    syncLibraryFromMemory();
    library_.flush();
}

void MidiToolkitProcessor::applyLibraryToMemory()
{
    starredFiles = library_.starredFiles;
    savedSearches = library_.savedSearches;
}

void MidiToolkitProcessor::applySharedUiSessionFromLibrary()
{
    if (!library_.hasUiSession)
        return;

    const auto& s = library_.uiSession;
    lastBrowserDir = s.lastBrowserDir;
    savedBrowserDirs = s.savedBrowserDirs;
    savedLibraries = s.savedLibraries;
    if (savedLibraries.empty())
    {
        for (const auto& path : savedBrowserDirs)
        {
            SavedLibraryFolder folder;
            folder.path = path;
            savedLibraries.push_back(std::move(folder));
        }
    }
    else if (savedBrowserDirs.isEmpty())
    {
        for (const auto& folder : savedLibraries)
            if (folder.path.isNotEmpty())
                savedBrowserDirs.add(folder.path);
    }
    browseMode = juce::jlimit(0, 2, s.browseMode);
    starredFilter = s.starredFilter;
    includeSubdirs = s.includeSubdirs;
    browserSessionSearch = s.browserSessionSearch;
    selectedClipPath = s.selectedClipPath;
    activeSavedSearchIdx = s.activeSavedSearchIdx;
    browserResultPaths = s.browserResultPaths;
    columnVisibility = s.columnVisibility;
    nameColumnWidth = juce::jlimit(120, 2400, s.nameColumnWidth);
    browserPaneW = juce::jmax(0, s.browserPaneW);
    editorOpen = s.editorOpen;
    effectsOpen = s.effectsOpen;
    previewOpen = s.previewOpen;
    sidebarCollapsed = s.sidebarCollapsed;
    passthrough = s.passthrough;
    passthroughActive.store(passthrough);

    tweaks().density.store(juce::jlimit(0, 1, s.tweakDensity));
    tweaks().size.store(juce::jlimit(0, kNumContentSizes - 1, s.tweakSize));
    tweaks().textScalePct.store(juce::jlimit(60, 150, s.tweakTextScalePct));
    tweaks().appearance.store(juce::jlimit(0, kNumAppearances - 1, s.tweakAppearance));
    tweaks().showTooltips.store(s.tweakShowTooltips != 0 ? 1 : 0);

    hasDefaultView = s.hasDefaultView;
    defEditorOpen = s.defEditorOpen;
    defEffectsOpen = s.defEffectsOpen;
    defPreviewOpen = s.defPreviewOpen;
    defSidebarCollapsed = s.defSidebarCollapsed;
    defPassthrough = s.defPassthrough;
    defColumns = s.defColumns;
    defNameColumnWidth = s.defNameColumnWidth;
    defWindowW = s.defWindowW;
    defWindowH = s.defWindowH;
    defTweakSize = s.defTweakSize;
    defTweakTextScalePct = s.defTweakTextScalePct;
    defTweakDensity = s.defTweakDensity;
    defTweakAppearance = s.defTweakAppearance;
}

void MidiToolkitProcessor::persistSharedUiSession()
{
    SharedUiSession s;
    s.lastBrowserDir = lastBrowserDir;
    s.savedBrowserDirs = savedBrowserDirs;
    s.savedLibraries = savedLibraries;
    s.browseMode = juce::jlimit(0, 2, browseMode);
    s.starredFilter = starredFilter;
    s.includeSubdirs = includeSubdirs;
    s.browserSessionSearch = browserSessionSearch;
    s.selectedClipPath = selectedClipPath;
    s.activeSavedSearchIdx = activeSavedSearchIdx;
    s.browserResultPaths = browserResultPaths;
    s.columnVisibility = columnVisibility;
    s.nameColumnWidth = juce::jlimit(120, 2400, nameColumnWidth);
    s.browserPaneW = juce::jmax(0, browserPaneW);
    s.editorOpen = editorOpen;
    s.effectsOpen = effectsOpen;
    s.previewOpen = previewOpen;
    s.sidebarCollapsed = sidebarCollapsed;
    s.passthrough = passthrough;
    s.tweakDensity = tweaks().density.load();
    s.tweakSize = tweaks().size.load();
    s.tweakTextScalePct = tweaks().textScalePct.load();
    s.tweakAppearance = tweaks().appearance.load();
    s.tweakShowTooltips = tweaks().showTooltips.load();
    s.hasDefaultView = hasDefaultView;
    s.defEditorOpen = defEditorOpen;
    s.defEffectsOpen = defEffectsOpen;
    s.defPreviewOpen = defPreviewOpen;
    s.defSidebarCollapsed = defSidebarCollapsed;
    s.defPassthrough = defPassthrough;
    s.defColumns = defColumns;
    s.defNameColumnWidth = defNameColumnWidth;
    s.defWindowW = defWindowW;
    s.defWindowH = defWindowH;
    s.defTweakSize = defTweakSize;
    s.defTweakTextScalePct = defTweakTextScalePct;
    s.defTweakDensity = defTweakDensity;
    s.defTweakAppearance = defTweakAppearance;
    library_.putUiSession(s);
}

void MidiToolkitProcessor::syncLibraryFromMemory()
{
    // Union memory stars into the library (never replace — disk accrues).
    library_.mergeStarsFromDisk();
    for (const auto& s : starredFiles)
        if (s.isNotEmpty() && !library_.starredFiles.contains(s))
            library_.starredFiles.add(s);
    library_.savedSearches = savedSearches;
    // Keep the shared workspace snapshot current whenever we flush the library.
    {
        SharedUiSession s;
        s.lastBrowserDir = lastBrowserDir;
        s.savedBrowserDirs = savedBrowserDirs;
        s.savedLibraries = savedLibraries;
        s.browseMode = juce::jlimit(0, 2, browseMode);
        s.starredFilter = starredFilter;
        s.includeSubdirs = includeSubdirs;
        s.browserSessionSearch = browserSessionSearch;
        s.selectedClipPath = selectedClipPath;
        s.activeSavedSearchIdx = activeSavedSearchIdx;
        s.browserResultPaths = browserResultPaths;
        s.columnVisibility = columnVisibility;
        s.nameColumnWidth = juce::jlimit(120, 2400, nameColumnWidth);
        s.browserPaneW = juce::jmax(0, browserPaneW);
        s.editorOpen = editorOpen;
        s.effectsOpen = effectsOpen;
        s.previewOpen = previewOpen;
        s.sidebarCollapsed = sidebarCollapsed;
        s.passthrough = passthrough;
        s.tweakDensity = tweaks().density.load();
        s.tweakSize = tweaks().size.load();
        s.tweakTextScalePct = tweaks().textScalePct.load();
        s.tweakAppearance = tweaks().appearance.load();
        s.tweakShowTooltips = tweaks().showTooltips.load();
        s.hasDefaultView = hasDefaultView;
        s.defEditorOpen = defEditorOpen;
        s.defEffectsOpen = defEffectsOpen;
        s.defPreviewOpen = defPreviewOpen;
        s.defSidebarCollapsed = defSidebarCollapsed;
        s.defPassthrough = defPassthrough;
        s.defColumns = defColumns;
        s.defNameColumnWidth = defNameColumnWidth;
        s.defWindowW = defWindowW;
        s.defWindowH = defWindowH;
        s.defTweakSize = defTweakSize;
        s.defTweakTextScalePct = defTweakTextScalePct;
        s.defTweakDensity = defTweakDensity;
        s.defTweakAppearance = defTweakAppearance;
        library_.uiSession = s;
        library_.hasUiSession = true;
    }
    library_.save();
    starredFiles = library_.starredFiles;
}

void MidiToolkitProcessor::saveLibrary()
{
    syncLibraryFromMemory();
}

void MidiToolkitProcessor::toggleStarred(const juce::String& path)
{
    library_.toggleStarred(path);
    starredFiles = library_.starredFiles;
}

void MidiToolkitProcessor::addSavedSearch(const SavedSearchEntry& entry)
{
    library_.addSavedSearch(entry);
    savedSearches = library_.savedSearches;
}

void MidiToolkitProcessor::removeSavedSearch(int index)
{
    library_.removeSavedSearch(index);
    savedSearches = library_.savedSearches;
}

void MidiToolkitProcessor::updateSavedSearchResults(int index,
                                                    const juce::StringArray& resultPaths,
                                                    const juce::String& rootPath)
{
    if (!juce::isPositiveAndBelow(index, (int) savedSearches.size()))
        return;
    auto& entry = savedSearches[(size_t) index];
    entry.resultPaths = resultPaths;
    if (rootPath.isNotEmpty())
        entry.rootPath = rootPath;
    syncLibraryFromMemory();
}

void MidiToolkitProcessor::prepareToPlay(double sr, int)
{
    sampleRate_ = sr;
}

void MidiToolkitProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
{
    buffer.clear();

    if (auto* playHead = getPlayHead())
    {
        if (const auto posInfo = playHead->getPosition())
        {
            if (auto bpm = posInfo->getBpm(); bpm.hasValue())
                hostBpm.store(*bpm);
            if (auto ppq = posInfo->getPpqPosition(); ppq.hasValue())
                hostBeatPos.store(*ppq);
            hostPlaying.store(posInfo->getIsPlaying());

            const bool looping = posInfo->getIsLooping();
            if (looping)
            {
                if (auto lp = posInfo->getLoopPoints(); lp.hasValue()
                    && lp->ppqEnd > lp->ppqStart + 1.0e-6)
                {
                    hostLoopActive.store(true);
                    hostLoopPpqStart.store(lp->ppqStart);
                    hostLoopPpqEnd.store(lp->ppqEnd);
                }
                else
                    hostLoopActive.store(false);
            }
            else
                hostLoopActive.store(false);
        }
    }

    if (recordingActive.load())
    {
        double bpm = hostBpm.load();
        if (bpm <= 0.0)
            bpm = freeBpm.load();
        recorder.process(midi, buffer.getNumSamples(), sampleRate_, bpm);
        if (wasSounding_)
        {
            flushActiveNotes(midi, 0);
            wasSounding_ = false;
            lastBeatPos_ = -1.0;
        }
        if (recordMonitorFx.load())
            liveFx_.process(midi, sampleRate_, hostBpm.load(), hostBeatPos.load(),
                            buffer.getNumSamples(), true);
        return;
    }

    const bool pass = passthroughActive.load();
    juce::MidiBuffer flushEvents;
    if (liveFlushPending_.exchange(false))
        liveFx_.flush(flushEvents, 0);

    if (pass)
    {
        // Passthrough replaces clip preview: transform host MIDI in place.
        if (wasSounding_)
        {
            flushActiveNotes(flushEvents, 0);
            wasSounding_ = false;
            lastBeatPos_ = -1.0;
        }
        liveFx_.process(midi, sampleRate_, hostBpm.load(), hostBeatPos.load(),
                        buffer.getNumSamples(), hostPlaying.load());
        for (const auto metadata : flushEvents)
            midi.addEvent(metadata.getMessage(), metadata.samplePosition);
        return;
    }

    // Clip-preview path: discard host MIDI, keep any live-note release events.
    midi.clear();
    for (const auto metadata : flushEvents)
        midi.addEvent(metadata.getMessage(), metadata.samplePosition);

    const bool synced = syncToHost.load();
    const bool hostIsPlaying = hostPlaying.load();
    // Sync mode: DAW transport arms preview on play so host start/stop drives
    // the plugin without a separate Play click in the UI.
    if (synced && hostIsPlaying && !wasHostPlaying_)
        previewArmed.store(true);
    wasHostPlaying_ = hostIsPlaying;

    const bool sounding = previewArmed.load() && (!synced || hostIsPlaying);

    if (!sounding)
    {
        // Release held notes once on the playing → stopped transition; stay
        // silent afterwards so downstream instruments aren't spammed.
        if (wasSounding_)
            flushActiveNotes(midi, 0);
        wasSounding_ = false;
        lastBeatPos_ = -1.0;
        return;
    }
    wasSounding_ = true;

    const double mult = juce::jlimit(0.25, 4.0, bpmMultiplier.load());
    double bpm = synced ? hostBpm.load() * mult : freeBpm.load();
    double beatPos = synced ? hostBeatPos.load() * mult : freerunBeat.load();
    if (bpm <= 0.0) bpm = 120.0;

    const double secPerBeat = 60.0 / bpm;
    const double beatsPerSample = 1.0 / (sampleRate_ * secPerBeat);
    const int numSamples = buffer.getNumSamples();
    const double blockBeats = (double) numSamples * beatsPerSample;
    const double endBeat = beatPos + blockBeats;

    juce::MidiBuffer generated;

    // Only flush on unexpected seeks. Free-run stores a wrapped clock in
    // freerunBeat / lastBeatPos_, so loop wraps must not look like jumps.
    if (lastBeatPos_ >= 0.0 && std::abs(beatPos - lastBeatPos_) > beatsPerSample * 2.0)
        flushActiveNotes(generated, 0);

    bool previewMuted = true, previewHasClip = false, flushHeldNotes = false;
    MidiClip previewClip;
    {
        juce::ScopedLock pl(previewLock_);
        previewMuted = previewMuted_;
        previewHasClip = previewHasClip_;
        previewClip = previewClip_;
        flushHeldNotes = previewFlushPending_;
        previewFlushPending_ = false;
    }

    if (flushHeldNotes)
        flushActiveNotes(generated, 0);

    if (!previewMuted && previewHasClip && previewClip.lengthBeats > 0.0)
    {
        const double sessionLenBeats = (double) (juce::jmax(1, syncSessionBars.load()) * 4);
        const double clipLen = juce::jmax(0.25, previewClip.lengthBeats);

        // Piano-roll loop region in preview beats. Invalid → full clip.
        double loopStart = previewLoopStartBeat.load();
        double loopEnd = previewLoopEndBeat.load();
        if (loopEnd <= loopStart + 1.0e-9)
        {
            loopStart = 0.0;
            loopEnd = clipLen;
        }
        loopStart = juce::jlimit(0.0, clipLen, loopStart);
        loopEnd = juce::jlimit(loopStart + 0.25, clipLen, loopEnd);
        const double loopLen = loopEnd - loopStart;
        const bool customLoop = loopStart > 1.0e-9 || loopEnd < clipLen - 1.0e-9;

        auto wrapIntoLoop = [&](double pos) -> double
        {
            double local = pos;
            if (local < loopStart || local >= loopEnd)
                local = loopStart + std::fmod(std::max(0.0, local - loopStart), loopLen);
            else
                local = loopStart + std::fmod(local - loopStart, loopLen);
            if (local < loopStart) local += loopLen;
            if (local >= loopEnd) local = loopStart;
            return local;
        };

        auto renderLoopingBlock = [&](double pStart)
        {
            double pEnd = pStart + blockBeats;
            if (pEnd > loopEnd + 1.0e-12)
            {
                const double firstLen = loopEnd - pStart;
                const int firstSamples = juce::jlimit(1, numSamples,
                    (int) std::lround(firstLen / blockBeats * (double) numSamples));
                generatePreviewMidi(previewClip, pStart, loopEnd, generated, firstSamples, 0);
                // Partial loop regions hard-cut hanging notes at the boundary.
                if (customLoop)
                    flushActiveNotes(generated, juce::jmax(0, firstSamples - 1));
                const double remain = pEnd - loopEnd;
                const int secondSamples = numSamples - firstSamples;
                if (secondSamples > 0 && remain > 1.0e-12)
                    generatePreviewMidi(previewClip, loopStart, loopStart + remain,
                                        generated, secondSamples, firstSamples);
            }
            else
            {
                generatePreviewMidi(previewClip, pStart, pEnd, generated, numSamples);
            }

            double next = pStart + blockBeats;
            if (next >= loopEnd)
                next = loopStart + std::fmod(next - loopStart, loopLen);
            return next;
        };

        if (!synced)
        {
            const double pStart = wrapIntoLoop(beatPos);
            const double next = renderLoopingBlock(pStart);
            freerunBeat.store(next);
            lastBeatPos_ = next;
        }
        else
        {
            // Map host (or session/host-loop) phase into the clip, then into
            // the piano-roll loop region so handles define the looping length.
            double phase = beatPos;
            if (mult != 1.0)
            {
                phase = std::fmod(beatPos, clipLen);
                if (phase < 0.0) phase += clipLen;
            }
            else if (hostLoopActive.load())
            {
                const double ls = hostLoopPpqStart.load();
                const double le = hostLoopPpqEnd.load();
                const double hostLoopLen = le - ls;
                if (hostLoopLen > 1.0e-6 && beatPos + 1.0e-9 >= ls)
                    phase = ls + std::fmod(beatPos - ls, hostLoopLen);
                else if (sessionLenBeats > 1.0e-9)
                {
                    phase = std::fmod(beatPos, sessionLenBeats);
                    if (phase < 0.0) phase += sessionLenBeats;
                }
            }
            else if (sessionLenBeats > 1.0e-9)
            {
                phase = std::fmod(beatPos, sessionLenBeats);
                if (phase < 0.0) phase += sessionLenBeats;
            }

            const double pStart = wrapIntoLoop(phase);
            renderLoopingBlock(pStart);
            lastBeatPos_ = endBeat;
        }
    }
    else if (synced)
    {
        lastBeatPos_ = endBeat;
    }

    for (const auto metadata : generated)
        midi.addEvent(metadata.getMessage(), metadata.samplePosition);
}

namespace {

// Cheap content fingerprint so a changed preview (new file, transposed notes,
// new groove timing) releases anything still sounding from the old one.
juce::uint64 previewFingerprint(const MidiClip& clip)
{
    juce::uint64 h = (juce::uint64) clip.notes.size() * 1099511628211ULL;
    for (const auto& n : clip.notes)
    {
        h = h * 1099511628211ULL
            ^ ((juce::uint64) n.noteNumber << 48)
            ^ ((juce::uint64) n.velocity << 40)
            ^ ((juce::uint64) juce::jmax<juce::int64>(0, (juce::int64) std::llround(n.startBeat * 960.0)))
            ^ ((juce::uint64) juce::jmax<juce::int64>(0, (juce::int64) std::llround(n.lengthBeats * 960.0)) << 20);
    }
    return h;
}

} // namespace

void MidiToolkitProcessor::setPreviewState(const MidiClip& clip, bool hasClip, bool muted, bool soloed,
                                           bool softUpdate)
{
    const auto fp = previewFingerprint(clip);
    juce::ScopedLock sl(previewLock_);
    // Hard updates (new file / clear) release held notes. Soft updates keep
    // sounding notes alive so slider drags don't click on every tick.
    if (!softUpdate && previewHasClip_ && (fp != previewFingerprint_ || !hasClip))
        previewFlushPending_ = true;
    else if (!hasClip && previewHasClip_)
        previewFlushPending_ = true;
    previewFingerprint_ = fp;
    previewClip_ = clip;
    previewHasClip_ = hasClip;
    previewMuted_ = muted;
    juce::ignoreUnused(soloed);
}

void MidiToolkitProcessor::setLiveFxState(const LiveFxState& state)
{
    liveFx_.setState(state);
}

void MidiToolkitProcessor::setRecordingEnabled(bool enabled)
{
    if (enabled)
    {
        requestNoteFlush();
        recorder.start();
        recordingActive.store(true);
        return;
    }
    recorder.stop();
    recordingActive.store(false);
}

void MidiToolkitProcessor::setPassthroughEnabled(bool enabled)
{
    const bool was = passthroughActive.load();
    passthrough = enabled;
    passthroughActive.store(enabled);
    if (was != enabled)
    {
        // Leaving either mode: release whatever that path was holding.
        requestNoteFlush();
        if (!enabled)
            liveFlushPending_.store(true);
        else
        {
            juce::ScopedLock sl(previewLock_);
            previewFlushPending_ = true;
        }
    }
}

void MidiToolkitProcessor::generatePreviewMidi(const MidiClip& clip, double startBeat, double endBeat,
                                               juce::MidiBuffer& output, int numSamples,
                                               int sampleOffsetBase)
{
    const double clipLen = std::max(0.25, clip.lengthBeats);
    const double blockLen = endBeat - startBeat;
    if (blockLen <= 0.0) return;

    auto emitAt = [&](double globalBeat, const juce::MidiMessage& msg)
    {
        if (globalBeat < startBeat || globalBeat >= endBeat) return;
        const double fraction = (globalBeat - startBeat) / blockLen;
        const int sampleOffset = sampleOffsetBase
            + juce::jlimit(0, numSamples - 1, (int) (fraction * (double) numSamples));
        output.addEvent(msg, sampleOffset);
    };

    // Sustain / other clip automation (CC), looped with the clip.
    for (const auto& entry : clip.automation)
    {
        const int cc = entry.first;
        for (const auto& pt : entry.second)
        {
            const int kMin = juce::jmax(0, (int) std::floor((startBeat - pt.beat) / clipLen));
            const int kMax = (int) std::ceil((endBeat - pt.beat) / clipLen);
            for (int k = kMin; k <= kMax; ++k)
            {
                const double globalBeat = (double) k * clipLen + pt.beat;
                emitAt(globalBeat,
                       juce::MidiMessage::controllerEvent(1, cc, juce::jlimit(0, 127, pt.value)));
            }
        }
    }

    for (const auto& note : clip.notes)
    {
        double noteStart = note.startBeat - clip.clipStartOffset;
        if (noteStart < 0.0) noteStart += clipLen;
        const double noteEnd = noteStart + note.lengthBeats;

        const int kMin = juce::jmax(0, (int) std::floor((startBeat - noteEnd) / clipLen) + 1);
        const int kMax = (int) std::ceil((endBeat - noteStart) / clipLen);
        for (int k = kMin; k <= kMax; ++k)
        {
            const double globalStart = (double) k * clipLen + noteStart;
            const double globalEnd = (double) k * clipLen + noteEnd;
            if (globalEnd <= startBeat || globalStart >= endBeat) continue;

            if (globalStart >= startBeat && globalStart < endBeat)
            {
                const int pitch = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset);
                emitAt(globalStart,
                       juce::MidiMessage::noteOn(note.channel, pitch, (juce::uint8) note.velocity));
                activeNotes_[juce::jlimit(1, 16, note.channel) - 1][pitch] = true;
            }
            if (globalEnd >= startBeat && globalEnd <= endBeat + 1.0e-12)
            {
                const int pitch = juce::jlimit(0, 127, note.noteNumber + clip.rootNoteOffset);
                emitAt(globalEnd, juce::MidiMessage::noteOff(note.channel, pitch));
                activeNotes_[juce::jlimit(1, 16, note.channel) - 1][pitch] = false;
            }
        }
    }
}

void MidiToolkitProcessor::flushActiveNotes(juce::MidiBuffer& output, int samplePosition)
{
    // Explicit note-offs for every held note (some instruments ignore CC123),
    // then all-notes-off + sustain-off as belt and braces on every channel.
    for (int ch = 0; ch < 16; ++ch)
    {
        for (int n = 0; n < 128; ++n)
            if (activeNotes_[ch][n])
            {
                output.addEvent(juce::MidiMessage::noteOff(ch + 1, n), samplePosition);
                activeNotes_[ch][n] = false;
            }
        output.addEvent(juce::MidiMessage::controllerEvent(ch + 1, 64, 0), samplePosition);  // sustain off
        output.addEvent(juce::MidiMessage::controllerEvent(ch + 1, 123, 0), samplePosition); // all notes off
        output.addEvent(juce::MidiMessage::allNotesOff(ch + 1), samplePosition);
    }
}

void MidiToolkitProcessor::addSavedBrowserDir(const juce::String& path)
{
    SavedLibraryFolder folder;
    folder.path = path;
    saveLibraryFolder(folder, false);
}

void MidiToolkitProcessor::saveLibraryFolder(const SavedLibraryFolder& incoming, bool replaceDetails)
{
    const juce::File dir(incoming.path);
    if (!dir.isDirectory()) return;
    const auto fullPath = dir.getFullPathName();

    SavedLibraryFolder kept;
    bool had = false;
    for (const auto& folder : savedLibraries)
        if (folder.path == fullPath)
        {
            kept = folder;
            had = true;
            break;
        }

    SavedLibraryFolder next = incoming;
    next.path = fullPath;
    if (had && !replaceDetails)
    {
        next.scanSubdirs = kept.scanSubdirs;
        next.filter = kept.filter;
    }

    if (had && replaceDetails)
    {
        for (auto& folder : savedLibraries)
            if (folder.path == fullPath)
            {
                folder = next;
                break;
            }
    }
    else
    {
        savedLibraries.erase(std::remove_if(savedLibraries.begin(), savedLibraries.end(),
                                             [&](const SavedLibraryFolder& folder)
                                             { return folder.path == fullPath; }),
                             savedLibraries.end());
        savedLibraries.insert(savedLibraries.begin(), next);
    }
    while ((int) savedLibraries.size() > 24)
        savedLibraries.pop_back();

    savedBrowserDirs.clear();
    for (const auto& folder : savedLibraries)
        savedBrowserDirs.add(folder.path);
    persistSharedUiSession();
}

void MidiToolkitProcessor::removeSavedBrowserDir(const juce::String& path)
{
    const auto full = juce::File(path).getFullPathName();
    savedBrowserDirs.removeString(path);
    if (full != path)
        savedBrowserDirs.removeString(full);
    savedLibraries.erase(std::remove_if(savedLibraries.begin(), savedLibraries.end(),
                                         [&](const SavedLibraryFolder& folder)
                                         { return folder.path == path || folder.path == full; }),
                         savedLibraries.end());
    persistSharedUiSession();
}

const SavedLibraryFolder* MidiToolkitProcessor::libraryFolderFor(const juce::String& path) const
{
    const auto full = juce::File(path).getFullPathName();
    for (const auto& folder : savedLibraries)
        if (folder.path == path || folder.path == full)
            return &folder;
    return nullptr;
}

void MidiToolkitProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("MidiToolkitState");
    xml.setAttribute("version", 5);
    xml.setAttribute("tweakDensity", tweaks().density.load());
    xml.setAttribute("tweakSize", tweaks().size.load());
    xml.setAttribute("tweakTextScalePct", tweaks().textScalePct.load());
    xml.setAttribute("tweakTextScaleV2", 1);
    xml.setAttribute("tweakAppearance", tweaks().appearance.load());
    xml.setAttribute("tweakShowTooltips", tweaks().showTooltips.load());
    xml.setAttribute("syncSessionBars", syncSessionBars.load());
    xml.setAttribute("lastBrowserDir", lastBrowserDir);
    xml.setAttribute("trimEmptyMeasuresPreview", trimEmptyMeasuresPreview ? 1 : 0);
    xml.setAttribute("syncToHost", syncToHost.load() ? 1 : 0);
    xml.setAttribute("freeBpm", freeBpm.load());
    xml.setAttribute("bpmMultiplier", bpmMultiplier.load());
    xml.setAttribute("editorOpen", editorOpen ? 1 : 0);
    xml.setAttribute("effectsOpen", effectsOpen ? 1 : 0);
    xml.setAttribute("previewOpen", previewOpen ? 1 : 0);
    xml.setAttribute("sidebarCollapsed", sidebarCollapsed ? 1 : 0);
    xml.setAttribute("passthrough", passthrough ? 1 : 0);
    xml.setAttribute("browseMode", browseMode);
    xml.setAttribute("starredFilter", starredFilter ? 1 : 0);
    xml.setAttribute("includeSubdirs", includeSubdirs ? 1 : 0);
    xml.setAttribute("selectedClipPath", selectedClipPath);
    xml.setAttribute("activeSavedSearchIdx", activeSavedSearchIdx);
    {
        auto* session = xml.createNewChildElement("BrowserSession");
        LibraryStore::browserSearchToXml(*session, browserSessionSearch);
        for (const auto& path : browserResultPaths)
        {
            if (path.isEmpty()) continue;
            auto* child = session->createNewChildElement("Result");
            child->setAttribute("path", path);
        }
    }
    xml.setAttribute("colKey", columnVisibility.key ? 1 : 0);
    xml.setAttribute("colTempo", columnVisibility.tempo ? 1 : 0);
    xml.setAttribute("colBars", columnVisibility.bars ? 1 : 0);
    xml.setAttribute("colKind", columnVisibility.kind ? 1 : 0);
    xml.setAttribute("colComplexity", columnVisibility.complexity ? 1 : 0);
    xml.setAttribute("colDifNotes", columnVisibility.difNotes ? 1 : 0);
    xml.setAttribute("colTimeSig", columnVisibility.timeSig ? 1 : 0);
    xml.setAttribute("colNotes", columnVisibility.notes ? 1 : 0);
    xml.setAttribute("nameColumnWidth", nameColumnWidth);
    syncLockFlagsFromSections();
    xml.setAttribute("sectionLocks", (int) sectionLocks);
    xml.setAttribute("editLock", editLock ? 1 : 0);
    xml.setAttribute("effectsLock", effectsLock ? 1 : 0);
    xml.setAttribute("lockAutoTrim", lockAutoTrim ? 1 : 0);
    xml.setAttribute("lockOctaveAbs", 1);
    xml.setAttribute("lockOctave", lockedEdit.octave);
    xml.setAttribute("lockPitchShift", lockedEdit.pitchShift);
    xml.setAttribute("lockOctaveRange", lockedEdit.octaveRange);
    xml.setAttribute("lockPitchMin", lockedEdit.pitchMin);
    xml.setAttribute("lockPitchMax", lockedEdit.pitchMax);
    xml.setAttribute("lockExtendMult", lockedEdit.extendMult);
    xml.setAttribute("lockFitScale", lockedEdit.fitScale ? 1 : 0);
    xml.setAttribute("lockMapToRoot", lockedEdit.mapToRoot ? 1 : 0);
    xml.setAttribute("lockRoot", lockedEdit.root);
    xml.setAttribute("lockMode", (int) lockedEdit.mode);
    xml.setAttribute("lockNoteFilterMask", (int) lockedEdit.noteFilterMask);
    xml.setAttribute("lockNoteFilterType", (int) lockedEdit.noteFilterType);
    xml.setAttribute("lockSwing", lockedGroove.swing);
    xml.setAttribute("lockPocket", lockedGroove.pocket);
    xml.setAttribute("lockHumanize", lockedGroove.humanize);
    xml.setAttribute("lockDynamics", lockedGroove.dynamics);
    xml.setAttribute("lockLength", lockedGroove.length);
    xml.setAttribute("lockIntensity", lockedGroove.intensity);
    xml.setAttribute("lockSwingGrid", lockedGroove.swingGridIndex);
    xml.setAttribute("lockQuantizeGrid", lockedGroove.quantizeGridIndex);
    xml.setAttribute("lockQuantizeStrength", lockedGroove.quantizeStrength);
    xml.setAttribute("lockArticulation", lockedGroove.articulationIndex);
    xml.setAttribute("lockArticulationV", 1);
    xml.setAttribute("lockArticulationStrength", lockedGroove.articulationStrength);
    xml.setAttribute("lockSustainPedal", lockedGroove.sustainPedalMode);
    xml.setAttribute("lockComplexityTarget", lockedGroove.complexityTarget);
    xml.setAttribute("lockDelayTime", lockedGroove.delayTimeIndex);
    xml.setAttribute("lockDelayAmount", lockedGroove.delayAmount);
    xml.setAttribute("lockDelayFeedback", lockedGroove.delayFeedback);
    xml.setAttribute("lockArpMode", lockedGroove.arpModeIndex);
    xml.setAttribute("lockArpRate", lockedGroove.arpRateIndex);
    xml.setAttribute("lockArpGate", lockedGroove.arpGate);
    xml.setAttribute("lockArpOctaves", lockedGroove.arpOctaves);
    xml.setAttribute("lockStrumDir", lockedGroove.strumDirectionIndex);
    xml.setAttribute("lockStrumSpeed", lockedGroove.strumSpeed);
    xml.setAttribute("lockStrumAmount", lockedGroove.strumAmount);
    xml.setAttribute("lockVelRangeLo", lockedGroove.velocityRangeLo);
    xml.setAttribute("lockVelRangeHi", lockedGroove.velocityRangeHi);
    xml.setAttribute("lockVariation", lockedGroove.variationIndex);
    if (!savedLibraries.empty())
    {
        for (const auto& folder : savedLibraries)
        {
            if (folder.path.isEmpty()) continue;
            auto* child = xml.createNewChildElement("SavedFolder");
            LibraryStore::savedLibraryToXml(*child, folder);
        }
    }
    else
    {
        for (const auto& folder : savedBrowserDirs)
        {
            if (folder.isNotEmpty())
            {
                auto* child = xml.createNewChildElement("SavedFolder");
                child->setAttribute("path", folder);
            }
        }
    }
    for (const auto& star : starredFiles)
    {
        if (star.isNotEmpty())
        {
            auto* child = xml.createNewChildElement("StarredFile");
            child->setAttribute("path", star);
        }
    }
    for (const auto& ss : savedSearches)
    {
        if (ss.name.isEmpty()) continue;
        auto* child = xml.createNewChildElement("SavedSearch");
        child->setAttribute("name", ss.name);
        if (ss.rootPath.isNotEmpty())
            child->setAttribute("root", ss.rootPath);
        LibraryStore::browserSearchToXml(*child, ss.search);
        for (const auto& p : ss.resultPaths)
        {
            if (p.isEmpty()) continue;
            auto* pathEl = child->createNewChildElement("Path");
            pathEl->setAttribute("value", p);
        }
    }

    for (const auto& [path, edit] : clipEdits)
    {
        if (path.isEmpty() || editIsClean(edit))
            continue;
        auto* e = xml.createNewChildElement("ClipEdit");
        e->setAttribute("path", path);
        e->setAttribute("octaveAbs", 1);
        e->setAttribute("octave", edit.octave);
        e->setAttribute("pitchShift", edit.pitchShift);
        e->setAttribute("octaveRange", edit.octaveRange);
        e->setAttribute("pitchMin", edit.pitchMin);
        e->setAttribute("pitchMax", edit.pitchMax);
        e->setAttribute("extendMult", edit.extendMult);
        e->setAttribute("fitScale", edit.fitScale ? 1 : 0);
        e->setAttribute("mapToRoot", edit.mapToRoot ? 1 : 0);
        e->setAttribute("root", edit.root);
        e->setAttribute("mode", (int) edit.mode);
        e->setAttribute("noteFilterMask", (int) edit.noteFilterMask);
        e->setAttribute("noteFilterType", (int) edit.noteFilterType);
        if (!edit.removedBars.empty())
        {
            juce::StringArray parts;
            for (int b : edit.removedBars)
                parts.add(juce::String(b));
            e->setAttribute("removedBars", parts.joinIntoString(","));
        }
        else if (edit.legacyTrimLead > 0 || edit.legacyTrimTail > 0)
        {
            e->setAttribute("trimLead", edit.legacyTrimLead);
            e->setAttribute("trimTail", edit.legacyTrimTail);
        }
        for (const auto& [id, mv] : edit.moves)
        {
            if (mv.dPitch == 0 && mv.dStep == 0) continue;
            auto* m = e->createNewChildElement("Move");
            m->setAttribute("id", id);
            m->setAttribute("dPitch", mv.dPitch);
            m->setAttribute("dStep", mv.dStep);
        }
        for (const auto& [id, vel] : edit.velocities)
        {
            auto* v = e->createNewChildElement("Vel");
            v->setAttribute("id", id);
            v->setAttribute("v", vel);
        }
        for (int id : edit.deleted)
        {
            auto* d = e->createNewChildElement("Del");
            d->setAttribute("id", id);
        }
    }
    for (const auto& [path, k] : clipGrooves)
    {
        if (path.isEmpty() || k.isDefault())
            continue;
        auto* e = xml.createNewChildElement("ClipGroove");
        e->setAttribute("path", path);
        e->setAttribute("swing", k.swing);
        e->setAttribute("pocket", k.pocket);
        e->setAttribute("humanize", k.humanize);
        e->setAttribute("dynamics", k.dynamics);
        e->setAttribute("length", k.length);
        e->setAttribute("intensity", k.intensity);
        e->setAttribute("swingBase", (int) k.swingBase);
        e->setAttribute("swingGrid", k.swingGridIndex);
        e->setAttribute("quantizeGrid", k.quantizeGridIndex);
        e->setAttribute("quantizeStrength", k.quantizeStrength);
        e->setAttribute("articulation", k.articulationIndex);
        e->setAttribute("articulationV", 1);
        e->setAttribute("articulationStrength", k.articulationStrength);
        e->setAttribute("sustainPedal", k.sustainPedalMode);
        e->setAttribute("complexityTarget", k.complexityTarget);
        e->setAttribute("delayTime", k.delayTimeIndex);
        e->setAttribute("delayAmount", k.delayAmount);
        e->setAttribute("delayFeedback", k.delayFeedback);
        e->setAttribute("arpMode", k.arpModeIndex);
        e->setAttribute("arpRate", k.arpRateIndex);
        e->setAttribute("arpGate", k.arpGate);
        e->setAttribute("arpOctaves", k.arpOctaves);
        e->setAttribute("strumDir", k.strumDirectionIndex);
        e->setAttribute("strumSpeed", k.strumSpeed);
        e->setAttribute("strumAmount", k.strumAmount);
        e->setAttribute("velRangeLo", k.velocityRangeLo);
        e->setAttribute("velRangeHi", k.velocityRangeHi);
        e->setAttribute("variation", k.variationIndex);
    }
    copyXmlToBinary(xml, dest);
}

void MidiToolkitProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    lastBrowserDir.clear();
    savedBrowserDirs.clear();
    savedLibraries.clear();
    // Favorites/searches accrue in Application Support — do not clear them here.
    // Host state is merged into the library at the end of this load.
    juce::StringArray hostStars;
    std::vector<SavedSearchEntry> hostSearches;
    trimEmptyMeasuresPreview = false;
    syncSessionBars.store(4);
    clipEdits.clear();
    clipGrooves.clear();
    browseMode = 0;
    starredFilter = false;
    includeSubdirs = false;
    browserSessionSearch = {};
    selectedClipPath.clear();
    activeSavedSearchIdx = -1;
    browserResultPaths.clear();
    if (data == nullptr || sizeInBytes <= 0)
    {
        library_.mergeStarsFromDisk();
        applyLibraryToMemory();
        applySharedUiSessionFromLibrary();
        return;
    }
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName("MidiToolkitState") || xml->hasTagName("MidiBrowserState") || xml->hasTagName("PatternFlowState"))
        {
            tweaks().density.store(juce::jlimit(0, 1,
                xml->getIntAttribute("tweakDensity", (int) Density::Comfortable)));
            tweaks().size.store(juce::jlimit(0, kNumContentSizes - 1,
                xml->getIntAttribute("tweakSize", (int) ContentSize::Medium)));
            {
                // v2: 100% == former 115% size. Remap legacy absolute percents once.
                const int raw = xml->getIntAttribute("tweakTextScalePct", 100);
                const int pct = xml->getIntAttribute("tweakTextScaleV2", 0) != 0
                    ? raw
                    : juce::roundToInt((double) raw * 100.0 / 115.0);
                tweaks().textScalePct.store(juce::jlimit(60, 150, pct));
            }
            tweaks().appearance.store(juce::jlimit(0, kNumAppearances - 1,
                xml->getIntAttribute("tweakAppearance", (int) Appearance::System)));
            tweaks().showTooltips.store(xml->getIntAttribute("tweakShowTooltips", 1) != 0 ? 1 : 0);
            syncSessionBars.store(juce::jlimit(1, 256, xml->getIntAttribute("syncSessionBars",
                xml->getIntAttribute("arrangementBars", syncSessionBars.load()))));
            lastBrowserDir = xml->getStringAttribute("lastBrowserDir");
            trimEmptyMeasuresPreview = xml->getIntAttribute("trimEmptyMeasuresPreview", 0) != 0;
            syncToHost.store(xml->getIntAttribute("syncToHost", 1) != 0);
            freeBpm.store(juce::jlimit(20.0, 300.0, xml->getDoubleAttribute("freeBpm", 124.0)));
            bpmMultiplier.store(juce::jlimit(0.25, 4.0, xml->getDoubleAttribute("bpmMultiplier", 1.0)));
            editorOpen = xml->getIntAttribute("editorOpen", 0) != 0;
            effectsOpen = xml->getIntAttribute("effectsOpen", 0) != 0;
            previewOpen = xml->getIntAttribute("previewOpen", 1) != 0;
            sidebarCollapsed = xml->getIntAttribute("sidebarCollapsed", 0) != 0;
            passthrough = xml->getIntAttribute("passthrough", 0) != 0;
            passthroughActive.store(passthrough);
            browseMode = juce::jlimit(0, 2, xml->getIntAttribute("browseMode", 0));
            starredFilter = xml->getIntAttribute("starredFilter", 0) != 0;
            includeSubdirs = xml->getIntAttribute("includeSubdirs", 0) != 0;
            selectedClipPath = xml->getStringAttribute("selectedClipPath");
            activeSavedSearchIdx = xml->getIntAttribute("activeSavedSearchIdx", -1);
            if (auto* session = xml->getChildByName("BrowserSession"))
            {
                browserSessionSearch = LibraryStore::browserSearchFromXml(*session);
                browserResultPaths.clear();
                for (auto* child : session->getChildIterator())
                {
                    if (!child->hasTagName("Result")) continue;
                    const auto path = child->getStringAttribute("path");
                    if (path.isNotEmpty())
                        browserResultPaths.add(path);
                }
            }
            columnVisibility.key = xml->getIntAttribute("colKey", 1) != 0;
            columnVisibility.tempo = xml->getIntAttribute("colTempo", 1) != 0;
            columnVisibility.bars = xml->getIntAttribute("colBars", 1) != 0;
            columnVisibility.kind = xml->getIntAttribute("colKind", 1) != 0;
            columnVisibility.complexity = xml->getIntAttribute("colComplexity", 0) != 0;
            columnVisibility.difNotes = xml->getIntAttribute("colDifNotes", 0) != 0;
            columnVisibility.timeSig = xml->getIntAttribute("colTimeSig", 0) != 0;
            columnVisibility.notes = xml->getIntAttribute("colNotes", 0) != 0;
            nameColumnWidth = juce::jlimit(120, 2400, xml->getIntAttribute("nameColumnWidth", 140));
            editLock = xml->getIntAttribute("editLock", 0) != 0;
            effectsLock = xml->getIntAttribute("effectsLock", 0) != 0;
            if (xml->hasAttribute("sectionLocks"))
                sectionLocks = (uint32_t) xml->getIntAttribute("sectionLocks", 0) & toolkitLock::All;
            else
            {
                // Migrate pre-sectionLocks presets.
                sectionLocks = 0;
                if (effectsLock)
                    sectionLocks |= toolkitLock::AnyGroove | toolkitLock::Playback;
                if (editLock)
                    sectionLocks |= toolkitLock::Pitch;
            }
            syncLockFlagsFromSections();
            lockAutoTrim = xml->getIntAttribute("lockAutoTrim", 0) != 0;
            lockedEdit = ClipEdit();
            if (xml->hasAttribute("lockOctaveAbs"))
                lockedEdit.octave = juce::jlimit(-1, 6, xml->getIntAttribute("lockOctave", -1));
            else
            {
                const int rel = juce::jlimit(-3, 3, xml->getIntAttribute("lockOctave", 0));
                lockedEdit.octave = rel == 0 ? -1 : juce::jlimit(0, 6, 4 + rel);
            }
            lockedEdit.pitchShift = juce::jlimit(-12, 12, xml->getIntAttribute("lockPitchShift", 0));
            lockedEdit.octaveRange = juce::jlimit(0, 3, xml->getIntAttribute("lockOctaveRange", 0));
            lockedEdit.pitchMin = juce::jlimit(0, 127, xml->getIntAttribute("lockPitchMin", 0));
            lockedEdit.pitchMax = juce::jlimit(0, 127, xml->getIntAttribute("lockPitchMax", 127));
            if (lockedEdit.pitchMax < lockedEdit.pitchMin)
                std::swap(lockedEdit.pitchMin, lockedEdit.pitchMax);
            {
                const int em = xml->getIntAttribute("lockExtendMult", 1);
                lockedEdit.extendMult = (em == 2 || em == 4 || em == 8) ? em : 1;
            }
            lockedEdit.fitScale = xml->getIntAttribute("lockFitScale", 0) != 0;
            lockedEdit.mapToRoot = xml->getIntAttribute("lockMapToRoot", 0) != 0;
            lockedEdit.root = juce::jlimit(-1, 11, xml->getIntAttribute("lockRoot", -1));
            lockedEdit.mode = (Mode) juce::jlimit(0, kNumModes - 1,
                xml->getIntAttribute("lockMode", (int) Mode::Ionian));
            {
                const int mask = xml->getIntAttribute("lockNoteFilterMask", 0x0FFF);
                lockedEdit.noteFilterMask = (uint16_t) juce::jlimit(1, 0x0FFF, mask & 0x0FFF);
                if (lockedEdit.noteFilterMask == 0)
                    lockedEdit.noteFilterMask = 0x0FFF;
                lockedEdit.noteFilterType = xml->getIntAttribute("lockNoteFilterType", 0) != 0
                    ? NoteFilterType::Fold : NoteFilterType::Mute;
            }
            lockedGroove = GrooveParams();
            lockedGroove.set(0, xml->getIntAttribute("lockSwing", 0));
            lockedGroove.set(1, xml->getIntAttribute("lockPocket", 0));
            lockedGroove.set(2, xml->getIntAttribute("lockHumanize", 0));
            lockedGroove.set(3, xml->getIntAttribute("lockDynamics", 0));
            lockedGroove.set(4, xml->getIntAttribute("lockLength", 100));
            lockedGroove.set(5, xml->getIntAttribute("lockIntensity", 100));
            lockedGroove.swingGridIndex = juce::jlimit(0, 5,
                xml->getIntAttribute("lockSwingGrid", xml->getIntAttribute("lockSwingBase", 0) != 0 ? 0 : 1));
            lockedGroove.swingBase = lockedGroove.swingGridIndex == 0
                                          ? SwingBase::Sixteenth : SwingBase::Eighth;
            lockedGroove.quantizeGridIndex = juce::jlimit(0, (int) QuantizeGrid::Count - 1,
                xml->getIntAttribute("lockQuantizeGrid", (int) QuantizeGrid::Eighth));
            lockedGroove.quantizeStrength = juce::jlimit(0, 100,
                xml->getIntAttribute("lockQuantizeStrength", 0));
            lockedGroove.articulationIndex = juce::jlimit(0, (int) Articulation::Count - 1,
                xml->getIntAttribute("lockArticulation", 0));
            // Pre-Off enum: 0 was Legato. New saves stamp lockArticulationV=1.
            if (xml->getIntAttribute("lockArticulationV", 0) == 0)
                lockedGroove.articulationIndex = juce::jlimit(0, (int) Articulation::Count - 1,
                    lockedGroove.articulationIndex + 1);
            lockedGroove.articulationStrength = juce::jlimit(0, 100,
                xml->getIntAttribute("lockArticulationStrength", 0));
            lockedGroove.sustainPedalMode = juce::jlimit(0, (int) SustainPedalMode::Count - 1,
                xml->getIntAttribute("lockSustainPedal", 0));
            lockedGroove.complexityTarget = xml->getIntAttribute("lockComplexityTarget", -1);
            if (lockedGroove.complexityTarget >= 0)
                lockedGroove.complexityTarget = juce::jlimit(0, 100, lockedGroove.complexityTarget);
            lockedGroove.delayTimeIndex = juce::jlimit(0, (int) DelayTime::Count - 1,
                xml->getIntAttribute("lockDelayTime", (int) DelayTime::Eighth));
            lockedGroove.delayAmount = juce::jlimit(0, 100, xml->getIntAttribute("lockDelayAmount", 0));
            lockedGroove.delayFeedback = juce::jlimit(0, 100, xml->getIntAttribute("lockDelayFeedback", 40));
            lockedGroove.arpModeIndex = juce::jlimit(0, (int) ArpMode::Count - 1,
                xml->getIntAttribute("lockArpMode", 0));
            lockedGroove.arpRateIndex = juce::jlimit(0, (int) DelayTime::Count - 1,
                xml->getIntAttribute("lockArpRate", (int) DelayTime::Sixteenth));
            lockedGroove.arpGate = juce::jlimit(10, 100, xml->getIntAttribute("lockArpGate", 70));
            lockedGroove.arpOctaves = juce::jlimit(1, 4, xml->getIntAttribute("lockArpOctaves", 1));
            lockedGroove.strumDirectionIndex = juce::jlimit(0, (int) StrumDirection::Count - 1,
                xml->getIntAttribute("lockStrumDir", 0));
            lockedGroove.strumSpeed = juce::jlimit(0, 100, xml->getIntAttribute("lockStrumSpeed", 40));
            lockedGroove.strumAmount = juce::jlimit(0, 100, xml->getIntAttribute("lockStrumAmount", 0));
            lockedGroove.velocityRangeLo = juce::jlimit(1, 127, xml->getIntAttribute("lockVelRangeLo", 1));
            lockedGroove.velocityRangeHi = juce::jlimit(1, 127, xml->getIntAttribute("lockVelRangeHi", 127));
            if (lockedGroove.velocityRangeHi < lockedGroove.velocityRangeLo)
                std::swap(lockedGroove.velocityRangeLo, lockedGroove.velocityRangeHi);
            lockedGroove.variationIndex = juce::jlimit(0, 16, xml->getIntAttribute("lockVariation", 0));
            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName("SavedFolder"))
                {
                    auto folder = LibraryStore::savedLibraryFromXml(*child);
                    if (folder.path.isNotEmpty() && juce::File(folder.path).isDirectory()
                        && !savedBrowserDirs.contains(folder.path))
                    {
                        savedBrowserDirs.add(folder.path);
                        savedLibraries.push_back(std::move(folder));
                    }
                }
                else if (child->hasTagName("StarredFile"))
                {
                    const auto path = child->getStringAttribute("path");
                    if (path.isNotEmpty() && !hostStars.contains(path))
                        hostStars.add(path);
                }
                else if (child->hasTagName("SavedSearch"))
                {
                    SavedSearchEntry entry;
                    entry.name = child->getStringAttribute("name");
                    entry.rootPath = child->getStringAttribute("root");
                    entry.search = LibraryStore::browserSearchFromXml(*child);
                    for (auto* pathEl : child->getChildIterator())
                        if (pathEl->hasTagName("Path") || pathEl->hasTagName("Result"))
                        {
                            const auto p = pathEl->hasAttribute("value")
                                ? pathEl->getStringAttribute("value")
                                : pathEl->getStringAttribute("path");
                            if (p.isNotEmpty())
                                entry.resultPaths.add(p);
                        }
                    if (entry.name.isNotEmpty())
                        hostSearches.push_back(std::move(entry));
                }
                else if (child->hasTagName("ClipEdit"))
                {
                    const auto path = child->getStringAttribute("path");
                    if (path.isEmpty()) continue;
                    ClipEdit e;
                    if (child->hasAttribute("octaveAbs"))
                        e.octave = juce::jlimit(-1, 6, child->getIntAttribute("octave", -1));
                    else
                    {
                        const int rel = juce::jlimit(-3, 3, child->getIntAttribute("octave", 0));
                        e.octave = rel == 0 ? -1 : juce::jlimit(0, 6, 4 + rel);
                    }
                    e.pitchShift = juce::jlimit(-12, 12, child->getIntAttribute("pitchShift", 0));
                    e.octaveRange = juce::jlimit(0, 3, child->getIntAttribute("octaveRange", 0));
                    e.pitchMin = juce::jlimit(0, 127, child->getIntAttribute("pitchMin", 0));
                    e.pitchMax = juce::jlimit(0, 127, child->getIntAttribute("pitchMax", 127));
                    if (e.pitchMax < e.pitchMin)
                        std::swap(e.pitchMin, e.pitchMax);
                    {
                        const int em = child->getIntAttribute("extendMult", 1);
                        e.extendMult = (em == 2 || em == 4 || em == 8) ? em : 1;
                    }
                    e.fitScale = child->getIntAttribute("fitScale", 0) != 0;
                    e.mapToRoot = child->getIntAttribute("mapToRoot", 0) != 0;
                    e.root = juce::jlimit(-1, 11, child->getIntAttribute("root", -1));
                    e.mode = (Mode) juce::jlimit(0, kNumModes - 1,
                                                 child->getIntAttribute("mode", (int) Mode::Ionian));
                    {
                        const int mask = child->getIntAttribute("noteFilterMask", 0x0FFF);
                        e.noteFilterMask = (uint16_t) juce::jlimit(1, 0x0FFF, mask & 0x0FFF);
                        if (e.noteFilterMask == 0)
                            e.noteFilterMask = 0x0FFF;
                        e.noteFilterType = child->getIntAttribute("noteFilterType", 0) != 0
                            ? NoteFilterType::Fold : NoteFilterType::Mute;
                    }
                    const auto removedAttr = child->getStringAttribute("removedBars");
                    if (removedAttr.isNotEmpty())
                    {
                        for (const auto& part : juce::StringArray::fromTokens(removedAttr, ",", ""))
                        {
                            const int b = part.getIntValue();
                            if (b >= 0)
                                e.removedBars.push_back(b);
                        }
                        std::sort(e.removedBars.begin(), e.removedBars.end());
                        e.removedBars.erase(std::unique(e.removedBars.begin(), e.removedBars.end()),
                                            e.removedBars.end());
                    }
                    else
                    {
                        e.legacyTrimLead = juce::jmax(0, child->getIntAttribute("trimLead", 0));
                        e.legacyTrimTail = juce::jmax(0, child->getIntAttribute("trimTail", 0));
                    }
                    for (auto* m : child->getChildIterator())
                    {
                        if (m->hasTagName("Move"))
                            e.moves[m->getIntAttribute("id")] = {
                                m->getIntAttribute("dPitch"), m->getIntAttribute("dStep") };
                        else if (m->hasTagName("Vel"))
                            e.velocities[m->getIntAttribute("id")] =
                                juce::jlimit(1, 127, m->getIntAttribute("v", 100));
                        else if (m->hasTagName("Del"))
                            e.deleted.insert(m->getIntAttribute("id"));
                    }
                    clipEdits[path] = std::move(e);
                }
                else if (child->hasTagName("ClipGroove"))
                {
                    const auto path = child->getStringAttribute("path");
                    if (path.isEmpty()) continue;
                    GrooveParams k;
                    k.set(0, child->getIntAttribute("swing", 0));
                    k.set(1, child->getIntAttribute("pocket", 0));
                    k.set(2, child->getIntAttribute("humanize", 0));
                    k.set(3, child->getIntAttribute("dynamics", 0));
                    k.set(4, child->getIntAttribute("length", 100));
                    // Legacy sessions stored Intensity default as 80; treat missing as 100 (neutral).
                    k.set(5, child->getIntAttribute("intensity", 100));
                    k.swingBase = child->getIntAttribute("swingBase", 0) != 0
                                      ? SwingBase::Sixteenth : SwingBase::Eighth;
                    k.swingGridIndex = juce::jlimit(0, 5,
                        child->getIntAttribute("swingGrid", k.swingBase == SwingBase::Sixteenth ? 0 : 1));
                    k.quantizeGridIndex = juce::jlimit(0, (int) QuantizeGrid::Count - 1,
                        child->getIntAttribute("quantizeGrid", (int) QuantizeGrid::Eighth));
                    k.quantizeStrength = juce::jlimit(0, 100, child->getIntAttribute("quantizeStrength", 0));
                    k.articulationIndex = juce::jlimit(0, (int) Articulation::Count - 1,
                        child->getIntAttribute("articulation", 0));
                    // Pre-Off enum: 0 was Legato. New saves stamp articulationV=1.
                    if (child->getIntAttribute("articulationV", 0) == 0)
                        k.articulationIndex = juce::jlimit(0, (int) Articulation::Count - 1,
                            k.articulationIndex + 1);
                    k.articulationStrength = juce::jlimit(0, 100,
                        child->getIntAttribute("articulationStrength", 0));
                    k.sustainPedalMode = juce::jlimit(0, (int) SustainPedalMode::Count - 1,
                        child->getIntAttribute("sustainPedal", 0));
                    k.complexityTarget = child->getIntAttribute("complexityTarget", -1);
                    if (k.complexityTarget >= 0)
                        k.complexityTarget = juce::jlimit(0, 100, k.complexityTarget);
                    k.delayTimeIndex = juce::jlimit(0, (int) DelayTime::Count - 1,
                        child->getIntAttribute("delayTime", (int) DelayTime::Eighth));
                    k.delayAmount = juce::jlimit(0, 100, child->getIntAttribute("delayAmount", 0));
                    k.delayFeedback = juce::jlimit(0, 100, child->getIntAttribute("delayFeedback", 40));
                    k.arpModeIndex = juce::jlimit(0, (int) ArpMode::Count - 1,
                        child->getIntAttribute("arpMode", 0));
                    k.arpRateIndex = juce::jlimit(0, (int) DelayTime::Count - 1,
                        child->getIntAttribute("arpRate", (int) DelayTime::Sixteenth));
                    k.arpGate = juce::jlimit(10, 100, child->getIntAttribute("arpGate", 70));
                    k.arpOctaves = juce::jlimit(1, 4, child->getIntAttribute("arpOctaves", 1));
                    k.strumDirectionIndex = juce::jlimit(0, (int) StrumDirection::Count - 1,
                        child->getIntAttribute("strumDir", 0));
                    k.strumSpeed = juce::jlimit(0, 100, child->getIntAttribute("strumSpeed", 40));
                    k.strumAmount = juce::jlimit(0, 100, child->getIntAttribute("strumAmount", 0));
                    k.velocityRangeLo = juce::jlimit(1, 127, child->getIntAttribute("velRangeLo", 1));
                    k.velocityRangeHi = juce::jlimit(1, 127, child->getIntAttribute("velRangeHi", 127));
                    if (k.velocityRangeHi < k.velocityRangeLo)
                        std::swap(k.velocityRangeLo, k.velocityRangeHi);
                    k.variationIndex = juce::jlimit(0, 16, child->getIntAttribute("variation", 0));
                    clipGrooves[path] = k;
                }
            }
        }
    }

    // App library (Application Support) is the accruing source of truth.
    // Pull disk + host/project stars together so favorites keep growing.
    library_.mergeStarsFromDisk();
    library_.mergeFromPluginState(hostStars, hostSearches);
    applyLibraryToMemory();

    // Folder / search / layout / tweaks are shared across every instance and
    // project — always prefer the last-used workspace from the app library.
    // (Host XML still carries them for older builds / portability.)
    if (library_.hasUiSession)
        applySharedUiSessionFromLibrary();
    else
    {
        // First run after upgrade: seed the library from whatever host state
        // (or defaults) we just loaded so the next instance inherits it.
        persistSharedUiSession();
    }
}

juce::AudioProcessorEditor* MidiToolkitProcessor::createEditor()
{
    return new MidiToolkitEditor(*this);
}

} // namespace pflow

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new pflow::MidiToolkitProcessor();
}
