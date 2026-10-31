#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiFileData.h"
#include "EditModel.h"
#include "GrooveEngine.h"
#include "LiveMidiFx.h"
#include "MidiRecorder.h"
#include "BrowserPanels.h"
#include "LibraryStore.h"
#include <atomic>
#include <map>

namespace pflow {

/** Lightweight MIDI-file browser: session-synced preview only (no arrangement). */
class MidiToolkitProcessor : public juce::AudioProcessor
{
public:
    MidiToolkitProcessor();
    ~MidiToolkitProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }

    const juce::String getName() const override { return "Midi Toolkit"; }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        const auto mainOut = layouts.getMainOutputChannelSet();
        if (mainOut.isDisabled()) return false;
        return mainOut == juce::AudioChannelSet::stereo()
            || mainOut == juce::AudioChannelSet::mono();
    }

    double getTailLengthSeconds() const override { return 0.0; }

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── Host transport (audio thread writes; UI may read) ────────────────────
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> hostBeatPos { 0.0 };
    std::atomic<bool> hostPlaying { false };

    /** Host loop cycle for preview phase (when supported). */
    std::atomic<bool> hostLoopActive { false };
    std::atomic<double> hostLoopPpqStart { 0.0 };
    std::atomic<double> hostLoopPpqEnd { 16.0 };

    /** When the host does not report a loop, wrap the playhead over this many bars. */
    std::atomic<int> syncSessionBars { 4 };

    // ── Preview transport ────────────────────────────────────────────────────
    // Synced: preview follows the host transport. Free-run: an internal clock
    // at freeBpm loops the armed clip; freerunBeat is the loop-local playhead.
    // Armed by default so a synced plugin sounds as soon as the host plays;
    // the plugin's play/stop buttons disarm it.
    std::atomic<bool> syncToHost { true };
    std::atomic<bool> previewArmed { true };
    std::atomic<double> freeBpm { 124.0 };
    std::atomic<double> freerunBeat { 0.0 };
    std::atomic<double> bpmMultiplier { 1.0 };   // synced ÷2 / ×2 playback speed
    /** Freerun loop region in preview beats [start, end). end<=start → full clip. */
    std::atomic<double> previewLoopStartBeat { 0.0 };
    std::atomic<double> previewLoopEndBeat { 0.0 };

    /** True when the preview is audibly playing right now. */
    bool isPreviewSounding() const
    {
        if (!previewArmed.load()) return false;
        return syncToHost.load() ? hostPlaying.load() : true;
    }

    // ── Per-clip non-destructive state (message thread; persisted) ──────────
    ClipEdit& editFor(const juce::String& filePath) { return clipEdits[filePath]; }
    GrooveParams& grooveFor(const juce::String& filePath) { return clipGrooves[filePath]; }
    std::map<juce::String, ClipEdit> clipEdits;
    std::map<juce::String, GrooveParams> clipGrooves;

    // Toolkit browse-locks: bitmask of toolkitLock::* — locked sections are
    // stamped onto every clip while browsing. lockAutoTrim reapplies empty-bar
    // trim on each file while Playback is locked with trim engaged.
    uint32_t sectionLocks = 0;
    bool lockAutoTrim = false;
    ClipEdit lockedEdit;
    GrooveParams lockedGroove;

    /** Legacy mirrors derived from sectionLocks (tests / older call sites). */
    bool editLock = false;
    bool effectsLock = false;

    void syncLockFlagsFromSections()
    {
        editLock = (sectionLocks & toolkitLock::Pitch) != 0;
        effectsLock = (sectionLocks & toolkitLock::AnyGroove) != 0
                   || (sectionLocks & toolkitLock::Playback) != 0;
    }

    // UI layout state (persisted)
    bool editorOpen = false;
    bool effectsOpen = false;
    bool previewOpen = true;
    bool sidebarCollapsed = false;
    /** Live MIDI through Toolkit effects (replaces clip preview while on). */
    bool passthrough = false;
    std::atomic<bool> passthroughActive { false };
    BrowserColumnVisibility columnVisibility;
    /** Logical (unscaled) Name column width in the file table. */
    int nameColumnWidth = 140;
    /** Browser column width while the piano roll is open. 0 = default split. */
    int browserPaneW = 0;

    /** Layout saved from Settings → “Save current view as default”. */
    bool hasDefaultView = false;
    bool defEditorOpen = false;
    bool defEffectsOpen = false;
    bool defPreviewOpen = true;
    bool defSidebarCollapsed = false;
    bool defPassthrough = false;
    BrowserColumnVisibility defColumns;
    int defNameColumnWidth = 140;
    int defWindowW = 0;
    int defWindowH = 700;
    int defTweakSize = 1;
    int defTweakTextScalePct = 100;
    int defTweakDensity = 1;
    int defTweakAppearance = 0;

    juce::String lastBrowserDir;
    bool trimEmptyMeasuresPreview = false;
    juce::StringArray savedBrowserDirs;
    std::vector<SavedLibraryFolder> savedLibraries;
    juce::StringArray starredFiles;   // favourites — mirrored from LibraryStore
    std::vector<SavedSearchEntry> savedSearches;

    /** Browser UI session mirrored from the editor so it survives UI teardown
        (hosts destroy the editor when you click another plugin) and host save. */
    int browseMode = 0;                 // 0 folder, 1 starred, 2 search
    bool starredFilter = false;
    bool includeSubdirs = false;
    BrowserSearch browserSessionSearch;
    juce::String selectedClipPath;
    int activeSavedSearchIdx = -1;
    /** Paths for the current search result list (browseMode == 2). */
    juce::StringArray browserResultPaths;

    void addSavedBrowserDir(const juce::String& path);
    /** Insert or update a library folder. `replaceDetails` keeps an existing filter when false. */
    void saveLibraryFolder(const SavedLibraryFolder& folder, bool replaceDetails);
    void removeSavedBrowserDir(const juce::String& path);
    const SavedLibraryFolder* libraryFolderFor(const juce::String& path) const;
    void addSavedSearch(const SavedSearchEntry& entry);
    void removeSavedSearch(int index);
    /** Persist result paths onto an existing saved search (library + host state). */
    void updateSavedSearchResults(int index, const juce::StringArray& resultPaths,
                                  const juce::String& rootPath = {});
    bool isStarred(const juce::String& path) const { return starredFiles.contains(path); }
    void toggleStarred(const juce::String& path);

    /** App-owned library (stars, saved searches, search result cache). */
    LibraryStore& library() { return library_; }
    const LibraryStore& library() const { return library_; }

    /** Persist library to disk now (e.g. after search cache update). */
    void saveLibrary();

    /** Push current folder/search/layout/tweaks into the shared app library. */
    void persistSharedUiSession();
    /** Pull last-used workspace from the shared app library into this instance. */
    void applySharedUiSessionFromLibrary();

    /** softUpdate: replace the sounding clip without flushing held notes (live slider tweaks). */
    void setPreviewState(const MidiClip& clip, bool hasClip, bool muted, bool soloed,
                         bool softUpdate = false);

    /** Publish Toolkit pitch/groove snapshot for Passthrough mode. */
    void setLiveFxState(const LiveFxState& state);

    /** Enable/disable passthrough; flushes held notes on transition. */
    void setPassthroughEnabled(bool enabled);

    /** Capture incoming MIDI while on. Stopping closes held notes. */
    void setRecordingEnabled(bool enabled);
    bool isRecording() const { return recordingActive.load(); }
    /** When set, recorded input is also run through the Toolkit effects. */
    void setRecordMonitorFx(bool enabled) { recordMonitorFx.store(enabled); }
    RecordingSnapshot recordSnapshot() const { return recorder.snapshot(); }

    /** Ask the audio thread to release every held note on the next block. */
    void requestNoteFlush()
    {
        juce::ScopedLock sl(previewLock_);
        previewFlushPending_ = true;
        liveFlushPending_.store(true);
    }

private:
    void generatePreviewMidi(const MidiClip& clip, double startBeat, double endBeat,
                             juce::MidiBuffer& output, int numSamples,
                             int sampleOffsetBase = 0);

    /** Explicit note-offs for everything currently sounding (audio thread). */
    void flushActiveNotes(juce::MidiBuffer& output, int samplePosition);

    double sampleRate_ = 44100.0;
    double lastBeatPos_ = -1.0;
    bool wasSounding_ = false;
    bool wasHostPlaying_ = false;
    bool activeNotes_[16][128] = {};   // audio-thread ledger of held note-ons

    juce::CriticalSection previewLock_;
    MidiClip previewClip_;
    bool previewHasClip_ = false;
    bool previewMuted_ = false;
    bool previewFlushPending_ = false;   // release held notes before the next block
    juce::uint64 previewFingerprint_ = 0;

    LiveMidiFx liveFx_;
    std::atomic<bool> liveFlushPending_ { false };
    MidiRecorder recorder;
    std::atomic<bool> recordingActive { false };
    std::atomic<bool> recordMonitorFx { false };

    LibraryStore library_;
    void syncLibraryFromMemory();
    void applyLibraryToMemory();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiToolkitProcessor)
};

} // namespace pflow
