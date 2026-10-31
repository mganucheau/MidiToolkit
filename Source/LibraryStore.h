#pragma once
#include <juce_core/juce_core.h>
#include "BrowserPanels.h"
#include <atomic>
#include <vector>

namespace pflow {

/** One cached search: criteria + root folder + matching file paths. */
struct CachedSearch
{
    BrowserSearch criteria;
    juce::String rootPath;
    juce::StringArray resultPaths;
    juce::int64 scannedAtMs = 0;
};

/**
 * Last-used browser/UI workspace shared by every plugin instance.
 * Survives new tracks, new projects, and app relaunches.
 */
struct SharedUiSession
{
    juce::String lastBrowserDir;
    juce::StringArray savedBrowserDirs;
    std::vector<SavedLibraryFolder> savedLibraries;

    int browseMode = 0; // 0 folder, 1 starred, 2 search
    bool starredFilter = false;
    bool includeSubdirs = false;
    BrowserSearch browserSessionSearch;
    juce::String selectedClipPath;
    int activeSavedSearchIdx = -1;
    juce::StringArray browserResultPaths;

    BrowserColumnVisibility columnVisibility;
    int nameColumnWidth = 140;
    int browserPaneW = 0;

    bool editorOpen = false;
    bool effectsOpen = false;
    bool previewOpen = true;
    bool sidebarCollapsed = false;
    bool passthrough = false;

    int tweakDensity = 1;       // Density::Comfortable
    int tweakSize = 1;          // ContentSize::Medium
    int tweakTextScalePct = 100;
    int tweakAppearance = 0;    // Appearance::System
    int tweakShowTooltips = 1;

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
};

/**
 * App-owned library file (Application Support), independent of DAW project state.
 * Survives plugin upgrades and is shared by Standalone / AU / VST3.
 *
 * Stores: starred files, saved searches, search caches, and the last UI session.
 */
class LibraryStore
{
public:
    static juce::File libraryFile();

    void load();
    void save() const;
    /** Snapshot + write on a background thread so the host UI never waits on disk. */
    void saveAsync() const;
    /** Block until the latest scheduled async write finishes (call on shutdown). */
    void flush() const;

    juce::StringArray starredFiles;
    std::vector<SavedSearchEntry> savedSearches;
    std::vector<CachedSearch> searchCache;
    SharedUiSession uiSession;
    /** True after a UiSession block was loaded or written at least once. */
    bool hasUiSession = false;

    bool isStarred(const juce::String& path) const { return starredFiles.contains(path); }

    void toggleStarred(const juce::String& path);
    void setStarredFiles(const juce::StringArray& paths);
    void addSavedSearch(const SavedSearchEntry& entry);
    void removeSavedSearch(int index);
    void setSavedSearches(std::vector<SavedSearchEntry> entries);

    /** Replace the shared UI session and persist (async). */
    void putUiSession(const SharedUiSession& session);

    /** Union stars/searches from host plugin state into the app library. */
    void mergeFromPluginState(const juce::StringArray& stars,
                              const std::vector<SavedSearchEntry>& searches);

    /** Pull any stars written by other instances into memory (no save). */
    void mergeStarsFromDisk();

    const CachedSearch* findSearchCache(const juce::File& root, const BrowserSearch& s) const;
    void putSearchCache(const juce::File& root, const BrowserSearch& s,
                        const juce::StringArray& resultPaths);

    static bool searchesEqual(const BrowserSearch& a, const BrowserSearch& b);

    /** Shared XML helpers for host state + library file. */
    static BrowserSearch browserSearchFromXml(const juce::XmlElement& el);
    static void browserSearchToXml(juce::XmlElement& el, const BrowserSearch& s);
    static SavedLibraryFolder savedLibraryFromXml(const juce::XmlElement& el);
    static void savedLibraryToXml(juce::XmlElement& el, const SavedLibraryFolder& folder);

    static SharedUiSession uiSessionFromXml(const juce::XmlElement& el);
    static void uiSessionToXml(juce::XmlElement& el, const SharedUiSession& s);

private:
    static constexpr int kMaxSavedSearches = 48;
    static constexpr int kMaxSearchCache = 32;
    static constexpr int kMaxBrowserResultPaths = 5000;

    static juce::StringArray readStarsFromDisk();
    static void writeLibraryFile(juce::StringArray stars,
                                 std::vector<SavedSearchEntry> searches,
                                 std::vector<CachedSearch> cache,
                                 SharedUiSession session,
                                 bool writeSession);

    /** Bumped on every save request; writers bail if a newer request superseded them. */
    mutable std::atomic<uint64_t> saveGeneration_ { 0 };
    mutable std::atomic<uint64_t> saveCompleted_ { 0 };
};

} // namespace pflow
