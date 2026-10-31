#pragma once
#include "EditModel.h"
#include <atomic>
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace pflow {

/** One MIDI file's browse metadata — no note events (those stay on disk until play). */
struct FolderIndexEntry
{
    juce::String path;
    juce::String name;
    juce::int64 modMs = 0;
    juce::int64 sizeBytes = 0;
    int kind = (int) ClipKind::Lead;
    int root = -1;
    double bpm = 120.0;
    int bars = 1;
    int timeSigNum = 4;
    int timeSigDen = 4;
    int noteCount = 0;
    int difNotes = 0;
    int complexity = 1;
};

/** Cheap folder signature — avoids re-statting hundreds of thousands of files. */
struct FolderFingerprint
{
    int fileCount = 0;
    juce::int64 dirModMs = 0;
    juce::int64 totalBytes = 0;

    bool matchesCount(const FolderFingerprint& o) const
    {
        return fileCount == o.fileCount;
    }
};

/**
 * Per-folder scan cache under Application Support.
 * Lets browse/filter reopen instantly; playback still parses the file once on select.
 */
struct FolderIndex
{
    juce::String rootPath;
    bool recursive = false;
    juce::int64 scannedAtMs = 0;
    FolderFingerprint fingerprint;
    std::vector<FolderIndexEntry> files;

    static juce::File indexDir();
    static juce::File indexFileFor(const juce::File& root, bool recursive);

    /** Fast signature for a folder (directory listing — no MIDI parse). */
    static FolderFingerprint computeFingerprint(const juce::File& root, bool recursive);

    static FolderIndex fromClips(const juce::File& root, bool recursive,
                                 const std::vector<StepClip>& clips);
    static StepClip entryToClip(const FolderIndexEntry& e);
    static std::vector<StepClip> toClips(const FolderIndex& idx);

    bool save() const;
    static std::optional<FolderIndex> load(const juce::File& root, bool recursive);

    /**
     * Load clips when the fingerprint still matches disk.
     * Returns nullopt when missing or stale — caller should Scan.
     */
    static std::optional<std::vector<StepClip>> loadValidClips(const juce::File& root,
                                                               bool recursive);

    /** Max MIDI names to show in the browser before Scan (huge leaf folders). */
    static constexpr int kMaxBrowseNames = 5000;
};

/** Enumerate MIDI files (optionally recursive) via fast native readdir. */
juce::Array<juce::File> collectMidiFiles(const juce::File& root, bool recursive);

/** One-level listing for browse UI — does not allocate every path when capped. */
struct QuickDirListing
{
    std::vector<juce::File> directories;
    std::vector<juce::File> midiFiles; // size <= midiCap
    int totalMidiFiles = 0;
};

QuickDirListing listDirectoryQuick(const juce::File& dir,
                                   int midiCap = FolderIndex::kMaxBrowseNames);

/** At most one progress publish per minIntervalMs, plus a final publish at `total`.
    Safe to call from several parse threads. */
inline bool claimScanProgressPublish(int done, int total,
                                     std::atomic<juce::int64>& lastMs,
                                     juce::int64 nowMs,
                                     juce::int64 minIntervalMs = 100)
{
    if (total > 0 && done >= total)
        return true;
    auto prev = lastMs.load();
    if (nowMs - prev < minIntervalMs)
        return false;
    return lastMs.compare_exchange_strong(prev, nowMs);
}

} // namespace pflow
