#include "PluginEditor.h"
#include "FolderIndex.h"
#include "GeneratorEngines.h"
#include <map>
#include <thread>
#include <atomic>
#include <algorithm>
#include "BuildInfo.h"
#include "NativeWindowChrome.h"
#include <set>

namespace pflow {

namespace {

FileListEntry entryForClip(const StepClip& clip, const juce::File& f,
                           bool edited, bool starred)
{
    FileListEntry e;
    e.file = f;
    e.name = f.getFileNameWithoutExtension();
    // Keyword lexicon always wins for display — fixes stale scan/index kinds.
    e.kind = clip.kind;
    if (auto tagged = kindKeywordFromFileName(f.getFullPathName()))
        e.kind = *tagged;
    else if (auto tagged = kindKeywordFromFileName(e.name))
        e.kind = *tagged;
    e.rootName = clip.root >= 0 ? juce::String(kNoteNames[(size_t) clip.root]) : juce::String();
    e.bpm = clip.bpm;
    e.bars = clip.bars;
    e.timeSigNum = clip.timeSigNum;
    e.timeSigDen = clip.timeSigDen;
    e.noteCount = clip.noteCount;
    e.difNotes = clip.difNotes;
    e.complexity = clip.complexity;
    e.edited = edited;
    e.starred = starred;
    return e;
}

bool clipMatchesFilter(const StepClip& clip, const BrowserSearch& s)
{
    if (s.bpmMin > 0.0 && clip.bpm < s.bpmMin - 0.5)
        return false;
    if (s.bpmMax > 0.0 && clip.bpm > s.bpmMax + 0.5)
        return false;

    if (s.keyMask != 0)
    {
        if (clip.root < 0 || (s.keyMask & (uint16_t) (1u << clip.root)) == 0)
            return false;
    }
    else if (s.keyRoot >= 0 && clip.root != s.keyRoot)
    {
        return false;
    }

    if (s.barsMin > 0)
    {
        if (s.barsMin >= 64)
        {
            if (clip.bars < 64)
                return false;
        }
        else if (clip.bars < s.barsMin)
        {
            return false;
        }
    }
    if (s.barsMax > 0 && s.barsMax < 64 && clip.bars > s.barsMax)
        return false;

    if (s.complexityMin > 0 && clip.complexity < s.complexityMin)
        return false;
    if (s.complexityMax > 0 && clip.complexity > s.complexityMax)
        return false;

    if (s.kindMask != 0
        && (s.kindMask & (uint8_t) (1u << (int) clip.kind)) == 0)
        return false;

    if (s.tagMask != 0 && (clip.tagMask & s.tagMask) != s.tagMask)
        return false;

    if (s.timeSigMask != 0)
    {
        const int bit = timeSigFilterBit(clip.timeSigNum, clip.timeSigDen);
        if (bit < 0 || (s.timeSigMask & (uint16_t) (1u << bit)) == 0)
            return false;
    }

    return true;
}

int visibleWorkspaceWidth(bool sidebarCollapsed, bool editorOpen, bool effectsOpen, int browserW,
                          bool generatorPane = false)
{
    if (generatorPane)
    {
        return metrics::effectsPaneWidth()
             + (editorOpen ? metrics::editorPaneWidth() : 0)
             + (effectsOpen ? metrics::effectsPaneWidth() : 0);
    }
    const int side = sidebarCollapsed ? metrics::sidebarRailWidth()
                                      : metrics::sidebarExpandedWidth();
    return side
         + browserW
         + (editorOpen ? metrics::editorPaneWidth() : 0)
         + (effectsOpen ? metrics::effectsPaneWidth() : 0);
}

bool clipMatchesQuery(const juce::File& file, const BrowserSearch& s)
{
    if (s.query.isEmpty())
        return true;
    return file.getFileNameWithoutExtension().containsIgnoreCase(s.query);
}

bool clipMatches(const StepClip& clip, const juce::File& file, const BrowserSearch& s)
{
    return clipMatchesQuery(file, s) && clipMatchesFilter(clip, s);
}

/** Query + recurse identity for disk cache (filters are applied after load). */
BrowserSearch searchScanKey(const BrowserSearch& s)
{
    BrowserSearch k;
    k.query = s.query;
    k.subdirs = s.subdirs;
    return k;
}

/** Pitch-lock template must store an absolute octave so browse retargets every
    clip to that register. Native (-1) would leave each file in its own octave. */
void capturePitchLockTemplate(const ClipEdit& src, const StepClip* clip, ClipEdit& lockedOut)
{
    ClipEdit absolute = src;
    if (clip != nullptr)
    {
        if (absolute.octave < 0)
            absolute.octave = clipReferenceOctave(*clip);
        // -1 displays as the first key chip but still follows each file's analysis.
        if (absolute.root < 0)
            absolute.root = analyseClipScale(*clip).primaryRoot;
    }
    applyPitchLock(absolute, lockedOut);
}

/** Non-mutating dedupe: returns primary path -> all locations; hiddenPaths are non-primaries.
    Uses name+size+mtime only — never reads file bytes on the message thread. */
std::map<juce::String, juce::StringArray>
computeDedupeLocations(const std::vector<StepClip>& clips,
                       std::set<juce::String>& hiddenPaths)
{
    hiddenPaths.clear();
    std::map<juce::String, juce::StringArray> locationsByPrimary;
    if (clips.empty())
        return locationsByPrimary;

    auto contentKey = [](const juce::File& f) -> juce::String
    {
        if (!f.existsAsFile())
            return {};
        return f.getFileName().toLowerCase()
             + "|" + juce::String(f.getSize())
             + "|" + juce::String(f.getLastModificationTime().toMilliseconds());
    };

    std::map<juce::String, juce::String> keyToPrimary;
    for (const auto& clip : clips)
    {
        const juce::File f(clip.filePath);
        const juce::String key = contentKey(f);
        if (key.isEmpty())
            continue;

        if (auto it = keyToPrimary.find(key); it != keyToPrimary.end())
        {
            const auto& primary = it->second;
            auto& locs = locationsByPrimary[primary];
            if (locs.isEmpty())
                locs.add(primary);
            if (!locs.contains(clip.filePath))
                locs.add(clip.filePath);
            if (clip.filePath != primary)
                hiddenPaths.insert(clip.filePath);
            continue;
        }

        keyToPrimary[key] = clip.filePath;
        locationsByPrimary[clip.filePath] = juce::StringArray { clip.filePath };
    }
    return locationsByPrimary;
}

juce::String searchBpmTitle(const BrowserSearch& criteria)
{
    if (criteria.bpmMin > 0.0 && criteria.bpmMax > 0.0)
    {
        if (std::abs(criteria.bpmMin - criteria.bpmMax) < 0.5)
            return juce::String((int) std::lround(criteria.bpmMin)) + " BPM";
        return juce::String((int) std::lround(criteria.bpmMin)) + "-"
             + juce::String((int) std::lround(criteria.bpmMax)) + " BPM";
    }
    if (criteria.bpmMin > 0.0)
        return juce::String((int) std::lround(criteria.bpmMin)) + "+ BPM";
    if (criteria.bpmMax > 0.0)
        return "<=" + juce::String((int) std::lround(criteria.bpmMax)) + " BPM";
    return {};
}

/** Compact saved-search label from query + active filters (may wrap in the sidebar). */
juce::String savedSearchShorthand(const BrowserSearch& s)
{
    juce::StringArray parts;
    if (s.query.isNotEmpty())
        parts.add(s.query.trim());

    if (s.kindMask != 0)
    {
        juce::StringArray kinds;
        for (int i = 0; i < kNumClipKinds; ++i)
            if ((s.kindMask & (uint8_t) (1u << i)) != 0)
                kinds.add(clipKindName((ClipKind) i));
        if (kinds.size() > 0)
            parts.add(kinds.joinIntoString("+"));
    }

    if (s.keyMask != 0)
    {
        juce::StringArray keys;
        for (int i = 0; i < 12; ++i)
            if ((s.keyMask & (uint16_t) (1u << i)) != 0)
                keys.add(kNoteNames[(size_t) i]);
        if (keys.size() == 1)
            parts.add(keys[0]);
        else if (keys.size() > 1 && keys.size() <= 4)
            parts.add(keys.joinIntoString("+"));
        else if (keys.size() > 4)
            parts.add(juce::String(keys.size()) + " keys");
    }
    else if (s.keyRoot >= 0 && s.keyRoot < 12)
    {
        parts.add(kNoteNames[(size_t) s.keyRoot]);
    }

    if (const auto bpm = searchBpmTitle(s); bpm.isNotEmpty())
        parts.add(bpm);

    if (s.barsMin > 0 || s.barsMax > 0)
    {
        if (s.barsMin > 0 && s.barsMax > 0 && s.barsMin == s.barsMax)
            parts.add(juce::String(s.barsMin) + " bar" + (s.barsMin == 1 ? "" : "s"));
        else if (s.barsMin > 0 && s.barsMax > 0)
            parts.add(juce::String(s.barsMin) + "-" + juce::String(s.barsMax) + " bars");
        else if (s.barsMin > 0)
            parts.add(juce::String(s.barsMin) + "+ bars");
        else
            parts.add("<=" + juce::String(s.barsMax) + " bars");
    }

    if (s.timeSigMask != 0)
    {
        juce::StringArray sigs;
        for (int i = 0; i < kNumFilterTimeSigs; ++i)
            if ((s.timeSigMask & (uint16_t) (1u << i)) != 0)
                sigs.add(kFilterTimeSigs[i].label);
        if (sigs.size() > 0 && sigs.size() <= 2)
            parts.add(sigs.joinIntoString("+"));
    }

    if (parts.isEmpty())
        return "Search";
    return parts.joinIntoString(" · ");
}

void scanMidiFiles(const juce::File& dir, bool subdirs,
                   std::vector<StepClip>& out,
                   const std::function<bool(const StepClip&, const juce::File&)>& matches)
{
    if (!dir.isDirectory()) return;

    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.mid;*.midi");
    files.sort();
    for (const auto& f : files)
    {
        auto clip = scanMidiBrowseMeta(f);
        if (matches(clip, f))
            out.push_back(std::move(clip));
    }

    if (!subdirs) return;
    auto dirs = dir.findChildFiles(juce::File::findDirectories, false, "*");
    dirs.sort();
    for (const auto& d : dirs)
    {
        if (d.getFileName().startsWithChar('.')) continue;
        scanMidiFiles(d, true, out, matches);
    }
}

} // namespace

MidiToolkitEditor::MidiToolkitEditor(MidiToolkitProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    // Startup defaults independent of prior-session chrome.
    tweaks().appearance.store((int) Appearance::System);
    processorRef.editorOpen = false;
    // Migrate the previous default width (280) to the new half-width default.
    if (processorRef.nameColumnWidth == 280)
        processorRef.nameColumnWidth = 140;

    setLookAndFeel(&lnf);
    lnf.refreshColours();
    juce::Desktop::getInstance().addDarkModeSettingListener(this);

    content.onLayout = [this] { layoutContent(); };
    addAndMakeVisible(content);

    // ── Transport ──
    transport.onPlayPause = [this]
    {
        const bool arm = !processorRef.previewArmed.load();
        if (arm && !processorRef.syncToHost.load())
            processorRef.freerunBeat.store(processorRef.freerunBeat.load());
        processorRef.previewArmed.store(arm);
    };
    transport.onStop = [this]
    {
        processorRef.previewArmed.store(false);
        processorRef.freerunBeat.store(0.0);
    };
    transport.onSyncChanged = [this](bool synced)
    {
        processorRef.syncToHost.store(synced);
        processorRef.freerunBeat.store(0.0);
        // Enabling sync while the DAW is already playing should start preview.
        if (synced && processorRef.hostPlaying.load())
            processorRef.previewArmed.store(true);
    };
    transport.onFreeBpmChanged = [this](double bpm) { processorRef.freeBpm.store(bpm); };
    transport.onToggleEditor = [this] { toggleEditorFold(); };
    transport.onToggleEffects = [this] { toggleEffectsFold(); };
    transport.onTogglePassthrough = [this] { togglePassthrough(); };
    transport.onToggleRecord = [this] { toggleRecord(); };
    transport.onDragToDaw = [this] { startDragExport(); };
    transport.onCopyToFolder = [this] { copyRenderedClipToFolder(); };
    transport.setSynced(processorRef.syncToHost.load());
    transport.setFreeBpm(processorRef.freeBpm.load());
    transport.setBpmMultiplier(processorRef.bpmMultiplier.load());
    transport.setHostBpm(processorRef.hostBpm.load());
    transport.setEditorOpen(processorRef.editorOpen);
    transport.setEffectsOpen(processorRef.effectsOpen);
    transport.setPassthrough(processorRef.passthrough);
    transport.setReserveTrafficLights(
        processorRef.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
    content.addAndMakeVisible(transport);

    // ── Sidebar ──
    sidebar.onOpenFolder = [this] { chooseFolder(); };
    sidebar.onPickDir = [this](const juce::String& path)
    {
        const juce::File dir(path);
        if (!dir.isDirectory())
            return;
        bool scanSubdirs = false;
        if (const auto* saved = processorRef.libraryFolderFor(path))
        {
            sidebar.setCriteria(saved->filter);
            scanSubdirs = saved->scanSubdirs;
        }
        setBrowseMode(0);
        setRootDirectory(dir, false, scanSubdirs);
    };
    sidebar.onAddCurrent = [this]
    {
        juce::File dir = rootDir;
        if (!dir.isDirectory() && processorRef.lastBrowserDir.isNotEmpty())
            dir = juce::File(processorRef.lastBrowserDir);
        if (!dir.isDirectory())
            return;
        const auto path = dir.getFullPathName();
        if (processorRef.savedBrowserDirs.contains(path))
        {
            processorRef.removeSavedBrowserDir(path);
        }
        else
        {
            SavedLibraryFolder folder;
            folder.path = path;
            folder.scanSubdirs = sidebar.getIncludeSubdirs();
            folder.filter = sidebar.getFilter();
            folder.filter.subdirs = folder.scanSubdirs;
            processorRef.saveLibraryFolder(folder, true);
        }
        refreshSidebar();
        if (sidebar.isCollapsed())
            sidebar.setCollapsed(false);
    };
    sidebar.onToggleStarFolder = [this]
    {
        juce::File dir = rootDir;
        if (!dir.isDirectory() && processorRef.lastBrowserDir.isNotEmpty())
            dir = juce::File(processorRef.lastBrowserDir);
        if (!dir.isDirectory())
            return;
        processorRef.toggleStarredFolder(dir.getFullPathName());
        sidebar.setCurrentFolderStarred(processorRef.isStarredFolder(dir.getFullPathName()));
        refreshSidebar();
    };
    sidebar.onRefreshFolder = [this]
    {
        if (!rootDir.isDirectory() && processorRef.lastBrowserDir.isNotEmpty())
            rootDir = juce::File(processorRef.lastBrowserDir);
        if (!rootDir.isDirectory() || sidebarScanning)
            return;
        rescanFolder(true);
    };
    sidebar.onScanSubdirs = [this]
    {
        if (!rootDir.isDirectory() && processorRef.lastBrowserDir.isNotEmpty())
            rootDir = juce::File(processorRef.lastBrowserDir);
        if (!rootDir.isDirectory())
            return;
        // Allow retoggle while a scan runs — cancel/replace via rescanFolder generation.
        const bool subdirs = sidebar.getIncludeSubdirs();
        recursiveBrowse = subdirs;
        processorRef.includeSubdirs = subdirs;
        browserBar.setSubfolders(subdirs);
        if (subdirs)
        {
            setBrowseMode(0);
            starredFilter = false;
            sidebar.setStarredFilter(false);
            activeSavedSearchIdx = -1;
        }
        if (processorRef.savedBrowserDirs.contains(rootDir.getFullPathName()))
        {
            SavedLibraryFolder folder;
            folder.path = rootDir.getFullPathName();
            folder.scanSubdirs = subdirs;
            folder.filter = sidebar.getFilter();
            folder.filter.subdirs = subdirs;
            processorRef.saveLibraryFolder(folder, true);
        }
        rescanFolder(true);
        refreshSidebar();
        refreshBrowserBar();
        fileList.grabBrowseFocus();
    };
    sidebar.onRemoveDir = [this](const juce::String& path)
    {
        processorRef.removeSavedBrowserDir(path);
        refreshSidebar();
    };
    sidebar.onShowStarred = [this]
    {
        if (browseMode == 1)
        {
            // Leave the global starred library; return to the folder browser.
            starredFilter = false;
            sidebar.setStarredFilter(false);
            setBrowseMode(0);
            if (rootDir.isDirectory())
                rescanFolder(true);
            else
            {
                clips.clear();
                rebuildEntries();
            }
            return;
        }

        starredFilter = false;
        sidebar.setStarredFilter(true); // highlights the Files row
        setBrowseMode(1);
        loadStarredClips();
        refreshSidebar();
    };
    sidebar.onShowStarredFolders = [this]
    {
        if (browseMode == 3)
        {
            setBrowseMode(0);
            if (rootDir.isDirectory())
                rescanFolder(true);
            else
            {
                clips.clear();
                rebuildEntries();
            }
            return;
        }
        starredFilter = false;
        sidebar.setStarredFilter(false);
        setBrowseMode(3);
        loadStarredFolders();
        refreshSidebar();
    };
    sidebar.onShowSearch = [this]
    {
        // Toggle / focus the SEARCH section.
        sidebar.setSearchFormOpen(!sidebar.isSearchFormOpen());
    };
    sidebar.onRunSearch = [this](const BrowserSearch& s) { runSearchAsync(s); };
    sidebar.onFilterChanged = [this] { applyBrowserFilter(); };
    sidebar.onExitSearch = [this]
    {
        // × clears criteria, collapses the form, and leaves search results.
        ++searchGeneration; // cancel any in-flight async search
        fileList.setSearching(false);
        if (browseMode == 2)
        {
            setBrowseMode(0);
            searchDuplicateLocations.clear();
            activeSavedSearchIdx = -1;
            if (rootDir.isDirectory())
                rescanFolder(true);
            else if (processorRef.lastBrowserDir.isNotEmpty()
                     && juce::File(processorRef.lastBrowserDir).isDirectory())
            {
                setRootDirectory(juce::File(processorRef.lastBrowserDir), true);
            }
            else
            {
                clips.clear();
                rebuildEntries();
                rollEditor.clearClip();
                syncEffectsInspector();
                processorRef.setPreviewState({}, false, false, false);
            }
        }
        refreshSidebar();
        fileList.grabBrowseFocus();
    };
    sidebar.onPickSavedSearch = [this](int idx)
    {
        if (!juce::isPositiveAndBelow(idx, (int) processorRef.savedSearches.size()))
            return;
        const auto& entry = processorRef.savedSearches[(size_t) idx];
        activeSavedSearchIdx = idx;
        activeSearch = entry.search;
        sidebar.setCriteria(activeSearch);
        starredFilter = false;
        sidebar.setStarredFilter(false);

        // Instant restore from in-session snapshot (no rescan / reparse).
        if (const auto it = savedSearchSnapshots.find(idx); it != savedSearchSnapshots.end())
        {
            const auto& snap = it->second;
            if (LibraryStore::searchesEqual(snap.criteria, entry.search)
                && (entry.rootPath.isEmpty() || snap.rootPath == entry.rootPath))
            {
                applySearchSnapshot(snap.clips, snap.locations, snap.criteria, idx,
                                    snap.rootPath, false);
                refreshSidebar();
                return;
            }
        }

        // Persisted result paths — hydrate from FolderIndex (no MIDI re-parse).
        if (entry.resultPaths.size() > 0)
        {
            const auto criteria = entry.search;
            const auto rootPath = entry.rootPath;
            loadClipsFromPathsAsync(entry.resultPaths,
                [this, criteria, rootPath, idx](std::vector<StepClip>&& found)
                {
                    if (found.empty())
                    {
                        juce::File searchRoot(rootPath);
                        if (!searchRoot.isDirectory())
                            searchRoot = rootDir.isDirectory() ? rootDir
                                : juce::File(processorRef.lastBrowserDir);
                        runSearchAsync(criteria, idx, searchRoot, false);
                        return;
                    }
                    applySearchSnapshot(std::move(found), {}, criteria, idx, rootPath, false);
                    refreshSidebar();
                },
                juce::File(rootPath));
            refreshSidebar();
            return;
        }

        juce::File searchRoot(entry.rootPath);
        if (!searchRoot.isDirectory())
            searchRoot = rootDir.isDirectory() ? rootDir
                : juce::File(processorRef.lastBrowserDir);
        runSearchAsync(entry.search, idx, searchRoot);
        refreshSidebar();
    };
    sidebar.onRemoveSavedSearch = [this](int idx)
    {
        processorRef.removeSavedSearch(idx);
        forgetSavedSearchSnapshot(idx);
        if (activeSavedSearchIdx == idx)
            activeSavedSearchIdx = -1;
        else if (activeSavedSearchIdx > idx)
            --activeSavedSearchIdx;
        refreshSidebar();
    };
    sidebar.onSaveCurrentSearch = [this] { saveCurrentSearch(); };
    sidebar.onCopyStarredToFolder = [this] { copyStarredToFolder(); };
    sidebar.onOpenSettings = [this] { showTweaksMenu(); };
    sidebar.onCollapsedChanged = [this]
    {
        processorRef.sidebarCollapsed = sidebar.isCollapsed();
        persistBrowserSession();
        applyLayoutState();
    };
    sidebar.onWidthChanged = [this]
    {
        // Live-resize without animating the window — file list absorbs the slack.
        layoutContent();
    };
    sidebar.onContentHeightChanged = [this] { updateWindowLimits(); };
    applySavedDefaultView();
    if (!processorRef.hasDefaultView && !processorRef.editorOpen)
        processorRef.previewOpen = true;
    sidebar.setCollapsed(processorRef.sidebarCollapsed);
    content.addAndMakeVisible(sidebar);

    // ── Browser toolbar + filter popover (Groove) ──
    browserBar.setSearchComponent(&sidebar.searchComponent());
    browserBar.onToggleFilters = [this] { sidebar.setFilterOpen(!sidebar.isFilterOpen()); };
    browserBar.onResetFilters = [this]
    {
        sidebar.resetFilters();
        refreshBrowserBar();
        layoutContent();
    };
    browserBar.onToggleSubfolders = [this]
    {
        const bool on = !sidebar.getIncludeSubdirs();
        sidebar.setIncludeSubdirs(on);
        processorRef.includeSubdirs = on;
        browserBar.setSubfolders(on);
        if (sidebar.onScanSubdirs)
            sidebar.onScanSubdirs();
        else
            refreshBrowserBar();
    };
    browserBar.onToggleGrid = [this]
    {
        gridView = !gridView;
        browserBar.setGrid(gridView);
        kindGrid.setKindMask(sidebar.getCriteria().kindMask);
        kindGrid.refresh();
        applyLayoutState();
        layoutContent();
        if (gridView) kindGrid.grabKeyboardFocus();
    };
    sidebar.onSortChanged = [this](BrowserSortColumn col)
    {
        fileList.setSort(col, true);
    };
    kindGrid.setSource(&fileList);
    kindGrid.setKindMask(sidebar.getCriteria().kindMask);
    kindGrid.onSelect = [this](int idx)
    {
        claimKeyNav(KeyNavTarget::Browser);
        fileList.setSelectedIndex(idx, juce::sendNotification);
        kindGrid.ensureSelectedVisible();
        kindGrid.repaint();
    };
    kindGrid.onToggleStar = [this](int idx)
    {
        if (fileList.onToggleStar) fileList.onToggleStar(idx);
        kindGrid.refresh();
    };
    kindGrid.onOpen = [this](int idx) { if (fileList.onOpenInPianoRoll) fileList.onOpenInPianoRoll(idx); };
    kindGrid.onDragFile = [this](const juce::File& f) { if (fileList.onDragFile) fileList.onDragFile(f); };
    kindGrid.onActivated = [this] { claimKeyNav(KeyNavTarget::Browser); };
    kindGrid.onStep = [this](int d)
    {
        fileList.selectAdjacent(d);
        kindGrid.ensureSelectedVisible();
        kindGrid.repaint();
    };
    content.addChildComponent(kindGrid);
    content.addAndMakeVisible(browserBar);

    filterPopover.setContent(&sidebar.filterComponent(), [this] { return sidebar.filterIdealHeight(); });
    filterPopover.onDone = [this] { sidebar.setFilterOpen(false); };
    filterPopover.onReset = [this]
    {
        sidebar.resetFilters();
        refreshBrowserBar();
    };
    filterPopover.onSaveSearch = [this] { if (sidebar.onSaveCurrentSearch) sidebar.onSaveCurrentSearch(); };
    content.addChildComponent(filterPopover);
    sidebar.onFilterOpenChanged = [this](bool open)
    {
        filterPopover.setVisible(open && leftPaneTab == 0);
        layoutContent();
        if (open)
        {
            filterPopover.toFront(false);
            filterPopover.grabKeyboardFocus();
        }
        refreshBrowserBar();
    };
    sidebar.onFilterLayoutChanged = [this] { layoutContent(); };
    fileList.onFolderNameChanged = [this](const juce::String& name)
    {
        browserBar.setTitle(name);
        layoutContent(); // title row changes BrowserToolbar height
    };
    fileList.onEntriesChanged = [this]
    {
        kindGrid.setKindMask(sidebar.getCriteria().kindMask);
        kindGrid.refresh();
        refreshBrowserBar();
    };
    transport.onOpenSettings = [this] { showTweaksMenu(); };

    // ── File list ──
    fileList.onSelect = [this](int entryIdx)
    {
        if (!juce::isPositiveAndBelow(entryIdx, (int) displayRows.size()))
            return;
        const auto& row = displayRows[(size_t) entryIdx];
        if (row.isDirectory)
            return;   // highlight only; enter via Right / click
        if (row.clipIndex < 0)
        {
            ensureAndSelectFile(row.file);
            return;
        }
        selectIndex(row.clipIndex);
    };
    fileList.onPlayRow = [this](int entryIdx)
    {
        if (!juce::isPositiveAndBelow(entryIdx, (int) displayRows.size()))
            return;
        const auto& row = displayRows[(size_t) entryIdx];
        if (row.isDirectory) return;
        if (row.clipIndex < 0)
            ensureAndSelectFile(row.file);
        else
            selectIndex(row.clipIndex);
        processorRef.previewArmed.store(true);
    };
    fileList.onToggleStar = [this](int entryIdx)
    {
        if (!juce::isPositiveAndBelow(entryIdx, (int) displayRows.size()))
            return;
        const auto& row = displayRows[(size_t) entryIdx];
        const auto path = row.clipIndex >= 0
                              ? clips[(size_t) row.clipIndex].filePath
                              : row.file.getFullPathName();
        if (path.isEmpty()) return;
        if (row.isDirectory || juce::File(path).isDirectory())
        {
            processorRef.toggleStarredFolder(path);
            if (browseMode == 3)
                loadStarredFolders();
            else
                rebuildEntries();
        }
        else
        {
            processorRef.toggleStarred(path);
            if (browseMode == 1)
                loadStarredClips();
            else
                rebuildEntries();
        }
        refreshSidebar();
        if (const int d = displayForClip(selectedIdx); d >= 0)
            fileList.setSelectedIndex(d, juce::dontSendNotification);
    };
    fileList.onEnterParent = [this]
    {
        if (browseMode == 0)
            enterParentFolder();
    };
    fileList.onEnterFolder = [this](int entryIdx) { enterFolderAtDisplay(entryIdx); };
    fileList.onOpenInPianoRoll = [this](int)
    {
        if (!processorRef.editorOpen)
            toggleEditorFold();
    };
    fileList.onDragFile = [this](const juce::File& f) { startDragOriginalFile(f); };
    fileList.onEmptyOpenFolder = [this] { chooseFolder(); };
    fileList.onActivated = [this] { claimKeyNav(KeyNavTarget::Browser); };
    fileList.onRevealFile = [](const juce::File& f) { f.revealToUser(); };
    fileList.onCopyFileToFolder = [this](const juce::File& f) { copyFileToFolder(f); };
    fileList.setColumnVisibility(processorRef.columnVisibility);
    fileList.setNameColumnWidth(processorRef.nameColumnWidth);
    fileList.onColumnVisibilityChanged = [this](const BrowserColumnVisibility& v)
    {
        processorRef.columnVisibility = v;
        persistBrowserSession();
        applyLayoutState();
    };
    fileList.onNameColumnWidthChanged = [this](int logicalW)
    {
        processorRef.nameColumnWidth = logicalW;
        persistBrowserSession();
        // Grow/shrink the browser column so fitted names stay visible.
        applyLayoutState();
        resized();
    };
    content.addAndMakeVisible(fileList);

    content.addAndMakeVisible(previewHeader);
    content.addAndMakeVisible(miniRoll);
    miniRoll.onContextClick = [this](const juce::MouseEvent&)
    {
        juce::PopupMenu m;
        m.addItem(1, "Mute preview", true, previewMuted);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&miniRoll),
                        [this](int r)
                        {
                            if (r != 1) return;
                            previewMuted = !previewMuted;
                            pushPreviewToProcessor();
                            previewHeader.refresh();
                        });
    };

    // ── Piano-roll editor ──
    rollEditor.onEditChanged = [this](const ClipEdit& e)
    {
        if (const auto* clip = selectedClip())
        {
            const ClipEdit previous = processorRef.editFor(clip->filePath);
            processorRef.editFor(clip->filePath) = e;
            captureLocksFromEdit(previous, e, clip);
            refreshEntryMeta(selectedIdx);
            pushPreviewToProcessor();
            syncEffectsInspector();
        }
    };
    rollEditor.onTrimStateChanged = [this](bool active, bool enabled, const juce::String& label)
    {
        // A locked trim stays visibly on so the mode reads as applied even when
        // this file has no empty bars to remove.
        if ((processorRef.sectionLocks & toolkitLock::Playback) != 0
            && processorRef.lockAutoTrim)
        {
            active = true;
            enabled = true;
        }
        effectsInspector.setTrimState(active, enabled, label);
    };
    rollEditor.onTrimRequested = [this] { handleTrimClicked(); };
    rollEditor.onLoopChanged = [this](double startStep, double endStep)
    {
        const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        const double startBeat = startStep * stretch / 4.0;
        const double endBeat = endStep * stretch / 4.0;
        processorRef.previewLoopStartBeat.store(startBeat);
        processorRef.previewLoopEndBeat.store(endBeat);
        // Keep the free-run playhead inside the new region immediately.
        double beat = processorRef.freerunBeat.load();
        const double loopLen = juce::jmax(0.25, endBeat - startBeat);
        if (beat < startBeat || beat >= endBeat)
        {
            beat = startBeat + std::fmod(std::max(0.0, beat - startBeat), loopLen);
            if (beat < startBeat) beat += loopLen;
            processorRef.freerunBeat.store(beat);
        }
    };
    rollEditor.onActivated = [this] { claimKeyNav(KeyNavTarget::Editor); };
    applyTimeStretchFromMultiplier();
    content.addAndMakeVisible(rollEditor);

    browserRollSplitter.onDragStart = [this]
    {
        splitterDragOrigin = fileList.getWidth();
        splitterDragSpan = fileList.getWidth() + rollEditor.getWidth();
    };
    browserRollSplitter.onDrag = [this](int dx)
    {
        const int minBrowser = metrics::scaled(180);
        const int minRoll = metrics::openRollMinWidth();
        const int maxBrowser = juce::jmin(juce::jmax(minBrowser, browserContentWidth()),
                                          juce::jmax(minBrowser, splitterDragSpan - minRoll));
        browserPaneW = juce::jlimit(minBrowser, maxBrowser, splitterDragOrigin + dx);
        processorRef.browserPaneW = browserPaneW;
        layoutContent();
    };
    content.addAndMakeVisible(browserRollSplitter);
    browserRollSplitter.setVisible(false);
    browserPaneW = processorRef.browserPaneW;

    // ── Effects inspector ──
    effectsInspector.onGrooveChanged = [this](const GrooveParams& k)
    {
        if (recording)
        {
            pushLiveFxToProcessor();
            processorRef.setRecordMonitorFx(processorRef.effectsOpen);
            return;
        }
        if (hasRecording)
        {
            // Generated clips re-render live when density / intensity / dynamics /
            // gate / form / length / style change. Recorded takes keep note data.
            if (lastGeneratedPart >= 0 && recordedClip.filePath.startsWith("generator://"))
            {
                regenerateGeneratedClip(k);
                return;
            }
            rollEditor.setClip(recordedClip, effectsInspector.getEdit(), k);
            pushPreviewToProcessor();
            updateMiniPreview();
            return;
        }
        if (const auto* clip = selectedClip())
        {
            processorRef.grooveFor(clip->filePath) = k;
            if ((processorRef.sectionLocks & toolkitLock::AnyGroove) != 0)
            {
                captureGrooveSectionLock(k, processorRef.lockedGroove, processorRef.sectionLocks,
                                         clip->complexity);
                applyGrooveSectionLock(processorRef.lockedGroove,
                                       processorRef.grooveFor(clip->filePath),
                                       processorRef.sectionLocks);
            }
            rollEditor.setClip(*clip, selectedEdit(), selectedGroove());
            pushPreviewToProcessor();
            updateMiniPreview();
        }
        else
            pushLiveFxToProcessor();
    };
    effectsInspector.onGeneratorGenerated = [this](int partIdx)
    {
        if (recording)
            return;
        lastGeneratedPart = juce::jlimit(0, (int) gen::Part::Count - 1, partIdx);
        regenerateGeneratedClip(effectsInspector.getGroove());
    };
    effectsInspector.onEditChanged = [this](const ClipEdit& e)
    {
        if (recording)
        {
            pushLiveFxToProcessor();
            processorRef.setRecordMonitorFx(processorRef.effectsOpen);
            return;
        }
        if (hasRecording)
        {
            rollEditor.setClip(recordedClip, e, effectsInspector.getGroove());
            pushPreviewToProcessor();
            updateMiniPreview();
            return;
        }
        if (const auto* clip = selectedClip())
        {
            const ClipEdit previous = processorRef.editFor(clip->filePath);
            processorRef.editFor(clip->filePath) = e;
            captureLocksFromEdit(previous, e, clip);
            rollEditor.setClip(*clip, e, selectedGroove());
            refreshEntryMeta(selectedIdx);
            pushPreviewToProcessor();
            updateMiniPreview();
        }
        else
            pushLiveFxToProcessor();
    };
    effectsInspector.onSectionLocksChanged = [this](uint32_t locks)
    {
        processorRef.sectionLocks = locks;
        processorRef.syncLockFlagsFromSections();
        if (locks != 0)
        {
            const auto* clip = selectedClip();
            const int cx = clip != nullptr ? clip->complexity : -1;
            captureGrooveSectionLock(selectedGroove(), processorRef.lockedGroove, locks, cx);
            if ((locks & (toolkitLock::Pitch | toolkitLock::Playback)) != 0)
            {
                if ((locks & toolkitLock::Pitch) != 0)
                    capturePitchLockTemplate(selectedEdit(), clip, processorRef.lockedEdit);
                else
                    applyPitchLock(selectedEdit(), processorRef.lockedEdit);
                if ((locks & toolkitLock::Playback) != 0)
                {
                    processorRef.lockedEdit.extendMult = selectedEdit().extendMult;
                    processorRef.lockAutoTrim = selectedEdit().hasTrim();
                }
            }
            stampLockedSections(false);
            if (clip != nullptr)
            {
                rollEditor.setClip(*clip, selectedEdit(), selectedGroove());
                pushPreviewToProcessor();
                updateMiniPreview();
            }
            syncEffectsInspector();
            if ((locks & toolkitLock::Playback) != 0 && processorRef.lockAutoTrim)
                effectsInspector.setTrimState(true, true, {});
        }
        else
        {
            processorRef.lockedGroove = GrooveParams();
            processorRef.lockedEdit = ClipEdit();
            processorRef.lockAutoTrim = false;
            processorRef.clipGrooves.clear();
            if (const auto* clip = selectedClip())
            {
                rollEditor.setClip(*clip, selectedEdit(), GrooveParams());
                pushPreviewToProcessor();
                updateMiniPreview();
            }
            syncEffectsInspector();
        }
        pushLiveFxToProcessor();
    };
    effectsInspector.onTrimClicked = [this] { handleTrimClicked(); };
    effectsInspector.onResetUnlocked = [this]
    {
        if (const auto* clip = selectedClip())
        {
            const auto locks = processorRef.sectionLocks;
            auto& g = processorRef.grooveFor(clip->filePath);
            auto& e = processorRef.editFor(clip->filePath);
            // Mirror inspector resets into processor state for unlocked sections.
            if ((locks & toolkitLock::AnyGroove) == 0)
                processorRef.clipGrooves.erase(clip->filePath);
            else
            {
                auto next = GrooveParams();
                applyGrooveSectionLock(processorRef.lockedGroove, next, locks);
                g = next;
            }
            if ((locks & toolkitLock::Pitch) == 0)
            {
                e.octave = -1;
                e.pitchShift = 0;
                e.octaveRange = 0;
                e.pitchMin = 0;
                e.pitchMax = 127;
                e.fitScale = false;
                e.mapToRoot = false;
                e.root = -1;
                e.mode = Mode::Ionian;
                e.noteFilterMask = 0x0FFF;
                e.noteFilterType = NoteFilterType::Mute;
            }
            if ((locks & toolkitLock::Playback) == 0)
                e.extendMult = 1;
            if ((locks & toolkitLock::Pitch) == 0 && (locks & toolkitLock::Playback) == 0)
                processorRef.lockedEdit = ClipEdit();
            else
                applyPitchLock(e, processorRef.lockedEdit);
            rollEditor.setClip(*clip, e, selectedGroove());
            pushPreviewToProcessor();
            updateMiniPreview();
        }
    };
    effectsInspector.onBpmMultiplierChanged = [this](double m)
    {
        processorRef.bpmMultiplier.store(m);
        transport.setBpmMultiplier(m);
        applyTimeStretchFromMultiplier();
        updateMiniPreview();
        pushLiveFxToProcessor();
    };
    processorRef.syncLockFlagsFromSections();
    effectsInspector.setSectionLocks(processorRef.sectionLocks);
    effectsInspector.setSectionOrder(processorRef.toolkitSectionOrder);
    effectsInspector.onSectionOrderChanged = [this](const juce::String& order)
    {
        processorRef.toolkitSectionOrder = order;
        persistBrowserSession();
    };
    effectsInspector.setBpmMultiplier(processorRef.bpmMultiplier.load());
    effectsInspector.onActivated = [this] { claimKeyNav(KeyNavTarget::Effects); };
    // Section folds scroll inside the viewport — they must not grow the window.
    effectsInspector.onContentHeightChanged = [this]
    {
        if (processorRef.passthrough)
            syncPassthroughWindowHeight();
    };
    content.addAndMakeVisible(effectsInspector);

    leftPaneTabs.onTab = [this](int tab) { setLeftPaneTab(tab); };
    leftPaneTabs.onTogglePreview = [this]
    {
        // Preview and piano roll stay mutually exclusive.
        if (processorRef.editorOpen)
        {
            processorRef.editorOpen = false;
            transport.setEditorOpen(false);
        }
        processorRef.previewOpen = !processorRef.previewOpen;
        leftPaneTabs.setPreviewOpen(processorRef.previewOpen);
        persistBrowserSession();
        applyLayoutState();
    };
    leftPaneTabs.onToggleGrid = [this]
    {
        gridView = !gridView;
        leftPaneTabs.setGrid(gridView);
        browserBar.setGrid(gridView);
        kindGrid.setKindMask(sidebar.getCriteria().kindMask);
        kindGrid.refresh();
        applyLayoutState();
        layoutContent();
        if (gridView) kindGrid.grabKeyboardFocus();
    };
    content.addAndMakeVisible(leftPaneTabs);
    generatorViewport.setViewedComponent(&generatorBody, false);
    generatorViewport.setScrollBarsShown(true, false);
    generatorViewport.setScrollBarThickness(10);
    content.addChildComponent(generatorViewport);
    effectsInspector.setExternalGeneratorHost(&generatorBody);

    passthroughVeil.setVisible(false);
    content.addChildComponent(passthroughVeil);

    restoreBrowserSession();
#if MIDITOOLKIT_MIDI_FX
    processorRef.editorOpen = false;
    if (!processorRef.hasDefaultView)
    {
        processorRef.effectsOpen = true;
        // FX defaults to effect mode (transform track MIDI). Turn Passthrough
        // off to play generated/file clips instead of accepting input.
        processorRef.passthrough = true;
    }
    processorRef.setPassthroughEnabled(processorRef.passthrough);
    effectsInspector.setGeneratorsEnabled(true);
    effectsInspector.setPaneTab(1);
    setLeftPaneTab(0);
#endif
    refreshSidebar();

    setResizable(true, true);
#if MIDITOOLKIT_MIDI_FX
    lastWindowH = processorRef.passthrough ? passthroughDefaultHeight()
                                          : metrics::transportH() + metrics::scaled(480);
    {
        const int browserW = browserContentWidth();
        const int w = processorRef.passthrough
            ? passthroughWindowWidth()
            : visibleWorkspaceWidth(sidebar.isCollapsed(),
                                     processorRef.editorOpen,
                                     processorRef.effectsOpen,
                                     browserW,
                                     leftPaneTab == 1);
        setSize(w, lastWindowH);
    }
#else
    if (processorRef.hasDefaultView && processorRef.defWindowW > 200)
    {
        lastWindowH = juce::jmax(metrics::transportH() + metrics::scaled(240), processorRef.defWindowH);
        setSize(processorRef.defWindowW, lastWindowH);
    }
    else
    {
        lastWindowH = 700;
        setSize(700, 700);
    }
#endif
    applyLayoutState();
    startTimerHz(30);
    setWantsKeyboardFocus(true);
    updateMiniPreview();
    pushLiveFxToProcessor();
    applyPassthroughUi();
    applyNativeWindowChrome();
    // Floor later resizes at the height this window actually opened to.
    openedWindowH = processorRef.passthrough
        ? juce::jmax(metrics::transportH() + metrics::scaled(240),
                     savedNonPassH > 0 ? savedNonPassH : lastWindowH)
        : juce::jmax(1, getHeight());
    updateWindowLimits();
}

MidiToolkitEditor::~MidiToolkitEditor()
{
    // Invalidate in-flight workers so they never touch a destroyed editor.
    ++folderScanGeneration;
    ++searchGeneration;
    ++noteLoadGeneration;
    persistBrowserSession();
    flushBrowserSession();
    juce::Desktop::getInstance().removeDarkModeSettingListener(this);
    nativeChrome::removeSpaceToHostMonitor(spaceToHostMonitor);
    spaceToHostMonitor = nullptr;
    stopTimer();
    setLookAndFeel(nullptr);
}

int MidiToolkitEditor::beginBackgroundClipLoad(bool hideList)
{
    // Cancel both folder and search workers — only one list load should win.
    const int gen = ++folderScanGeneration;
    ++searchGeneration;
    sidebarScanning = true;
    sidebar.setScanning(true);
    // Folder scans keep the file list up so names stay visible while metadata loads.
    fileList.setSearching(hideList);
    return gen;
}

void MidiToolkitEditor::loadClipsFromPathsAsync(
    juce::StringArray paths,
    std::function<void(std::vector<StepClip>&&)> onDone,
    juce::File indexRoot)
{
    const int gen = beginBackgroundClipLoad();
    juce::Component::SafePointer<MidiToolkitEditor> safe(this);
    juce::Thread::launch([safe, paths = std::move(paths), gen,
                          onDone = std::move(onDone), indexRoot]() mutable
    {
        // Prefer FolderIndex metadata — never re-parse MIDI just to rebuild a list.
        std::map<juce::String, StepClip> byPath;
        auto ingestIndex = [&byPath](const juce::File& root, bool recursive)
        {
            if (!root.isDirectory())
                return;
            // Use the on-disk index even if the fingerprint drifted slightly —
            // we only need browse metadata; missing files are skipped below.
            if (auto idx = FolderIndex::load(root, recursive))
                for (auto& c : FolderIndex::toClips(*idx))
                    if (c.filePath.isNotEmpty())
                        byPath.emplace(c.filePath, std::move(c));
        };

        if (indexRoot.isDirectory())
        {
            ingestIndex(indexRoot, true);
            ingestIndex(indexRoot, false);
        }

        std::vector<StepClip> found;
        found.reserve((size_t) paths.size());
        for (const auto& path : paths)
        {
            if (safe == nullptr)
                break;
            const juce::File f(path);
            if (!f.existsAsFile())
                continue;

            if (auto it = byPath.find(path); it != byPath.end())
            {
                found.push_back(it->second);
                continue;
            }

            // Lightweight stub — notes load on select via ensureClipNotesLoaded.
            StepClip stub;
            stub.filePath = path;
            stub.name = f.getFileNameWithoutExtension();
            applyFilenameKindTag(stub);
            found.push_back(std::move(stub));
        }

        juce::MessageManager::callAsync([safe, gen, found = std::move(found),
                                         onDone = std::move(onDone)]() mutable
        {
            if (safe == nullptr || gen != safe->folderScanGeneration.load())
                return;
            safe->sidebarScanning = false;
            safe->sidebar.setScanning(false);
            safe->fileList.setSearching(false);
            onDone(std::move(found));
        });
    });
}

void MidiToolkitEditor::parentHierarchyChanged()
{
    applyNativeWindowChrome();
}

void MidiToolkitEditor::visibilityChanged()
{
    if (isShowing())
        applyNativeWindowChrome();
}

void MidiToolkitEditor::applyNativeWindowChrome()
{
    if (getPeer() == nullptr)
        return;

    if (spaceToHostMonitor == nullptr)
        spaceToHostMonitor = nativeChrome::installSpaceToHostMonitor(*this, processorRef.syncToHost);

    // Already folded: don't resize the host window again when focus leaves.
    // applyCupertinoTitlebar still no-ops its frame change when the rect matches,
    // and styles the close button only if yellow/green are still showing.
    if (nativeChrome::titlebarFolded(*this))
    {
        nativeChrome::applyCupertinoTitlebar(*this, metrics::transportH());
        transport.setLightsZoneWidth(nativeChrome::trafficLightsTrailingEdge(*this, 12));
        chromeApplyAttempts = 0;
        return;
    }

    nativeChrome::applyCupertinoTitlebar(*this, metrics::transportH());
    transport.setLightsZoneWidth(nativeChrome::trafficLightsTrailingEdge(*this, 12));
    transport.resized();
    transport.repaint();

    if (nativeChrome::titlebarFolded(*this))
    {
        chromeApplyAttempts = 0;
        return;
    }
    if (chromeApplyAttempts >= 12)
        return;
    ++chromeApplyAttempts;
    juce::Component::SafePointer<MidiToolkitEditor> safe(this);
    juce::Timer::callAfterDelay(80, [safe]
    {
        if (safe != nullptr)
            safe->applyNativeWindowChrome();
    });
}

void MidiToolkitEditor::darkModeSettingChanged()
{
    if (currentAppearance() != Appearance::System)
        return;
    lnf.refreshColours();
    sendLookAndFeelChange();
    repaint();
}

// ── data ─────────────────────────────────────────────────────────────────────

void MidiToolkitEditor::setRootDirectory(const juce::File& dir, bool keepSelection, bool scanSubdirs)
{
    browseMode = 0;
    starredFilter = false;
    searchDuplicateLocations.clear();
    sidebar.setStarredFilter(false);
    sidebar.setIncludeSubdirs(scanSubdirs);
    recursiveBrowse = scanSubdirs;
    rootDir = dir;
    processorRef.lastBrowserDir = dir.getFullPathName();
    activeSavedSearchIdx = -1;
    ++browseListGeneration;
    auto name = dir.getFileName();
    if (name.isEmpty()) name = dir.getFullPathName();
    fileList.setFolderName(name);
    refreshSidebar();
    // Scan this folder immediately. Subdirectories are included only when asked.
    rescanFolder(keepSelection);
    fileList.grabBrowseFocus();
}

void MidiToolkitEditor::refreshBrowseListingAsync(const juce::String& selectPath,
                                                    bool rescanIfMissing)
{
    if (!rootDir.isDirectory())
    {
        cachedSubdirs.clear();
        cachedMidiFiles.clear();
        return;
    }

    const juce::File dir = rootDir;
    const bool recursive = recursiveBrowse;
    const int gen = browseListGeneration.load();
    juce::Component::SafePointer<MidiToolkitEditor> safe(this);
    juce::Thread::launch([safe, dir, recursive, gen, selectPath, rescanIfMissing]()
    {
        // Prefer a still-valid folder index (instant browse after a prior Scan).
        if (auto indexed = FolderIndex::loadValidClips(dir, recursive))
        {
            std::vector<juce::File> childDirs;
            if (!recursive)
            {
                auto listing = listDirectoryQuick(dir, 0);
                childDirs = std::move(listing.directories);
            }
            juce::MessageManager::callAsync([safe, clips = std::move(*indexed),
                                             childDirs = std::move(childDirs), gen, dir,
                                             selectPath]() mutable
            {
                if (safe == nullptr || gen != safe->browseListGeneration.load())
                    return;
                if (!safe->rootDir.isDirectory()
                    || safe->rootDir.getFullPathName() != dir.getFullPathName())
                    return;

                safe->clips = std::move(clips);
                safe->folderScanned = true;
                safe->sidebarScanning = false;
                safe->sidebar.setScanning(false);
                safe->sidebar.setScanFinished(true);
                safe->fileList.setSearching(false);
                safe->cachedMidiFiles.clear();
                safe->cachedSubdirs = std::move(childDirs);
                safe->fileList.setEmptyHint(safe->clips.empty() ? "No files" : juce::String());
                safe->rebuildEntries();
                if (selectPath.isNotEmpty())
                    safe->selectPathOrFirst(selectPath);
                safe->refreshSidebar();
                safe->persistBrowserSession();
            });
            return;
        }

        if (rescanIfMissing)
        {
            juce::MessageManager::callAsync([safe, gen, dir, selectPath]()
            {
                if (safe == nullptr || gen != safe->browseListGeneration.load())
                    return;
                if (!safe->rootDir.isDirectory()
                    || safe->rootDir.getFullPathName() != dir.getFullPathName())
                    return;
                safe->rescanFolder(true, selectPath);
            });
            return;
        }

        auto listing = listDirectoryQuick(dir, FolderIndex::kMaxBrowseNames);
        std::vector<juce::File> keptDirs = std::move(listing.directories);
        std::vector<juce::File> keptMidis = std::move(listing.midiFiles);
        const int totalMidis = listing.totalMidiFiles;
        const int hiddenMidis = juce::jmax(0, totalMidis - (int) keptMidis.size());

        juce::MessageManager::callAsync([safe, subdirs = std::move(keptDirs),
                                         midiFiles = std::move(keptMidis), hiddenMidis, gen, dir]() mutable
        {
            if (safe == nullptr || gen != safe->browseListGeneration.load())
                return;
            if (!safe->rootDir.isDirectory()
                || safe->rootDir.getFullPathName() != dir.getFullPathName())
                return;

            safe->cachedSubdirs = std::move(subdirs);
            safe->cachedMidiFiles = std::move(midiFiles);
            if (safe->folderScanned)
                safe->fileList.setEmptyHint("No files");
            else if (safe->cachedSubdirs.empty() && safe->cachedMidiFiles.empty() && hiddenMidis == 0)
                safe->fileList.setEmptyHint("No files");
            else if (!safe->sidebarScanning)
                safe->fileList.setEmptyHint("No files");
            safe->rebuildEntries();
        });
    });
}

void MidiToolkitEditor::persistBrowserSession()
{
    processorRef.browseMode = browseMode;
    processorRef.starredFilter = starredFilter;
    processorRef.includeSubdirs = sidebar.getIncludeSubdirs();
    // Live sidebar criteria (query + filters) — not a stale search snapshot.
    processorRef.browserSessionSearch = sidebar.getCriteria();
    if (browseMode == 2)
        activeSearch = processorRef.browserSessionSearch;
    processorRef.activeSavedSearchIdx = activeSavedSearchIdx;
    if (rootDir.isDirectory())
        processorRef.lastBrowserDir = rootDir.getFullPathName();

    if (juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
        processorRef.selectedClipPath = clips[(size_t) selectedIdx].filePath;
    else if (folderScanned)
        processorRef.selectedClipPath.clear();

    processorRef.browserResultPaths.clear();
    if (browseMode == 2)
    {
        processorRef.browserResultPaths.ensureStorageAllocated((int) clips.size());
        for (const auto& c : clips)
            if (c.filePath.isNotEmpty())
                processorRef.browserResultPaths.add(c.filePath);
    }

    processorRef.sidebarCollapsed = sidebar.isCollapsed();
    processorRef.columnVisibility = fileList.getColumnVisibility();
    processorRef.nameColumnWidth = fileList.getNameColumnWidth();
    processorRef.browserPaneW = browserPaneW;

    sessionDirty = true;
    sessionDirtyAtMs = juce::Time::getMillisecondCounterHiRes();
}

void MidiToolkitEditor::flushBrowserSession()
{
    if (!sessionDirty)
        return;
    sessionDirty = false;
    processorRef.persistSharedUiSession();
}

void MidiToolkitEditor::selectPathOrFirst(const juce::String& path)
{
    int nextSel = -1;
    if (path.isNotEmpty())
    {
        for (int i = 0; i < (int) clips.size(); ++i)
        {
            if (clips[(size_t) i].filePath == path && displayForClip(i) >= 0)
            {
                nextSel = i;
                break;
            }
        }
    }
    if (nextSel < 0)
    {
        for (const auto& row : displayRows)
            if (!row.isDirectory)
            {
                nextSel = row.clipIndex;
                break;
            }
    }

    selectedIdx = -1;
    if (nextSel >= 0)
        selectIndex(nextSel);
    else
    {
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
        transport.setHasClip(false);
        transport.setClipName({});
    }
}

void MidiToolkitEditor::handleTrimClicked()
{
    const bool playbackLocked = (processorRef.sectionLocks & toolkitLock::Playback) != 0;
    // Nothing to remove on this file: the click turns the locked mode off.
    if (playbackLocked && processorRef.lockAutoTrim && !selectedEdit().hasTrim())
    {
        processorRef.lockAutoTrim = false;
        if (const auto* clip = selectedClip())
            processorRef.editFor(clip->filePath).clearTrim();
        effectsInspector.setTrimState(false, true, {});
        return;
    }

    // Drop the hold before toggleTrim refreshes the switch, or a locked-on
    // display would snap the control back on after the user turned it off.
    if (playbackLocked && processorRef.lockAutoTrim)
        processorRef.lockAutoTrim = false;

    rollEditor.toggleTrim();

    if (playbackLocked)
        processorRef.lockAutoTrim = selectedEdit().hasTrim();

    if (playbackLocked && processorRef.lockAutoTrim)
        effectsInspector.setTrimState(true, true, {});
    else if (!selectedEdit().hasTrim())
        effectsInspector.setTrimState(false, true, {});
}

void MidiToolkitEditor::restoreBrowserSession()
{
    // Restore sidebar chrome before any rescan so filters / include-subdirs apply.
    sidebar.setIncludeSubdirs(processorRef.includeSubdirs);
    sidebar.setCriteria(processorRef.browserSessionSearch);

    rootDir = juce::File();

    browseMode = juce::jlimit(0, 3, processorRef.browseMode);
    starredFilter = processorRef.starredFilter;
    activeSavedSearchIdx = processorRef.activeSavedSearchIdx;
    activeSearch = processorRef.browserSessionSearch;
    recursiveBrowse = processorRef.includeSubdirs;
    sidebar.setBrowseMode(browseMode);
    sidebar.setStarredFilter(starredFilter);

    const juce::String wantPath = processorRef.selectedClipPath;

    if (browseMode == 1)
    {
        loadStarredClips(wantPath);
        fileList.grabBrowseFocus();
    }
    else if (browseMode == 2)
    {
        if (processorRef.browserResultPaths.size() > 0)
        {
            activeSearch = processorRef.browserSessionSearch;
            recursiveBrowse = false;
            setBrowseMode(2);
            juce::String title = "Search";
            if (activeSearch.query.isNotEmpty())
                title = activeSearch.query;
            fileList.setFolderName(title);
            loadClipsFromPathsAsync(processorRef.browserResultPaths,
                [this, wantPath](std::vector<StepClip>&& found)
                {
                    if (found.empty())
                    {
                        runSearchAsync(activeSearch, activeSavedSearchIdx, {}, false);
                        return;
                    }
                    clips = std::move(found);
                    rebuildEntries();
                    selectPathOrFirst(wantPath);
                    persistBrowserSession();
                },
                rootDir.isDirectory() ? rootDir
                    : juce::File(processorRef.lastBrowserDir));
        }
        else
        {
            runSearchAsync(activeSearch, activeSavedSearchIdx);
        }
        fileList.grabBrowseFocus();
    }
    else if (browseMode == 3)
    {
        loadStarredFolders();
        fileList.grabBrowseFocus();
    }
    else
    {
        browseMode = 0;
        processorRef.browseMode = 0;
        sidebar.setBrowseMode(0);
        searchDuplicateLocations.clear();
        const auto dir = juce::File(processorRef.lastBrowserDir);
        if (dir.isDirectory())
        {
            // Hosts destroy the editor when focus moves to another plugin.
            // Reopen the folder from the index so the list is back without a manual scan.
            rootDir = dir;
            recursiveBrowse = processorRef.includeSubdirs;
            sidebar.setIncludeSubdirs(recursiveBrowse);
            auto name = dir.getFileName();
            if (name.isEmpty()) name = dir.getFullPathName();
            fileList.setFolderName(name);
            ++browseListGeneration;
            refreshBrowseListingAsync(wantPath, true);
        }
        else
        {
            clips.clear();
            fileList.setFolderName("Select a folder");
            rebuildEntries();
            rollEditor.clearClip();
            syncEffectsInspector();
            processorRef.setPreviewState({}, false, false, false);
        }
        fileList.grabBrowseFocus();
    }

    persistBrowserSession();
    // Selection lands when the folder index (or rescan) finishes. Keep the path
    // until then so a host state flush does not forget the open file.
    if (browseMode == 0 && rootDir.isDirectory() && wantPath.isNotEmpty() && selectedIdx < 0)
        processorRef.selectedClipPath = wantPath;
}

void MidiToolkitEditor::rescanFolder(bool keepSelection, const juce::String& preferredPath)
{
    if (!rootDir.isDirectory() && processorRef.lastBrowserDir.isNotEmpty())
        rootDir = juce::File(processorRef.lastBrowserDir);
    if (!rootDir.isDirectory())
    {
        clips.clear();
        fileList.setFolderName("Select a folder");
        rebuildEntries();
        return;
    }

    juce::String previousPath = preferredPath;
    if (previousPath.isEmpty() && keepSelection
        && juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
        previousPath = clips[(size_t) selectedIdx].filePath;

    recursiveBrowse = sidebar.getIncludeSubdirs();
    const juce::File dir = rootDir;
    const bool recursive = recursiveBrowse;
    auto name = dir.getFileName();
    if (name.isEmpty()) name = dir.getFullPathName();
    fileList.setFolderName(name);
    fileList.setEmptyHint("Scanning...");

    // Drop the previous folder's rows. A stale subdirectory cache used to skip
    // the listing, so the folder you just entered showed no MIDI files.
    clips.clear();
    folderScanned = false;
    selectedIdx = -1;
    cachedSubdirs.clear();
    cachedMidiFiles.clear();
    fileList.setSearching(false);
    rebuildEntries();
    if (!recursive)
        refreshBrowseListingAsync();
    rollEditor.clearClip();
    syncEffectsInspector();
    processorRef.setPreviewState({}, false, false, false);

    // Metadata scan stays in the background. Names from the listing stay visible.
    const int gen = beginBackgroundClipLoad(false);
    juce::Component::SafePointer<MidiToolkitEditor> safe(this);
    juce::Thread::launch([safe, dir, recursive, gen, previousPath]()
    {
        std::vector<StepClip> found;
        try
        {
            {
                // Always enumerate first, then parse in parallel. Recursive "Scan all"
                // on Midi_Parsed (~411k) is still heavy — progress keeps UI alive.
                auto files = collectMidiFiles(dir, recursive);
                const int total = files.size();
                found.resize((size_t) total);
                if (safe != nullptr)
                {
                    juce::MessageManager::callAsync([safe, gen, total]()
                    {
                        if (safe == nullptr || gen != safe->folderScanGeneration.load())
                            return;
                        safe->sidebar.setScanProgress(0, total);
                        safe->fileList.setScanProgress(0, total);
                    });
                }
                std::atomic<int> done { 0 };
                std::atomic<juce::int64> lastProgressMs { 0 };
                const int nThreads = juce::jmax(2, (int) std::thread::hardware_concurrency());
                juce::ThreadPool pool(nThreads);
                for (int i = 0; i < total; ++i)
                {
                    pool.addJob([safe, gen, &files, &found, &done, &lastProgressMs, i, total]()
                    {
                        const auto& file = files.getReference(i);
                        try
                        {
                            found[(size_t) i] = scanMidiBrowseMeta(file);
                        }
                        catch (...)
                        {
                            StepClip stub;
                            stub.filePath = file.getFullPathName();
                            stub.name = file.getFileNameWithoutExtension();
                            applyFilenameKindTag(stub);
                            found[(size_t) i] = std::move(stub);
                        }
                        const int d = ++done;
                        const auto now = (juce::int64) juce::Time::getMillisecondCounter();
                        if (safe != nullptr
                            && claimScanProgressPublish(d, total, lastProgressMs, now))
                        {
                            juce::MessageManager::callAsync([safe, gen, d, total]()
                            {
                                if (safe == nullptr || gen != safe->folderScanGeneration.load())
                                    return;
                                safe->sidebar.setScanProgress(d, total);
                                safe->fileList.setScanProgress(d, total);
                            });
                        }
                    });
                }
                while (done.load() < total)
                    juce::Thread::sleep(5);
                FolderIndex::fromClips(dir, recursive, found).save();
            }
        }
        catch (...)
        {
            found.clear();
        }

        juce::MessageManager::callAsync([safe, results = std::move(found), gen, previousPath,
                                         dir]() mutable
        {
            if (safe == nullptr || gen != safe->folderScanGeneration.load())
                return;
            safe->sidebarScanning = false;
            safe->sidebar.setScanning(false);
            safe->fileList.setSearching(false);
            if (!safe->rootDir.isDirectory()
                || safe->rootDir.getFullPathName() != dir.getFullPathName())
                return;

            if (results.empty() && !safe->cachedMidiFiles.empty())
            {
                // The directory listing already has MIDI names. An empty parse
                // must not replace that list with a blank browser.
                safe->folderScanned = false;
                safe->rebuildEntries();
                safe->refreshSidebar();
                return;
            }

            safe->clips = std::move(results);
            safe->folderScanned = true;
            safe->sidebar.setScanFinished(true);
            safe->cachedMidiFiles.clear();
            auto folderName = dir.getFileName();
            if (folderName.isEmpty()) folderName = dir.getFullPathName();
            safe->fileList.setFolderName(folderName);
            safe->fileList.setEmptyHint("No files");
            safe->rebuildEntries();
            safe->selectPathOrFirst(previousPath);
            safe->refreshSidebar();
            safe->persistBrowserSession();
        });
    });
}

void MidiToolkitEditor::scanAllFolders()
{
    sidebar.setIncludeSubdirs(true);
    if (rootDir.isDirectory()
        || (processorRef.lastBrowserDir.isNotEmpty()
            && juce::File(processorRef.lastBrowserDir).isDirectory()))
    {
        if (!rootDir.isDirectory())
            rootDir = juce::File(processorRef.lastBrowserDir);
        rescanFolder(true);
        refreshSidebar();
        fileList.grabBrowseFocus();
    }
}

void MidiToolkitEditor::applyBrowserFilter()
{
    const juce::String previousPath =
        (selectedIdx >= 0 && selectedIdx < (int) clips.size())
            ? clips[(size_t) selectedIdx].filePath : juce::String();

    rebuildEntries();

    int nextSel = selectedIdx;
    if (nextSel >= 0 && displayForClip(nextSel) < 0)
        nextSel = -1;
    if (nextSel < 0 && previousPath.isNotEmpty())
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath && displayForClip(i) >= 0)
            {
                nextSel = i;
                break;
            }
    if (nextSel < 0)
    {
        for (const auto& row : displayRows)
            if (!row.isDirectory) { nextSel = row.clipIndex; break; }
    }

    if (nextSel >= 0)
    {
        if (nextSel != selectedIdx)
            selectIndex(nextSel);
        else if (const int d = displayForClip(selectedIdx); d >= 0)
            fileList.setSelectedIndex(d, juce::dontSendNotification);
    }
    else if (selectedIdx >= 0)
    {
        selectedIdx = -1;
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
    }
    if (browseMode == 0 && rootDir.isDirectory()
        && processorRef.savedBrowserDirs.contains(rootDir.getFullPathName()))
    {
        SavedLibraryFolder folder;
        folder.path = rootDir.getFullPathName();
        folder.scanSubdirs = sidebar.getIncludeSubdirs();
        folder.filter = sidebar.getFilter();
        folder.filter.subdirs = folder.scanSubdirs;
        processorRef.saveLibraryFolder(folder, true);
    }
    persistBrowserSession();
}

void MidiToolkitEditor::setBrowseMode(int mode)
{
    // 0 = folder, 1 = starred files, 2 = search, 3 = starred folders.
    browseMode = juce::jlimit(0, 3, mode);
    sidebar.setBrowseMode(browseMode);
    persistBrowserSession();
}

void MidiToolkitEditor::loadStarredClips(const juce::String& preferredPath)
{
    juce::String previousPath = preferredPath;
    if (previousPath.isEmpty()
        && juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
        previousPath = clips[(size_t) selectedIdx].filePath;

    fileList.setFolderName("Favorites");
    clips.clear();
    selectedIdx = -1;
    rebuildEntries();
    rollEditor.clearClip();
    syncEffectsInspector();
    processorRef.setPreviewState({}, false, false, false);

    loadClipsFromPathsAsync(processorRef.starredFiles,
        [this, previousPath](std::vector<StepClip>&& found)
        {
            clips = std::move(found);
            fileList.setFolderName("Favorite Files");
            rebuildEntries();
            selectPathOrFirst(previousPath);
            fileList.grabBrowseFocus();
            persistBrowserSession();
        });
}

void MidiToolkitEditor::loadStarredFolders()
{
    clips.clear();
    selectedIdx = -1;
    rollEditor.clearClip();
    syncEffectsInspector();
    processorRef.setPreviewState({}, false, false, false);
    rebuildEntries();
    fileList.grabBrowseFocus();
    persistBrowserSession();
}

bool MidiToolkitEditor::clipMatchesSearch(const StepClip& clip, const juce::File& file,
                                          const BrowserSearch& criteria) const
{
    return clipMatches(clip, file, criteria);
}

void MidiToolkitEditor::runSearch(const BrowserSearch& criteria)
{
    const juce::String previousPath =
        (selectedIdx >= 0 && selectedIdx < (int) clips.size())
            ? clips[(size_t) selectedIdx].filePath : juce::String();

    activeSearch = criteria;
    recursiveBrowse = false;
    clips.clear();
    searchDuplicateLocations.clear();

    const juce::File searchRoot = rootDir.isDirectory() ? rootDir
        : juce::File(processorRef.lastBrowserDir);
    if (searchRoot.isDirectory())
    {
        // Disk scan matches query only; FILTER is applied in rebuildEntries.
        const auto matcher = [criteria](const StepClip&, const juce::File& f)
        {
            return clipMatchesQuery(f, criteria);
        };
        scanMidiFiles(searchRoot, criteria.subdirs, clips, matcher);
    }

    juce::String title = "Search";
    if (criteria.query.isNotEmpty())
        title = criteria.query;
    else if (const auto bpmTitle = searchBpmTitle(criteria); bpmTitle.isNotEmpty())
        title = bpmTitle;
    fileList.setFolderName(title);
    rebuildEntries();

    int nextSel = -1;
    if (previousPath.isNotEmpty())
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath && displayForClip(i) >= 0)
                nextSel = i;
    if (nextSel < 0)
        for (const auto& row : displayRows)
            if (!row.isDirectory) { nextSel = row.clipIndex; break; }

    selectedIdx = -1;
    if (nextSel >= 0)
        selectIndex(nextSel);
    else
    {
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
    }
    fileList.grabBrowseFocus();
}

void MidiToolkitEditor::rememberSearchSnapshot(int savedIdx, SearchSnapshot snap)
{
    lastSearchSnapshot = snap;
    if (savedIdx >= 0)
        savedSearchSnapshots[savedIdx] = std::move(snap);
}

void MidiToolkitEditor::forgetSavedSearchSnapshot(int index)
{
    savedSearchSnapshots.erase(index);
    std::map<int, SearchSnapshot> next;
    for (auto& [k, v] : savedSearchSnapshots)
    {
        if (k < index)
            next[k] = std::move(v);
        else if (k > index)
            next[k - 1] = std::move(v);
    }
    savedSearchSnapshots = std::move(next);
}

void MidiToolkitEditor::applySearchSnapshot(std::vector<StepClip> found,
                                            std::map<juce::String, juce::StringArray> locMap,
                                            const BrowserSearch& criteria, int savedIdx,
                                            const juce::String& rootPath, bool showSearching)
{
    if (showSearching)
        fileList.setSearching(true);

    activeSearch = criteria;
    activeSavedSearchIdx = savedIdx;
    recursiveBrowse = false;
    setBrowseMode(2);

    SearchSnapshot snap;
    snap.criteria = criteria;
    snap.rootPath = rootPath;
    snap.clips = found;          // full query hits; filter applied on display
    snap.locations = std::move(locMap);
    rememberSearchSnapshot(savedIdx, std::move(snap));

    clips = std::move(found);
    juce::String title = "Search";
    if (criteria.query.isNotEmpty())
        title = criteria.query;
    else if (const auto bpmTitle = searchBpmTitle(criteria); bpmTitle.isNotEmpty())
        title = bpmTitle;
    fileList.setFolderName(title);
    rebuildEntries();
    fileList.setSearching(false);
    refreshSidebar();

    selectedIdx = -1;
    int nextSel = -1;
    for (const auto& row : displayRows)
        if (!row.isDirectory) { nextSel = row.clipIndex; break; }
    if (nextSel >= 0)
        selectIndex(nextSel);
    else
    {
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
        persistBrowserSession();
    }
    fileList.grabBrowseFocus();
}

void MidiToolkitEditor::runSearchAsync(const BrowserSearch& criteria, int savedIdx,
                                       juce::File searchRoot, bool allowCache)
{
    // Keep SEARCH/FILTER sections open; just blur the query field.
    sidebar.deactivateSearch(false);

    if (!searchRoot.isDirectory())
        searchRoot = rootDir.isDirectory() ? rootDir
            : juce::File(processorRef.lastBrowserDir);
    if (!searchRoot.isDirectory())
    {
        fileList.setSearching(false);
        fileList.setFolderName("Open a folder to search");
        clips.clear();
        rebuildEntries();
        return;
    }

    const int gen = ++searchGeneration;
    activeSearch = criteria;
    if (savedIdx < 0)
        activeSavedSearchIdx = -1;
    else
        activeSavedSearchIdx = savedIdx;
    starredFilter = false;
    recursiveBrowse = false;
    sidebar.setStarredFilter(false);
    setBrowseMode(2);

    // Cache key is query-only; filters apply afterward in rebuildEntries.
    const auto cacheKey = searchScanKey(criteria);

    if (allowCache)
    {
        if (const auto* cached = processorRef.library().findSearchCache(searchRoot, cacheKey))
        {
            if (cached->resultPaths.size() > 0)
            {
                const auto cachedPaths = cached->resultPaths;
                const auto rootPath = searchRoot.getFullPathName();
                loadClipsFromPathsAsync(cachedPaths,
                    [this, criteria, savedIdx, rootPath, cachedPaths](std::vector<StepClip>&& found)
                    {
                        if (found.empty())
                        {
                            // Cached paths are stale — rescan disk, skip cache.
                            runSearchAsync(criteria, savedIdx, juce::File(rootPath), false);
                            return;
                        }
                        if (savedIdx >= 0)
                            processorRef.updateSavedSearchResults(savedIdx, cachedPaths, rootPath);
                        applySearchSnapshot(std::move(found), {}, criteria, savedIdx, rootPath, false);
                    },
                    searchRoot);
                return;
            }
        }
    }

    fileList.setSearching(true);
    ++folderScanGeneration; // cancel any folder load while searching

    juce::Component::SafePointer<MidiToolkitEditor> safe(this);
    juce::Thread::launch([safe, criteria, cacheKey, searchRoot, gen, savedIdx]
    {
        std::vector<StepClip> found;
        juce::StringArray resultPaths;
        try
        {
            const auto matcher = [criteria](const StepClip&, const juce::File& f)
            {
                return clipMatchesQuery(f, criteria);
            };

            // Fast path: filter a prior Scan's FolderIndex instead of re-parsing MIDI.
            if (auto indexed = FolderIndex::loadValidClips(searchRoot, criteria.subdirs))
            {
                found.reserve(indexed->size());
                for (auto& c : *indexed)
                {
                    const juce::File f(c.filePath);
                    if (!f.existsAsFile())
                        continue;
                    if (!matcher(c, f))
                        continue;
                    resultPaths.add(c.filePath);
                    found.push_back(std::move(c));
                }
            }
            else
            {
                scanMidiFiles(searchRoot, criteria.subdirs, found, matcher);
                for (const auto& c : found)
                    resultPaths.add(c.filePath);
            }
        }
        catch (...)
        {
            found.clear();
            resultPaths.clear();
        }

        juce::MessageManager::callAsync([safe, results = std::move(found), criteria, cacheKey,
                                         searchRoot, paths = std::move(resultPaths),
                                         gen, savedIdx]() mutable
        {
            if (safe == nullptr || gen != safe->searchGeneration.load())
                return;

            safe->processorRef.library().putSearchCache(searchRoot, cacheKey, paths);
            if (savedIdx >= 0)
                safe->processorRef.updateSavedSearchResults(savedIdx, paths,
                                                            searchRoot.getFullPathName());
            safe->applySearchSnapshot(std::move(results), {}, criteria, savedIdx,
                                      searchRoot.getFullPathName(), false);
        });
    });
}

void MidiToolkitEditor::saveCurrentSearch()
{
    // Prefer the live sidebar form; fall back to the last-run search.
    BrowserSearch criteria = sidebar.getCriteria();
    const bool formEmpty = criteria.query.isEmpty()
        && criteria.keyRoot < 0 && criteria.keyMask == 0
        && criteria.bpmMin <= 0.0 && criteria.bpmMax <= 0.0
        && criteria.barsMin <= 0 && criteria.barsMax <= 0
        && criteria.complexityMin <= 0 && criteria.complexityMax <= 0
        && criteria.kindMask == 0
        && criteria.tagMask == 0
        && criteria.timeSigMask == 0
        && !criteria.subdirs
        && !criteria.removeDuplicates;
    if (formEmpty && browseMode == 2)
        criteria = activeSearch;
    if (formEmpty && browseMode != 2)
        return;

    SavedSearchEntry entry;
    entry.search = criteria;
    if (LibraryStore::searchesEqual(lastSearchSnapshot.criteria, criteria)
        && lastSearchSnapshot.rootPath.isNotEmpty())
        entry.rootPath = lastSearchSnapshot.rootPath;
    else if (rootDir.isDirectory())
        entry.rootPath = rootDir.getFullPathName();
    else
        entry.rootPath = processorRef.lastBrowserDir;

    // Capture result paths so the saved search reopens instantly next session.
    // Prefer the last search snapshot, then currently visible (filtered) rows,
    // then the full clip list — so Keep+filters and folder filters persist too.
    auto capturePaths = [&](const std::vector<StepClip>& pool)
    {
        entry.resultPaths.clear();
        entry.resultPaths.ensureStorageAllocated((int) pool.size());
        for (const auto& c : pool)
            if (c.filePath.isNotEmpty())
                entry.resultPaths.add(c.filePath);
    };

    if (LibraryStore::searchesEqual(lastSearchSnapshot.criteria, criteria)
        && !lastSearchSnapshot.clips.empty())
    {
        capturePaths(lastSearchSnapshot.clips);
    }
    else if (!displayRows.empty())
    {
        entry.resultPaths.clear();
        for (const auto& row : displayRows)
        {
            if (row.isDirectory)
                continue;
            if (juce::isPositiveAndBelow(row.clipIndex, (int) clips.size()))
            {
                const auto& path = clips[(size_t) row.clipIndex].filePath;
                if (path.isNotEmpty())
                    entry.resultPaths.add(path);
            }
            else if (row.file.existsAsFile())
            {
                entry.resultPaths.add(row.file.getFullPathName());
            }
        }
    }
    else if (!clips.empty())
    {
        capturePaths(clips);
    }

    entry.name = savedSearchShorthand(criteria);

    processorRef.addSavedSearch(entry);
    activeSavedSearchIdx = (int) processorRef.savedSearches.size() - 1;

    // Attach the last completed results so re-opening is instant this session.
    if (LibraryStore::searchesEqual(lastSearchSnapshot.criteria, criteria))
        savedSearchSnapshots[activeSavedSearchIdx] = lastSearchSnapshot;

    // Keep the generic search cache warm for the same criteria.
    if (entry.resultPaths.size() > 0 && entry.rootPath.isNotEmpty())
        processorRef.library().putSearchCache(juce::File(entry.rootPath),
                                              searchScanKey(criteria),
                                              entry.resultPaths);

    refreshSidebar();
    persistBrowserSession();
}


bool MidiToolkitEditor::ensureClipNotesLoaded(int clipIndex)
{
    if (!juce::isPositiveAndBelow(clipIndex, (int) clips.size()))
        return false;

    auto& clip = clips[(size_t) clipIndex];
    if (clip.notesResident || !clip.notes.empty())
        return true;

    if (auto cached = noteCache.find(clip.filePath))
    {
        clip = std::move(*cached);
        return clip.notesResident || !clip.notes.empty();
    }

    const juce::File f(clip.filePath);
    if (!f.existsAsFile())
        return false;

    try
    {
        auto loaded = makeStepClip(parseMidiFile(f));
        loaded.filePath = clip.filePath;
        loaded.notesResident = true;
        noteCache.store(loaded);
        clip = std::move(loaded);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

void MidiToolkitEditor::loadClipNotesAsync(int clipIndex, bool applyWhenReady)
{
    if (!juce::isPositiveAndBelow(clipIndex, (int) clips.size()))
        return;

    auto& clip = clips[(size_t) clipIndex];
    if (clip.notesResident || !clip.notes.empty())
    {
        if (applyWhenReady && selectedIdx == clipIndex)
        {
            finishClipSelection();
            prefetchNearbyClips(clipIndex);
        }
        return;
    }

    if (auto cached = noteCache.find(clip.filePath))
    {
        clip = std::move(*cached);
        if (applyWhenReady && selectedIdx == clipIndex)
        {
            finishClipSelection();
            prefetchNearbyClips(clipIndex);
        }
        return;
    }

    const auto path = clip.filePath;
    if (path.isEmpty())
        return;

    const int gen = applyWhenReady ? ++noteLoadGeneration : noteLoadGeneration.load();
    juce::Component::SafePointer<MidiToolkitEditor> safe(this);
    juce::Thread::launch([safe, path, clipIndex, gen, applyWhenReady]()
    {
        StepClip loaded;
        try
        {
            loaded = makeStepClip(parseMidiFile(juce::File(path)));
        }
        catch (...)
        {
            loaded.filePath = path;
            loaded.name = juce::File(path).getFileNameWithoutExtension();
        }
        loaded.filePath = path;
        loaded.notesResident = true;

        juce::MessageManager::callAsync([safe, loaded = std::move(loaded),
                                         clipIndex, gen, applyWhenReady]() mutable
        {
            if (safe == nullptr)
                return;
            if (applyWhenReady && gen != safe->noteLoadGeneration.load())
            {
                safe->noteCache.store(std::move(loaded));
                return;
            }
            if (juce::isPositiveAndBelow(clipIndex, (int) safe->clips.size())
                && safe->clips[(size_t) clipIndex].filePath == loaded.filePath)
            {
                safe->noteCache.store(loaded);
                safe->clips[(size_t) clipIndex] = std::move(loaded);
            }
            else
            {
                safe->noteCache.store(std::move(loaded));
            }
            if (applyWhenReady && safe->selectedIdx == clipIndex)
            {
                safe->finishClipSelection();
                safe->prefetchNearbyClips(clipIndex);
            }
        });
    });
}

void MidiToolkitEditor::prefetchNearbyClips(int clipIndex)
{
    const int next = clipIndex + browseDirection;
    if (!juce::isPositiveAndBelow(next, (int) clips.size()))
        return;
    const auto& clip = clips[(size_t) next];
    if (clip.notesResident || !clip.notes.empty() || noteCache.contains(clip.filePath))
        return;
    loadClipNotesAsync(next, false);
}

void MidiToolkitEditor::ensureAndSelectFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    const auto path = file.getFullPathName();
    for (int i = 0; i < (int) clips.size(); ++i)
    {
        if (clips[(size_t) i].filePath == path)
        {
            selectIndex(i);
            return;
        }
    }

    try
    {
        clips.push_back(makeStepClip(parseMidiFile(file)));
    }
    catch (...)
    {
        return;
    }

    rebuildEntries();
    selectIndex((int) clips.size() - 1);
}

void MidiToolkitEditor::rebuildEntries()
{
    displayRows.clear();
    std::vector<FileListEntry> entries;

    // Re-apply filename keywords so stale Lead tags (pre-lexicon scans / stubs) correct.
    for (auto& clip : clips)
        applyFilenameKindTag(clip);

    const auto filter = sidebar.getFilter();

    // Build the clip pool that passes the FILTER (search already narrowed by query).
    std::vector<int> passing;
    passing.reserve(clips.size());
    for (int i = 0; i < (int) clips.size(); ++i)
    {
        const auto& clip = clips[(size_t) i];
        const bool starred = processorRef.isStarred(clip.filePath);
        // Starred library mode already lists only favourites — don't re-filter stars.
        if (starredFilter && browseMode != 1 && !starred)
            continue;
        // Favorites views keep every starred path; FILTER chips don't hide them.
        if (browseMode != 1 && !clipMatchesFilter(clip, filter))
            continue;
        passing.push_back(i);
    }

    std::set<juce::String> hiddenDupes;
    searchDuplicateLocations.clear();
    if (filter.removeDuplicates && !passing.empty())
    {
        std::vector<StepClip> pool;
        pool.reserve(passing.size());
        for (int i : passing)
            pool.push_back(clips[(size_t) i]);
        searchDuplicateLocations = computeDedupeLocations(pool, hiddenDupes);
    }

    if (browseMode == 0 && rootDir.isDirectory() && !recursiveBrowse)
    {
        for (const auto& d : cachedSubdirs)
        {
            FileListEntry e;
            e.file = d;
            e.name = d.getFileName();
            e.isDirectory = true;
            e.starred = processorRef.isStarredFolder(d.getFullPathName());
            entries.push_back(e);
            displayRows.push_back({ true, -1, d });
        }
    }
    if (browseMode == 3)
    {
        for (const auto& path : processorRef.starredFolders)
        {
            const juce::File d(path);
            if (!d.isDirectory()) continue;
            FileListEntry e;
            e.file = d;
            e.name = d.getFileName().isNotEmpty() ? d.getFileName() : path;
            e.isDirectory = true;
            e.starred = true;
            entries.push_back(e);
            displayRows.push_back({ true, -1, d });
        }
        fileList.setEntries(std::move(entries));
        fileList.setFolderName("Favorite Folders");
        return;
    }

    // Show filenames until a scan has produced clips. An empty scan must not
    // hide MIDI files the directory listing already found.
    const bool showListedNames = browseMode == 0 && !recursiveBrowse
                                 && (clips.empty() || !folderScanned);
    if (showListedNames)
    {
        for (const auto& f : cachedMidiFiles)
        {
            const auto path = f.getFullPathName();
            int clipIdx = -1;
            for (int i = 0; i < (int) clips.size(); ++i)
                if (clips[(size_t) i].filePath == path) { clipIdx = i; break; }

            FileListEntry e;
            if (clipIdx >= 0)
            {
                const auto& clip = clips[(size_t) clipIdx];
                const bool starred = processorRef.isStarred(clip.filePath);
                const bool edited = (processorRef.sectionLocks & toolkitLock::Pitch) != 0
                                        && !editIsClean(processorRef.editFor(clip.filePath));
                e = entryForClip(clip, f, edited, starred);
            }
            else
            {
                e.file = f;
                e.name = f.getFileNameWithoutExtension();
                e.starred = processorRef.isStarred(path);
                if (auto tagged = kindKeywordFromFileName(path))
                    e.kind = *tagged;
                else if (auto tagged = kindKeywordFromFileName(e.name))
                    e.kind = *tagged;
            }
            entries.push_back(std::move(e));
            displayRows.push_back({ false, clipIdx, f });
        }
    }
    else
    {
        for (int i : passing)
        {
            const auto& clip = clips[(size_t) i];
            if (hiddenDupes.count(clip.filePath) != 0)
                continue;

            const bool starred = processorRef.isStarred(clip.filePath);
            const bool edited = (processorRef.sectionLocks & toolkitLock::Pitch) != 0
                                    && !editIsClean(processorRef.editFor(clip.filePath));
            auto entry = entryForClip(clip, juce::File(clip.filePath), edited, starred);
            if (auto it = searchDuplicateLocations.find(clip.filePath);
                it != searchDuplicateLocations.end() && it->second.size() > 1)
                entry.locations = it->second;
            entries.push_back(std::move(entry));
            displayRows.push_back({ false, i, juce::File(clip.filePath) });
        }
    }

    fileList.setEntries(std::move(entries));
    sidebar.setFilterHistograms(clips);
    if (browseMode == 0)
    {
        int files = 0;
        for (const auto& row : displayRows)
            if (!row.isDirectory)
                ++files;
        sidebar.setCurrentClipCount(files);
    }

    if (const int d = displayForClip(selectedIdx); d >= 0)
        fileList.setSelectedIndex(d, juce::dontSendNotification);
}

int MidiToolkitEditor::displayForClip(int clipIdx) const
{
    for (int i = 0; i < (int) displayRows.size(); ++i)
        if (!displayRows[(size_t) i].isDirectory && displayRows[(size_t) i].clipIndex == clipIdx)
            return i;
    return -1;
}

void MidiToolkitEditor::enterFolderAtDisplay(int displayIdx)
{
    if (browseMode != 0)
        return;
    if (!juce::isPositiveAndBelow(displayIdx, (int) displayRows.size()))
        return;
    const auto& row = displayRows[(size_t) displayIdx];
    if (!row.isDirectory || !row.file.isDirectory())
        return;
    setRootDirectory(row.file);
}

void MidiToolkitEditor::enterParentFolder()
{
    if (!rootDir.isDirectory())
        return;
    const auto parent = rootDir.getParentDirectory();
    if (parent != rootDir && parent.isDirectory())
        setRootDirectory(parent);
}

void MidiToolkitEditor::applyTimeStretchFromMultiplier()
{
    const double mult = juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
    const double stretch = 1.0 / mult;
    rollEditor.setTimeStretch(stretch);
    if (selectedClip() != nullptr)
        pushPreviewToProcessor();
}

void MidiToolkitEditor::syncEffectsInspector()
{
    // Form + pitch context: any loaded library clip or an active recording/generator clip.
    const auto* clip = hasRecording ? &recordedClip : selectedClip();
    // Keep Form rows available as soon as a clip is on the roll / selected.
    effectsInspector.setHasClip(clip != nullptr);
    effectsInspector.setClipRoot(clip != nullptr ? clip->root : -1);
    effectsInspector.setClipSourceOctave(clip != nullptr ? clipReferenceOctave(*clip) : 4);
    effectsInspector.setClipComplexity(clip != nullptr ? clip->complexity : 50);
    if (clip != nullptr)
        effectsInspector.setClipScaleAnalysis(analyseClipScale(*clip));
    else
        effectsInspector.setClipScaleAnalysis({});
    // Keep generator/recording Form edits; only reload library-clip edit/groove maps.
    if (!hasRecording)
    {
        effectsInspector.setEdit(selectedEdit(), juce::dontSendNotification);
        effectsInspector.setGroove(selectedGroove(), juce::dontSendNotification);
    }
    effectsInspector.setBpmMultiplier(processorRef.bpmMultiplier.load(), juce::dontSendNotification);
    effectsInspector.setSectionLocks(processorRef.sectionLocks);
    updateMiniPreview();
}

void MidiToolkitEditor::chooseFolder()
{
    auto chooser = std::make_shared<juce::FileChooser>("Select MIDI folder", rootDir, "");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc)
        {
            if (fc.getResult().isDirectory())
                setRootDirectory(fc.getResult());
        });
}

const StepClip* MidiToolkitEditor::selectedClip() const
{
    if (juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
        return &clips[(size_t) selectedIdx];
    return nullptr;
}

ClipEdit MidiToolkitEditor::selectedEdit() const
{
    if (const auto* c = selectedClip())
        return processorRef.clipEdits.count(c->filePath) > 0
                   ? processorRef.clipEdits.at(c->filePath) : ClipEdit();
    return {};
}

GrooveParams MidiToolkitEditor::selectedGroove() const
{
    if (const auto* c = selectedClip())
        return processorRef.clipGrooves.count(c->filePath) > 0
                   ? processorRef.clipGrooves.at(c->filePath) : GrooveParams();
    return {};
}

void MidiToolkitEditor::regenerateGeneratedClip(const GrooveParams& groove)
{
    if (recording || lastGeneratedPart < 0)
        return;

    const bool firstGenerate = !hasRecording
        || !recordedClip.filePath.startsWith("generator://");
    const auto part = (gen::Part) juce::jlimit(0, (int) gen::Part::Count - 1, lastGeneratedPart);
    const auto edit = effectsInspector.getEdit();
    int keyRoot = edit.root >= 0 ? edit.root : 0;
    if (const auto* clip = selectedClip(); clip != nullptr && clip->root >= 0 && edit.root < 0)
        keyRoot = clip->root;

    int chordBuf[1] {};
    int chordCount = 0;
    const int latch = processorRef.liveFxPitchLatch();
    if (latch >= 0)
    {
        chordBuf[0] = latch;
        chordCount = 1;
        if (edit.root < 0)
            keyRoot = latch % 12;
    }

    const double prevBpm = recordedClip.bpm;
    recordedClip = gen::renderPartClip(part, groove, keyRoot, edit.mode,
                                       chordCount > 0 ? chordBuf : nullptr, chordCount);
    const double bpm = processorRef.hostBpm.load();
    recordedClip.bpm = prevBpm > 0.0 ? prevBpm
                     : (bpm > 0.0 ? bpm : processorRef.freeBpm.load());
    hasRecording = true;
    selectedIdx = -1;
    fileList.setSelectedIndex(-1, juce::dontSendNotification);

    if (!processorRef.editorOpen)
    {
        processorRef.previewOpen = true;
        transport.setEditorOpen(false);
    }
    transport.setClipBpm(recordedClip.bpm);
    transport.setClipName(recordedClip.name);
    transport.setHasClip(!recordedClip.notes.empty());
    rollEditor.setEmptyHint(recordedClip.notes.empty()
                                ? "Generator produced no notes — try Generate again."
                                : juce::String());
    // Force a fresh vertical fit even when the generator:// path is unchanged.
    rollEditor.clearClip();
    rollEditor.setClip(recordedClip, edit, groove);
    effectsInspector.setHasClip(true);
    effectsInspector.setClipRoot(recordedClip.root);
    effectsInspector.setClipSourceOctave(clipReferenceOctave(recordedClip));
    effectsInspector.setClipComplexity(recordedClip.complexity);
    effectsInspector.setClipScaleAnalysis(analyseClipScale(recordedClip));
    pushPreviewToProcessor();
    updateMiniPreview();
    if (firstGenerate)
        applyLayoutState();
    if (!recordedClip.notes.empty())
        rollEditor.setClip(recordedClip, effectsInspector.getEdit(), effectsInspector.getGroove());
}

void MidiToolkitEditor::selectIndex(int index)
{
    if (recording)
        return;
    if (!juce::isPositiveAndBelow(index, (int) clips.size()))
        return;
    hasRecording = false;
    lastGeneratedPart = -1;

    const auto locks = processorRef.sectionLocks;
    const int previous = selectedIdx;

    // Carry Fit/Map/Key/Mode across unlocked browse so audition stays in the
    // chosen scale (still ephemeral — cleared when Pitch stays unlocked).
    pendingCarriedScale = {};
    havePendingCarriedScale = false;
    if (previous >= 0 && previous != index
        && (locks & toolkitLock::Pitch) == 0
        && juce::isPositiveAndBelow(previous, (int) clips.size()))
    {
        const auto& prev = clips[(size_t) previous];
        const auto& prevEdit = processorRef.editFor(prev.filePath);
        if (prevEdit.fitScale || prevEdit.mapToRoot)
        {
            pendingCarriedScale.fitScale = prevEdit.fitScale;
            pendingCarriedScale.mapToRoot = prevEdit.mapToRoot;
            pendingCarriedScale.root = prevEdit.root;
            pendingCarriedScale.mode = prevEdit.mode;
            havePendingCarriedScale = prevEdit.root >= 0;
        }
        processorRef.clipEdits.erase(prev.filePath);
        refreshEntryMeta(previous);
    }
    // No groove-section locks: discard per-file groove so browsing auditions raw clips.
    if (previous >= 0 && previous != index && (locks & toolkitLock::AnyGroove) == 0
        && juce::isPositiveAndBelow(previous, (int) clips.size()))
    {
        const auto& prev = clips[(size_t) previous];
        processorRef.clipGrooves.erase(prev.filePath);
    }

    if (previous >= 0 && previous != index
        && juce::isPositiveAndBelow(previous, (int) clips.size()))
    {
        auto& old = clips[(size_t) previous];
        if (old.notesResident && !old.notes.empty())
        {
            noteCache.store(old);
            old.notes.clear();
            old.notes.shrink_to_fit();
            old.notesResident = false;
        }
    }

    browseDirection = index >= previous ? 1 : -1;
    selectedIdx = index;
    processorRef.requestNoteFlush();

    const auto& listed = clips[(size_t) index];
    transport.setClipBpm(listed.bpm);
    transport.setClipName(listed.name);
    if (const int d = displayForClip(index); d >= 0)
        fileList.setSelectedIndex(d, juce::dontSendNotification);

    loadClipNotesAsync(index, true);
}

void MidiToolkitEditor::finishClipSelection()
{
    if (!juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
        return;

    const int index = selectedIdx;
    const auto locks = processorRef.sectionLocks;
    const auto& clip = clips[(size_t) index];
    const bool haveCarriedScale = havePendingCarriedScale;
    const ClipEdit carriedScale = pendingCarriedScale;

    if ((locks & (toolkitLock::Pitch | toolkitLock::Playback)) == 0)
    {
        // Unlocked: seed octave / range / key / mode from clip analysis.
        auto& e = processorRef.editFor(clip.filePath);
        if (editIsClean(e) || e.root < 0)
        {
            const auto analysis = analyseClipScale(clip);
            e.octave = -1;
            e.octaveRange = analysis.octaveRange;
            e.root = analysis.primaryRoot;
            e.mode = analysis.primaryMode;
        }
        if (haveCarriedScale)
        {
            e.fitScale = carriedScale.fitScale;
            e.mapToRoot = carriedScale.mapToRoot;
            e.root = carriedScale.root;
            e.mode = carriedScale.mode;
        }
    }
    havePendingCarriedScale = false;
    stampLockedSections(true);

    const auto edit = selectedEdit();
    const auto groove = selectedGroove();
    const auto resolved = resolveClip(clip, edit, formSelectionIds());

    // Keep freerun phase continuous across clip changes so browsing mid-playback
    // feels seamless. Wrap into the new clip length; synced mode already follows
    // the host and does not restart.
    {
        const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        const double newLen = juce::jmax(0.25, (double) (resolved.bars * kStepsPerBar) / 4.0 * stretch);
        double beat = processorRef.freerunBeat.load();
        beat = std::fmod(beat, newLen);
        if (beat < 0.0) beat += newLen;
        processorRef.freerunBeat.store(beat);
    }

    transport.setClipBpm(clip.bpm);
    transport.setClipName(clip.name);
    rollEditor.setClip(clip, edit, groove);
    applyTimeStretchFromMultiplier();
    syncEffectsInspector();
    if ((locks & toolkitLock::Playback) != 0 && processorRef.lockAutoTrim)
        effectsInspector.setTrimState(true, true, {});

    if (const int d = displayForClip(index); d >= 0)
        fileList.setSelectedIndex(d, juce::dontSendNotification);

    pushPreviewToProcessor();
    updateMiniPreview();
    persistBrowserSession();
}

void MidiToolkitEditor::stampLockedSections(bool resetUnlockedGroove)
{
    if (!juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
        return;

    const auto locks = processorRef.sectionLocks;
    if (locks == 0)
        return;

    const int index = selectedIdx;
    const auto& clip = clips[(size_t) index];

    if ((locks & (toolkitLock::Pitch | toolkitLock::Playback)) != 0)
    {
        auto& e = processorRef.editFor(clip.filePath);
        if ((locks & toolkitLock::Pitch) != 0)
            applyPitchLock(processorRef.lockedEdit, e);
        if ((locks & toolkitLock::Playback) != 0)
            applyPlaybackLock(processorRef.lockedEdit, e, processorRef.lockAutoTrim, clip);
        refreshEntryMeta(index);
    }

    if ((locks & toolkitLock::AnyGroove) != 0)
    {
        auto& g = processorRef.grooveFor(clip.filePath);
        if (resetUnlockedGroove)
        {
            GrooveParams next;
            applyGrooveSectionLock(processorRef.lockedGroove, next, locks);
            g = next;
        }
        else
        {
            applyGrooveSectionLock(processorRef.lockedGroove, g, locks);
        }
    }
}

void MidiToolkitEditor::captureLocksFromEdit(const ClipEdit& previous, const ClipEdit& next,
                                             const StepClip* clip)
{
    const auto locks = processorRef.sectionLocks;
    if ((locks & (toolkitLock::Pitch | toolkitLock::Playback)) == 0)
        return;

    if ((locks & toolkitLock::Pitch) != 0)
        capturePitchLockTemplate(next, clip, processorRef.lockedEdit);
    else
        applyPitchLock(next, processorRef.lockedEdit);

    if ((locks & toolkitLock::Playback) != 0)
    {
        processorRef.lockedEdit.extendMult = next.extendMult;
        const bool trimChanged = previous.removedBars != next.removedBars
                              || previous.legacyTrimLead != next.legacyTrimLead
                              || previous.legacyTrimTail != next.legacyTrimTail;
        if (trimChanged)
            processorRef.lockAutoTrim = next.hasTrim();
    }
}

void MidiToolkitEditor::refreshEntryMeta(int index)
{
    if (!juce::isPositiveAndBelow(index, (int) clips.size()))
        return;
    const int displayIdx = displayForClip(index);
    if (displayIdx < 0)
        return;

    const auto& clip = clips[(size_t) index];
    // Only show the edited dot for locked (persisted) pitch edits.
    const bool edited = (processorRef.sectionLocks & toolkitLock::Pitch) != 0
                            && !editIsClean(processorRef.editFor(clip.filePath));
    fileList.updateEntry(displayIdx,
                         entryForClip(clip, juce::File(clip.filePath), edited,
                                      processorRef.isStarred(clip.filePath)));
}

const std::set<int>* MidiToolkitEditor::formSelectionIds() const
{
    const auto& e = effectsInspector.getEdit();
    if (!e.selectedNotesOnly || !rollEditor.hasSelection())
        return nullptr;
    return &rollEditor.selectedNoteIds();
}

MidiClip MidiToolkitEditor::buildRenderedClip() const
{
    // Non-destructive pipeline: resolveClip → applyGroove → velocities.
    // bpmMultiplier stretches timing so /2 exports half-time MIDI at session tempo.
    MidiClip out;
    const auto* clip = hasRecording ? &recordedClip : selectedClip();
    if (clip == nullptr)
        return out;

    const auto edit = hasRecording ? effectsInspector.getEdit() : selectedEdit();
    const auto groove = hasRecording ? effectsInspector.getGroove() : selectedGroove();
    const auto resolved = resolveClip(*clip, edit, formSelectionIds());
    auto notes = applyGroove(resolved.notes, groove, clip->complexity,
                             clip->kind, clip->root, edit.mode);
    // Complexity morph can invent ±1/±2 ornaments — keep Fit to Scale honest.
    if (edit.fitScale && edit.root >= 0)
        refitNotesToScale(notes, edit.root, edit.mode);
    const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());

    out.name = clip->name;
    out.bpm = clip->bpm;
    out.lengthBeats = (double) (resolved.bars * kStepsPerBar) / 4.0 * stretch;
    for (const auto& n : notes)
    {
        NoteEvent ev;
        ev.noteNumber = juce::jlimit(0, 127, n.pitch);
        ev.velocity = juce::jlimit(1, 127, (int) std::lround(effectiveVelocity(n, groove) * 127.0));
        ev.startBeat = n.start / 4.0 * stretch;
        ev.lengthBeats = juce::jmax(0.05, n.len / 4.0 * stretch);
        ev.channel = juce::jlimit(1, 16, n.channel);
        out.notes.push_back(ev);
    }

    auto sustain = buildSustainPedalAutomation(notes, out.lengthBeats / juce::jmax(0.25, stretch), groove);
    if (!sustain.empty())
    {
        // Automation is stored in unstretched beat time matching note events after stretch.
        for (auto& p : sustain)
            p.beat *= stretch;
        out.automation[64] = std::move(sustain);
    }
    return out;
}

void MidiToolkitEditor::pushPreviewToProcessor()
{
    const auto preview = buildRenderedClip();
    // Soft update while tweaking Toolkit params so held notes aren't choked.
    processorRef.setPreviewState(preview, !preview.notes.empty(), previewMuted, false, true);
    pushLiveFxToProcessor();
}

void MidiToolkitEditor::pushLiveFxToProcessor()
{
    // The inspector is what the user is hearing. Per-file storage can be empty
    // in passthrough when no clip is selected, which left the stream dry.
    LiveFxState state;
    state.edit = effectsInspector.getEdit();
    state.groove = effectsInspector.getGroove();
    state.bpmMultiplier = effectsInspector.getBpmMultiplier();
    state.recording = processorRef.isRecording();
    if (const auto* clip = selectedClip())
    {
        state.sourceRoot = clip->root;
        state.sourceOctave = clipReferenceOctave(*clip);
    }
    else
    {
        state.sourceRoot = state.edit.root >= 0 ? state.edit.root : 0;
        state.sourceOctave = 4;
    }
    processorRef.setLiveFxState(state);
}

void MidiToolkitEditor::applyPassthroughUi()
{
    const bool pass = processorRef.passthrough;
    transport.setPassthrough(pass);
    // Instrument: audio through follows the UI flag. MIDI FX: audio stays through.
    processorRef.setPassthroughEnabled(pass);
    effectsInspector.setLiveStreamMode(pass);
    // Generators live in the left pane; keep them available with or without Passthrough.
    effectsInspector.setGeneratorsEnabled(true);

    fileList.setAlpha(1.0f);
    miniRoll.setAlpha(1.0f);
    previewHeader.setAlpha(1.0f);
    rollEditor.setAlpha(1.0f);
    fileList.setEnabled(true);
    rollEditor.setEnabled(true);
    // Passthrough accepts MIDI in; generate-only does not (except while recording).
    const bool liveInput = pass || recording;
    rollEditor.setInputMode(liveInput, recording && !pass ? "Live MIDI" : "Live MIDI through the Toolkit");
    if (pass)
    {
        // Effects-only: roll is hidden; keep live take ready if recording resumes.
        rollEditor.setEmptyHint({});
        if (hasRecording && !recordedClip.notes.empty())
            rollEditor.setClip(recordedClip, effectsInspector.getEdit(), effectsInspector.getGroove());
        else
            rollEditor.clearClip();
    }
    else
    {
        rollEditor.setEmptyHint("Select a MIDI file to edit it here.");
        if (const auto* clip = selectedClip())
            rollEditor.setClip(*clip, selectedEdit(), selectedGroove());
    }
    previewHeader.setEnabled(true);
    passthroughVeil.setVisible(false);
    applyLayoutState();
    repaint();
}

void MidiToolkitEditor::togglePassthrough()
{
    if (recording)
        return;
    processorRef.passthrough = !processorRef.passthrough;
    if (processorRef.passthrough)
    {
        // Effects-only layout: Effects must be open; piano roll closes.
        processorRef.effectsOpen = true;
        processorRef.editorOpen = false;
        processorRef.previewOpen = true;
    }
    persistBrowserSession();
    applyPassthroughUi();
    pushLiveFxToProcessor();
}

void MidiToolkitEditor::toggleRecord()
{
    if (!recording)
    {
        recordFromThrough = processorRef.passthrough;
        if (processorRef.passthrough)
        {
            processorRef.passthrough = false;
            processorRef.setPassthroughEnabled(false);
            transport.setPassthrough(false);
        }
        recording = true;
        hasRecording = true;
        lastGeneratedPart = -1;
        processorRef.editorOpen = true;
        transport.setEditorOpen(true);
        processorRef.setRecordMonitorFx(processorRef.effectsOpen);
        processorRef.setRecordingEnabled(true);
        transport.setRecording(true);
        recordedClip = {};
        recordedClip.name = "Recording";
        recordedClip.filePath = "recording://take";
        recordedClip.bars = 1;
        recordedClip.notesResident = true;
        applyLayoutState();
        rollEditor.setInputMode(true, recordFromThrough ? "Live MIDI through the Toolkit" : "Live MIDI");
        rollEditor.beginRecording(recordedClip);
        return;
    }

    recording = false;
    processorRef.setRecordingEnabled(false);
    transport.setRecording(false);
    finishRecording(processorRef.recordSnapshot());
    rollEditor.setInputMode(processorRef.passthrough, "Live MIDI through the Toolkit");
    if (recordFromThrough)
    {
        recordFromThrough = false;
        processorRef.passthrough = true;
        persistBrowserSession();
        applyPassthroughUi();
        pushLiveFxToProcessor();
        return;
    }
    applyLayoutState();
}

void MidiToolkitEditor::refreshRecordingRoll(const RecordingSnapshot& snap)
{
    const double bpm = processorRef.hostBpm.load();
    recordedClip = stepClipFromRecording(snap.notes, snap.lengthBeats, bpm > 0.0 ? bpm : processorRef.freeBpm.load());
    rollEditor.setRecordingNotes(recordedClip, snap.lengthBeats * 4.0);
}

void MidiToolkitEditor::finishRecording(const RecordingSnapshot& snap)
{
    const double bpm = processorRef.hostBpm.load();
    recordedClip = stepClipFromRecording(snap.notes, snap.lengthBeats, bpm > 0.0 ? bpm : processorRef.freeBpm.load());
    hasRecording = true;
    const double loopEndSteps = loopEndBeatForNotes(snap.notes, 4) * 4.0;
    rollEditor.endRecording(recordedClip, effectsInspector.getEdit(), effectsInspector.getGroove(), loopEndSteps);
    pushPreviewToProcessor();
    updateMiniPreview();
}

void MidiToolkitEditor::startDragExport()
{
    const auto* clip = hasRecording ? &recordedClip : selectedClip();
    if (clip == nullptr)
        return;
    const auto rendered = buildRenderedClip();
    if (rendered.notes.empty())
        return;

    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("MidiToolkit Drag");
    dir.createDirectory();
    const auto file = dir.getChildFile(
        juce::File::createLegalFileName(clip->name + " (edited).mid"));
    file.deleteFile();
    if (!writeMidiFile(rendered, file, rendered.bpm))
        return;

    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        juce::StringArray(file.getFullPathName()), false, &transport);
}

void MidiToolkitEditor::startDragOriginalFile(const juce::File& file)
{
    if (!file.getFullPathName().isNotEmpty() || !file.existsAsFile())
        return;
    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        juce::StringArray(file.getFullPathName()), true, &fileList);
}

void MidiToolkitEditor::copyFileToFolder(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    auto chooser = std::make_shared<juce::FileChooser>("Copy to Folder",
                                                       rootDir.isDirectory() ? rootDir
                                                           : juce::File::getSpecialLocation(
                                                                 juce::File::userDocumentsDirectory),
                                                       "");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [chooser, file](const juce::FileChooser& fc)
        {
            const auto destDir = fc.getResult();
            if (!destDir.isDirectory())
                return;
            const auto dest = destDir.getChildFile(file.getFileName());
            if (dest.existsAsFile())
                dest.deleteFile();
            file.copyFileTo(dest);
        });
}

void MidiToolkitEditor::copyStarredToFolder()
{
    auto chooser = std::make_shared<juce::FileChooser>("Copy All Starred to Folder",
                                                       rootDir.isDirectory() ? rootDir
                                                           : juce::File::getSpecialLocation(
                                                                 juce::File::userDocumentsDirectory),
                                                       "");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto destDir = fc.getResult();
            if (!destDir.isDirectory())
                return;
            for (const auto& path : processorRef.starredFiles)
            {
                const juce::File src(path);
                if (!src.existsAsFile())
                    continue;
                const auto dest = destDir.getChildFile(src.getFileName());
                if (dest.existsAsFile())
                    dest.deleteFile();
                src.copyFileTo(dest);
            }
        });
}

void MidiToolkitEditor::copyRenderedClipToFolder()
{
    const auto* clip = selectedClip();
    if (clip == nullptr)
        return;
    const auto rendered = buildRenderedClip();
    if (rendered.notes.empty())
        return;

    auto chooser = std::make_shared<juce::FileChooser>("Copy Edited Clip to Folder",
                                                       rootDir.isDirectory() ? rootDir
                                                           : juce::File::getSpecialLocation(
                                                                 juce::File::userDocumentsDirectory),
                                                       "");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser, rendered, name = clip->name](const juce::FileChooser& fc)
        {
            const auto destDir = fc.getResult();
            if (!destDir.isDirectory())
                return;
            const auto dest = destDir.getChildFile(
                juce::File::createLegalFileName(name + " (edited).mid"));
            if (dest.existsAsFile())
                dest.deleteFile();
            writeMidiFile(rendered, dest, rendered.bpm);
        });
}

void MidiToolkitEditor::updateMiniPreview()
{
    if (hasRecording)
    {
        const auto edit = effectsInspector.getEdit();
        const auto groove = effectsInspector.getGroove();
        const auto resolved = resolveClip(recordedClip, edit, formSelectionIds());
        const int rootPc = edit.root >= 0 ? edit.root : 0;
        auto grooved = applyGroove(resolved.notes, groove, recordedClip.complexity,
                                   recordedClip.kind, recordedClip.root, edit.mode);
        if (edit.fitScale && edit.root >= 0)
            refitNotesToScale(grooved, edit.root, edit.mode);
        // Frame on resolved notes so invert / octave / delete refit the strip.
        miniRoll.setNotes(grooved, resolved.bars, groove, rootPc, grooved,
                          edit.mode, 4, edit.noteFilterMask);
        previewHeader.repaint();
        transport.setHasClip(!recordedClip.notes.empty());
        transport.setClipName(recordedClip.name);
        return;
    }

    const auto* clip = selectedClip();
    if (clip == nullptr)
    {
        miniRoll.setNotes({}, 4, GrooveParams{}, 0, {}, Mode::Ionian, 4, 0x0FFF);
        previewHeader.repaint();
        transport.setHasClip(false);
        transport.setClipName({});
        return;
    }

    transport.setHasClip(true);
    transport.setClipName(clip->name);
    const auto edit = selectedEdit();
    const auto groove = selectedGroove();
    const auto resolved = resolveClip(*clip, edit, formSelectionIds());
    const int rootPc = edit.root >= 0 ? edit.root : (clip->root >= 0 ? clip->root : 0);
    miniRoll.setNoteColour(ds::primary()); // Levels: primary notes
    auto grooved = applyGroove(resolved.notes, groove, clip->complexity,
                               clip->kind, clip->root, edit.mode);
    if (edit.fitScale && edit.root >= 0)
        refitNotesToScale(grooved, edit.root, edit.mode);
    // Fit vertical zoom to the audible notes (±1), not the raw source pitches.
    miniRoll.setNotes(grooved, resolved.bars, groove,
                      rootPc, grooved, edit.mode, 4, edit.noteFilterMask);
    const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
    miniRoll.setTimeStretch(stretch);
    previewHeader.repaint();
}

// ── Preview header ───────────────────────────────────────────────────────────

MidiToolkitEditor::PreviewHeader::PreviewHeader(MidiToolkitEditor& o) : owner(o)
{
    btnVel.setTooltip("Show velocity");
    btnVel.onClick = [this]
    {
        owner.showPreviewVelocity = !owner.showPreviewVelocity;
        owner.miniRoll.setShowVelocity(owner.showPreviewVelocity);
        refresh();
        owner.layoutContent();
    };
    btnTrim.setTooltip("Trim empty bars (T)");
    btnTrim.onClick = [this] { owner.handleTrimClicked(); refresh(); };
    btnMute.setTooltip("Mute preview output");
    btnMute.onClick = [this]
    {
        owner.previewMuted = !owner.previewMuted;
        owner.pushPreviewToProcessor();
        refresh();
    };
    btnDrag.setTooltip("Drag this clip into your DAW");
    btnDrag.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    btnDrag.onDragStart = [this] { owner.startDragExport(); };
    // The preview roll no longer has a velocity lane, so the Vel chip is not shown.
    for (auto* b : { (ChipBtn*) &btnTrim, (ChipBtn*) &btnMute, (ChipBtn*) &btnDrag })
        addAndMakeVisible(*b);
}

void MidiToolkitEditor::PreviewHeader::refresh()
{
    juce::String n, m;
    if (const auto* e = owner.fileList.entryAt(owner.fileList.getSelectedIndex()); e != nullptr && !e->isDirectory)
    {
        n = e->name;
        if (e->rootName.isNotEmpty()) m = e->rootName;
        if (e->noteCount > 0)
            m += (m.isNotEmpty() ? juce::String::fromUTF8(" \xc2\xb7 ") : juce::String())
                 + juce::String(e->noteCount) + (e->noteCount == 1 ? " note" : " notes");
        else if (e->bars > 0)
            m += (m.isNotEmpty() ? juce::String::fromUTF8(" \xc2\xb7 ") : juce::String())
                 + juce::String(e->bars) + (e->bars == 1 ? " bar" : " bars");
    }
    const bool vel = owner.showPreviewVelocity, trim = owner.effectsInspector.isTrimOn(), mute = owner.previewMuted;
    const bool hasClip = n.isNotEmpty();
    if (n != name || m != meta || btnVel.active != vel || btnTrim.active != trim || btnMute.active != mute
        || btnDrag.isEnabled() != hasClip)
    {
        name = n;
        meta = m;
        btnVel.active = vel;
        btnTrim.active = trim;
        btnMute.active = mute;
        btnDrag.setEnabled(hasClip);
        btnTrim.setEnabled(hasClip);
        repaint();
        for (auto* c : getChildren()) c->repaint();
    }
}

void MidiToolkitEditor::PreviewHeader::resized()
{
    auto r = getLocalBounds().reduced(metrics::scaled(8), 0).withTrimmedBottom(1);
    const int h = metrics::scaled(18);
    const int gap = metrics::scaled(4);
    for (auto* b : { (ChipBtn*) &btnDrag, (ChipBtn*) &btnMute, (ChipBtn*) &btnTrim })
    {
        const int w = b->idealWidth();
        b->setBounds(r.removeFromRight(w).withSizeKeepingCentre(w, h));
        r.removeFromRight(gap);
    }
}

void MidiToolkitEditor::PreviewHeader::paint(juce::Graphics& g)
{
    // Levels preview strip: surface100 with hairlines; Inter name + mono meta.
    g.fillAll(ds::surface100());
    g.setColour(ds::line());
    g.fillRect(getLocalBounds().removeFromTop(1));
    g.fillRect(getLocalBounds().removeFromBottom(1));

    auto r = getLocalBounds().reduced(metrics::scaled(8), 0);
    r.setRight(btnTrim.getX() - metrics::scaled(6));
    const auto nameFont = uiFontFixed(12.0f, true);
    const auto metaFont = monoFontFixed(12.0f);
    if (name.isEmpty())
    {
        g.setColour(ds::inkMuted());
        g.setFont(uiFontFixed(12.0f, false));
        g.drawText("Select a MIDI file to preview", r, juce::Justification::centredLeft, true);
        return;
    }
    const int metaW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(metaFont, meta)) + 2;
    const int nameW = juce::jmin(r.getWidth() - metaW - 6,
                                 (int) std::ceil(juce::GlyphArrangement::getStringWidth(nameFont, name)) + 2);
    g.setColour(ds::ink());
    g.setFont(nameFont);
    g.drawText(name, r.removeFromLeft(juce::jmax(0, nameW)), juce::Justification::centredLeft, true);
    r.removeFromLeft(6);
    g.setColour(ds::inkMuted());
    g.setFont(metaFont);
    g.drawText(meta, r, juce::Justification::centredLeft, true);
}

// ── layout ───────────────────────────────────────────────────────────────────

void MidiToolkitEditor::toggleEditorFold()
{
    processorRef.editorOpen = !processorRef.editorOpen;
    // Preview strip and piano roll never share the window.
    processorRef.previewOpen = !processorRef.editorOpen;
    // Piano roll needs the left column; leave effects-only layout.
    if (processorRef.editorOpen)
    {
        processorRef.passthrough = false;
        processorRef.setPassthroughEnabled(false);
    }
    transport.setPassthrough(processorRef.passthrough);
    persistBrowserSession();
    applyLayoutState();
}

void MidiToolkitEditor::toggleEffectsFold()
{
    processorRef.effectsOpen = !processorRef.effectsOpen;
    // Passthrough is effects-only — turning Effects off exits it.
    if (!processorRef.effectsOpen)
    {
        processorRef.passthrough = false;
        processorRef.setPassthroughEnabled(false);
    }
    transport.setPassthrough(processorRef.passthrough);
    transport.setEffectsOpen(processorRef.effectsOpen);
    if (recording)
        processorRef.setRecordMonitorFx(processorRef.effectsOpen);
    persistBrowserSession();
    applyLayoutState();
}

void MidiToolkitEditor::applySavedDefaultView()
{
    if (!processorRef.hasDefaultView)
        return;
    processorRef.editorOpen = processorRef.defEditorOpen;
    processorRef.effectsOpen = processorRef.defEffectsOpen;
    processorRef.previewOpen = processorRef.defPreviewOpen;
    processorRef.sidebarCollapsed = processorRef.defSidebarCollapsed;
    processorRef.passthrough = processorRef.defPassthrough;
    processorRef.columnVisibility = processorRef.defColumns;
    processorRef.nameColumnWidth = processorRef.defNameColumnWidth;
    tweaks().size.store(juce::jlimit(0, kNumContentSizes - 1, processorRef.defTweakSize));
    tweaks().textScalePct.store(juce::jlimit(60, 150, processorRef.defTweakTextScalePct));
    tweaks().density.store(juce::jlimit(0, 1, processorRef.defTweakDensity));
    tweaks().appearance.store(juce::jlimit(0, kNumAppearances - 1, processorRef.defTweakAppearance));
}

void MidiToolkitEditor::saveCurrentViewAsDefault()
{
    processorRef.hasDefaultView = true;
    processorRef.defEditorOpen = processorRef.editorOpen;
    processorRef.defEffectsOpen = processorRef.effectsOpen;
    processorRef.defPreviewOpen = processorRef.previewOpen;
    processorRef.defSidebarCollapsed = sidebar.isCollapsed();
    processorRef.defPassthrough = processorRef.passthrough;
    processorRef.defColumns = fileList.getColumnVisibility();
    processorRef.defNameColumnWidth = fileList.getNameColumnWidth();
    processorRef.defWindowW = getWidth();
    processorRef.defWindowH = getHeight();
    processorRef.defTweakSize = tweaks().size.load();
    processorRef.defTweakTextScalePct = tweaks().textScalePct.load();
    processorRef.defTweakDensity = tweaks().density.load();
    processorRef.defTweakAppearance = tweaks().appearance.load();
    persistBrowserSession();
    flushBrowserSession();
}

int MidiToolkitEditor::passthroughDefaultHeight() const
{
    const int fold = metrics::transportH() + effectsInspector.defaultFoldHeight();
    const int withRoll = processorRef.editorOpen
                             ? juce::jmax(fold, metrics::transportH() + metrics::scaled(520))
                             : fold;
    auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = displays.getDisplayForRect(getScreenBounds());
    if (display == nullptr)
        display = displays.getPrimaryDisplay();
    const int limit = display != nullptr ? juce::jmax(360, display->userArea.getHeight() - 48) : 900;
    return juce::jmin(withRoll, limit);
}

int MidiToolkitEditor::passthroughWindowHeight() const
{
    // Default screenshot fold. User may have dragged the window taller.
    const int def = passthroughDefaultHeight();
    if (userPassthroughH > def)
        return userPassthroughH;
    return def;
}

int MidiToolkitEditor::passthroughMinHeight() const
{
    return metrics::transportH() + metrics::scaled(200);
}

int MidiToolkitEditor::passthroughMaxHeight() const
{
    auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = displays.getDisplayForRect(getScreenBounds());
    if (display == nullptr)
        display = displays.getPrimaryDisplay();
    return display != nullptr ? juce::jmax(360, display->userArea.getHeight() - 48) : 900;
}

int MidiToolkitEditor::passthroughWindowWidth() const
{
    // Effects-only: snap to the Effects column. Otherwise fit every open column.
    if (processorRef.passthrough && processorRef.effectsOpen && !recording)
        return metrics::effectsPaneWidth();

    if (leftPaneTab == 1)
    {
        return metrics::effectsPaneWidth()
             + (processorRef.editorOpen ? metrics::editorPaneWidth() : 0)
             + (processorRef.effectsOpen ? metrics::effectsPaneWidth() : 0);
    }

    const int side = sidebar.isCollapsed() ? metrics::sidebarRailWidth()
                                           : metrics::sidebarExpandedWidth();
    const int browser = juce::jmax(browserContentWidth(), metrics::scaled(260));
    return side + browser
         + (processorRef.editorOpen ? metrics::editorPaneWidth() : 0)
         + (processorRef.effectsOpen ? metrics::effectsPaneWidth() : 0);
}

MidiToolkitEditor::LeftPaneTabs::LeftPaneTabs()
{
    auto prep = [](IconBtn& b)
    {
        b.ghost = false;
        b.iconScale = 1.0f;
        b.setWantsKeyboardFocus(true);
    };

    tabToggle.setComponentID("btnPaneTabToggle");
    tabToggle.setTooltip("Switch between Browser and Generator");
    tabToggle.onChange = [this](int i) { if (onTab) onTab(i); };
    addAndMakeVisible(tabToggle);

    prep(btnGrid);
    btnGrid.setComponentID("btnGridView");
    btnGrid.setTooltip("Grid: files as clips in columns by kind");
    btnGrid.onClick = [this] { if (onToggleGrid) onToggleGrid(); };
    addAndMakeVisible(btnGrid);

    prep(btnPreview);
    btnPreview.setComponentID("btnPreview");
    btnPreview.setTooltip("Show preview under the browser or generator");
    btnPreview.onClick = [this] { if (onTogglePreview) onTogglePreview(); };
    addAndMakeVisible(btnPreview);

    setTab(0);
}

void MidiToolkitEditor::LeftPaneTabs::setTab(int t)
{
    tab = juce::jlimit(0, 1, t);
    tabToggle.setIndex(tab);
    btnGrid.setVisible(tab == 0);
    // Preview stays available in Generator-only mode.
    btnPreview.setVisible(true);
    resized();
    repaint();
}

void MidiToolkitEditor::LeftPaneTabs::setPreviewOpen(bool on)
{
    btnPreview.active = on;
    btnPreview.repaint();
}

void MidiToolkitEditor::LeftPaneTabs::setGrid(bool on)
{
    btnGrid.active = on;
    btnGrid.repaint();
}

void MidiToolkitEditor::LeftPaneTabs::resized()
{
    auto row = getLocalBounds().reduced(metrics::scaled(8), 0);
    const int ctl = metrics::chromeIconButton();
    const int gap = metrics::scaled(4);
    const int toggleH = juce::jmin(ctl, row.getHeight() - metrics::scaled(4));
    const int toggleW = tabToggle.idealWidth();
    tabToggle.setBounds(row.removeFromLeft(toggleW)
                            .withSizeKeepingCentre(toggleW, toggleH));

    btnPreview.setBounds(row.removeFromRight(ctl).withSizeKeepingCentre(ctl, ctl));
    row.removeFromRight(gap);

    if (btnGrid.isVisible())
        btnGrid.setBounds(row.removeFromRight(ctl).withSizeKeepingCentre(ctl, ctl));
    else
        btnGrid.setBounds({});
}

void MidiToolkitEditor::LeftPaneTabs::paint(juce::Graphics& g)
{
    g.fillAll(ds::surface300());
    g.setColour(ds::line());
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
}

void MidiToolkitEditor::setLeftPaneTab(int tab)
{
    leftPaneTab = juce::jlimit(0, 1, tab);
    leftPaneTabs.setTab(leftPaneTab);
    transport.setTransportControlsVisible(leftPaneTab == 0);
    transport.setCompactChrome(leftPaneTab == 1
                               && !processorRef.editorOpen
                               && !processorRef.effectsOpen);
    applyLayoutState();
}

void MidiToolkitEditor::syncPassthroughWindowHeight()
{
    if (!processorRef.passthrough || getHeight() <= 0)
        return;
    const int w = passthroughWindowWidth();
    const int minH = passthroughMinHeight();
    const int maxH = passthroughMaxHeight();
    setResizeLimits(w, minH, w, maxH);
    // Opening sections never grows the window — only width / clamp changes.
    int h = getHeight();
    if (h < minH)
        h = passthroughDefaultHeight();
    h = juce::jlimit(minH, maxH, h);
    if (std::abs(getHeight() - h) > 1 || getWidth() != w)
        setSize(w, h);
    if (getHeight() > passthroughDefaultHeight())
        userPassthroughH = getHeight();
}

void MidiToolkitEditor::updateWindowLimits()
{
    const bool pass = processorRef.passthrough;
    const bool rec = recording;
    const int browserW = browserContentWidth();
    const int recordW = metrics::editorPaneWidth()
                      + (processorRef.effectsOpen ? metrics::effectsPaneWidth() : 0);
    int maxW = pass ? passthroughWindowWidth()
                    : rec ? recordW
                          : visibleWorkspaceWidth(sidebar.isCollapsed(),
                                                  processorRef.editorOpen,
                                                  processorRef.effectsOpen,
                                                  browserW,
                                                  leftPaneTab == 1);
    int minW = maxW;
    // After the first layout, don't raise the minimum above the current window.
    // UI size then reflows inside that window instead of pushing columns off-screen.
    if (layoutTracked && getWidth() > 0 && getWidth() < minW)
        minW = getWidth();
    if (minW > maxW)
        minW = maxW;
    if (pass)
    {
        setResizeLimits(minW, passthroughMinHeight(), maxW, passthroughMaxHeight());
        return;
    }
    const int minH = openedWindowH > 0
        ? openedWindowH
        : metrics::transportH() + metrics::scaled(240);
    setResizeLimits(minW, minH, maxW, 2400);
    if (getWidth() <= 0)
        return;
    const int w = juce::jlimit(minW, maxW, getWidth());
    const int h = juce::jmax(getHeight(), minH);
    if (std::abs(getWidth() - w) > 1 || getHeight() < minH)
        setSize(w, h);
}

void MidiToolkitEditor::applyLayoutState()
{
    const bool edOpen = processorRef.editorOpen;
    const bool fxOpen = processorRef.effectsOpen;
    const bool side = sidebar.isCollapsed();
    const bool pass = processorRef.passthrough;
    transport.setEditorOpen(edOpen);
    transport.setEffectsOpen(fxOpen);
    transport.setCompactChrome(leftPaneTab == 1 && !edOpen && !fxOpen);
    if (getHeight() > 0)
        lastWindowH = getHeight();

    updateWindowLimits();

    const int browserContentW = browserContentWidth();
    const int recordW = metrics::editorPaneWidth() + (fxOpen ? metrics::effectsPaneWidth() : 0);
    const bool rec = recording;
    const int maxW = processorRef.passthrough
        ? passthroughWindowWidth()
        : rec ? recordW
              : visibleWorkspaceWidth(side, edOpen, fxOpen, browserContentW, leftPaneTab == 1);
    const int minW = processorRef.passthrough
        ? passthroughWindowWidth()
        : maxW;
    const bool contentChanged = browserContentW != trackedBrowserContentW;
    trackedBrowserContentW = browserContentW;
    int targetW = getWidth() > 0 ? getWidth() : minW;

    int targetH = juce::jmax(metrics::transportH() + metrics::scaled(240), lastWindowH);
    if (pass && !trackedPassthrough)
    {
        savedNonPassW = juce::jmax(targetW, visibleWorkspaceWidth(side, edOpen, fxOpen, browserContentW, leftPaneTab == 1));
        savedNonPassH = getHeight() > 0 ? getHeight() : lastWindowH;
        targetW = passthroughWindowWidth();
        targetH = passthroughWindowHeight();
    }
    else if (!pass && trackedPassthrough)
    {
        targetW = juce::jmax(minW, savedNonPassW > 0 ? savedNonPassW : targetW);
        targetH = juce::jmax(metrics::transportH() + metrics::scaled(240),
                             savedNonPassH > 0 ? savedNonPassH : lastWindowH);
    }
    else if (pass)
    {
        // Always snap width to the open columns — no empty strip past Effects.
        targetW = passthroughWindowWidth();
        targetH = getHeight() > 0 ? getHeight() : passthroughWindowHeight();
    }
    else if (rec && !trackedRecording)
    {
        const int fullW = visibleWorkspaceWidth(side, edOpen, fxOpen, browserContentW, leftPaneTab == 1);
        savedNonRecordW = juce::jmax(targetW, fullW);
        targetW = recordW;
    }
    else if (!rec && trackedRecording)
    {
        targetW = juce::jmax(minW, savedNonRecordW > 0 ? savedNonRecordW : targetW);
    }
    else if (!pass && layoutTracked)
    {
        if (edOpen != trackedEditor)
            targetW += (edOpen ? 1 : -1) * metrics::editorPaneWidth();
        if (fxOpen != trackedEffects)
            targetW += (fxOpen ? 1 : -1) * metrics::effectsPaneWidth();
        if (side != trackedSidebar)
        {
            const int delta = metrics::sidebarExpandedWidth() - metrics::sidebarRailWidth();
            targetW += side ? -delta : delta;
        }
        if (leftPaneTab != trackedLeftPaneTab)
            targetW = maxW;
    }
    if (!pass && openedWindowH > 0)
        targetH = juce::jmax(targetH, openedWindowH);

    const bool passChanged = pass != trackedPassthrough;
    const bool recordChanged = rec != trackedRecording;
    const bool leftTabChanged = leftPaneTab != trackedLeftPaneTab;
    const bool structureChanged = !layoutTracked
        || edOpen != trackedEditor
        || fxOpen != trackedEffects
        || side != trackedSidebar
        || passChanged
        || recordChanged
        || leftTabChanged
        || contentChanged;
    // Snap to the last open column when the column set changes — no empty strip past Effects.
    const bool columnsChanged = !layoutTracked
        || edOpen != trackedEditor
        || fxOpen != trackedEffects
        || side != trackedSidebar
        || passChanged
        || recordChanged
        || leftTabChanged
        || pass;
    trackedEditor = edOpen;
    trackedEffects = fxOpen;
    trackedSidebar = side;
    trackedPassthrough = pass;
    trackedRecording = rec;
    trackedLeftPaneTab = leftPaneTab;
    layoutTracked = true;
    juce::ignoreUnused(structureChanged, columnsChanged);
    // Always snap to the last open column — no empty strip past browser / roll / Effects.
    targetW = maxW;

    const int currentW = getWidth() > 0 ? getWidth() : targetW;

    layoutAnimFromW = currentW;
    layoutTargetW = targetW;
    layoutAnimStartMs = juce::Time::getMillisecondCounterHiRes();
    // Passthrough height tracks open sections, so snap instead of animating width only.
    if (pass || passChanged || recordChanged || std::abs(layoutAnimFromW - layoutTargetW) < 2)
    {
        layoutTargetW = 0;
        if (std::abs(getWidth() - targetW) > 1 || std::abs(getHeight() - targetH) > 1)
            setSize(targetW, targetH);
        else
            resized();
    }
}

void MidiToolkitEditor::resized()
{
    content.setTransform({});
    content.setBounds(getLocalBounds());
    if (processorRef.passthrough && getHeight() > passthroughDefaultHeight() + 2)
        userPassthroughH = getHeight();
    layoutContent();
}

int MidiToolkitEditor::browserContentWidth() const
{
    return gridView ? KindGridView::idealWidth() : fileList.idealContentWidth();
}

void MidiToolkitEditor::refreshBrowserBar()
{
    browserBar.setGrid(gridView);
    browserBar.setSubfolders(sidebar.getIncludeSubdirs());
    leftPaneTabs.setGrid(gridView);
    leftPaneTabs.setPreviewOpen(processorRef.previewOpen && !processorRef.editorOpen);
    kindGrid.setKindMask(sidebar.getCriteria().kindMask);
    previewHeader.refresh();
    int files = 0;
    for (int i = 0; i < fileList.getNumEntries(); ++i)
        if (const auto* e = fileList.entryAt(i); e != nullptr && !e->isDirectory)
            ++files;
    const int barH = browserBar.idealHeight();
    browserBar.setTitle(fileList.getFolderName());
    browserBar.setCount(files, files);
    browserBar.setFiltersActive(sidebar.isFilterOpen(), sidebar.activeFilterCount(),
                                sidebar.describeActiveFilters());
    filterPopover.setCountText(juce::String(files) + (files == 1 ? " file" : " files"));
    // Filter summary strip changes BrowserToolbar height — reflow only when it does.
    if (browserBar.isVisible() && browserBar.idealHeight() != barH)
        layoutContent();
}

void MidiToolkitEditor::layoutContent()
{
    auto r = content.getLocalBounds();
    const bool edOpen = processorRef.editorOpen;
    const bool fxOpen = processorRef.effectsOpen;
    // Preview and piano roll are mutually exclusive.
    const bool previewOpen = !edOpen && processorRef.previewOpen;
    const bool pass = processorRef.passthrough;

    transport.setBounds(r.removeFromTop(metrics::transportH()));
    transport.setTransportControlsVisible(leftPaneTab == 0);

    // Passthrough + Effects: only the effects column is visible.
    if (pass && fxOpen && !recording)
    {
        browserBar.setVisible(false);
        filterPopover.setVisible(false);
        kindGrid.setVisible(false);
        sidebar.setVisible(false);
        fileList.setVisible(false);
        previewHeader.setVisible(false);
        miniRoll.setVisible(false);
        browserRollSplitter.setVisible(false);
        passthroughVeil.setVisible(false);
        leftPaneTabs.setVisible(false);
        generatorViewport.setVisible(false);
        rollEditor.setVisible(false);
        effectsInspector.setVisible(true);
        effectsInspector.setBounds(r);
        return;
    }

    if (recording)
    {
        browserBar.setVisible(false);
        filterPopover.setVisible(false);
        kindGrid.setVisible(false);
        sidebar.setVisible(false);
        fileList.setVisible(false);
        previewHeader.setVisible(false);
        miniRoll.setVisible(false);
        browserRollSplitter.setVisible(false);
        passthroughVeil.setVisible(false);
        leftPaneTabs.setVisible(false);
        generatorViewport.setVisible(false);
        rollEditor.setVisible(true);
        effectsInspector.setVisible(fxOpen);
        auto row = r;
        if (fxOpen)
            effectsInspector.setBounds(row.removeFromRight(metrics::effectsPaneWidth()));
        rollEditor.setBounds(row);
        return;
    }

    const bool showFiles = leftPaneTab == 0;
    const bool showFx = fxOpen;
    juce::ignoreUnused(pass);
    sidebar.setVisible(showFiles);
    browserBar.setVisible(showFiles);
    fileList.setVisible(showFiles);
    // Levels: clip title strip above the mini preview when the full roll is closed.
    previewHeader.setVisible(previewOpen && !edOpen);
    rollEditor.setVisible(edOpen);
    effectsInspector.setVisible(showFx);
    leftPaneTabs.setVisible(true);
    generatorViewport.setVisible(!showFiles);

    auto row = r;
    const int colW = metrics::effectsPaneWidth();
    int sideW = 0;
    int leftColW = 0;
    int fxW = showFx ? colW : 0;
    const int browserMin = metrics::scaled(180);
    const int browserMax = juce::jmax(browserMin, browserContentWidth());
    const int rollMin = metrics::openRollMinWidth();
    int editorW = 0;
    int browserW = 0;
    if (showFiles)
    {
        sideW = sidebar.idealWidth();
        if (sideW > row.getWidth() - metrics::scaled(120))
            sideW = juce::jmax(metrics::sidebarRailWidth(), row.getWidth() / 4);
        int remain = row.getWidth() - sideW;
        if (edOpen)
        {
            if (fxW + rollMin + browserMin > remain)
                fxW = juce::jmax(metrics::scaled(160), remain - rollMin - browserMin);
            const int span = juce::jmax(0, remain - fxW);
            const int maxBrowser = juce::jmin(browserMax, juce::jmax(browserMin, span - rollMin));
            if (browserPaneW > 0)
            {
                browserW = juce::jlimit(browserMin, maxBrowser, browserPaneW);
            }
            else
            {
                editorW = juce::jmin(metrics::editorPaneWidth(), juce::jmax(0, span - browserMin));
                browserW = juce::jmin(browserMax, juce::jmax(0, span - editorW));
            }
            editorW = juce::jmax(0, span - browserW);
        }
        else
        {
            if (fxW + browserMin > remain)
                fxW = juce::jmax(0, remain - browserMin);
            browserW = juce::jmin(browserMax, juce::jmax(0, remain - fxW));
        }
    }
    else
    {
        // Generator and Effects always share one width.
        int shared = colW;
        if (edOpen)
        {
            const int pair = showFx ? shared * 2 : shared;
            if (pair + rollMin > row.getWidth())
                shared = showFx ? juce::jmax(0, (row.getWidth() - rollMin) / 2)
                                : juce::jmax(0, row.getWidth() - rollMin);
        }
        else if (showFx)
        {
            shared = juce::jmin(colW, row.getWidth() / 2);
        }
        else
        {
            shared = juce::jmin(colW, row.getWidth());
        }
        leftColW = shared;
        fxW = showFx ? shared : 0;
        editorW = juce::jmax(0, row.getWidth() - leftColW - fxW);
    }

    const int paneY = row.getY();
    const int paneH = row.getHeight();
    const int leftTotal = showFiles ? (sideW + browserW) : leftColW;
    auto left = row.removeFromLeft(leftTotal);
    leftPaneTabs.setBounds(left.removeFromTop(metrics::paneHeaderH()));
    int paneX = row.getX();
    juce::Rectangle<int> browserCol;
    if (!showFiles)
    {
        filterPopover.setVisible(false);
        kindGrid.setVisible(false);
        fileList.setVisible(false);

        // Preview under the generator when the piano roll is closed.
        const int headerH = 0; // Levels: no title/Trim/Mute row above the preview (Mute → right-click)
        const int desiredRollH = previewOpen ? juce::jmax(metrics::miniRollH(), metrics::scaled(80)) : 0;
        const int stackH = juce::jmin(headerH + desiredRollH, juce::jmax(0, left.getHeight() / 3));
        const int rollH = juce::jmax(0, stackH - headerH);
        if (rollH > 0)
        {
            miniRoll.setBounds(left.removeFromBottom(rollH));
            miniRoll.setVisible(true);
            miniRoll.setShowVelocity(false);
            if (headerH > 0)
            {
                previewHeader.setBounds(left.removeFromBottom(headerH));
                previewHeader.setVisible(true);
            }
            else
            {
                previewHeader.setBounds({});
            }
        }
        else
        {
            miniRoll.setBounds({});
            miniRoll.setVisible(false);
            previewHeader.setBounds({});
            previewHeader.setVisible(false);
        }

        generatorViewport.setBounds(left);
        generatorBody.setSize(juce::jmax(1, generatorViewport.getMaximumVisibleWidth()),
                              juce::jmax(1, generatorBody.getHeight()));
        effectsInspector.layoutExternalGenerators();
    }
    else
    {
        sidebar.setBounds(left.removeFromLeft(sideW));
        browserCol = left;
        browserBar.setBounds(browserCol.removeFromTop(juce::jmin(browserBar.idealHeight(), browserCol.getHeight())));
    }
    if (edOpen)
    {
        rollEditor.setBounds(paneX, paneY, editorW, paneH);
        const int splitW = 6;
        browserRollSplitter.setBounds(paneX - splitW / 2, paneY, splitW, paneH);
        browserRollSplitter.setVisible(showFiles);
        if (showFiles)
            browserRollSplitter.toFront(false);
        paneX += editorW;
        miniRoll.setBounds({});
        miniRoll.setVisible(false);
    }
    else
    {
        browserRollSplitter.setVisible(false);
    }
    if (showFx)
        effectsInspector.setBounds(paneX, paneY, fxW, paneH);

    // Mini preview when the piano roll is closed (Files tab).
    if (showFiles)
    {
        const int fileListMinH = metrics::listHeaderH() + metrics::listRowH() * 2;
        const int colH = browserCol.getHeight();
        const int softMaxStack = juce::jmax(0, colH - fileListMinH);
        const int headerH = 0; // Levels: no title/Trim/Mute row above the preview (Mute → right-click)
        const int desiredRollH = previewOpen ? juce::jmax(metrics::miniRollH(), metrics::scaled(80)) : 0;
        const int stackH = juce::jmin(headerH + desiredRollH, softMaxStack);
        const int rollH = juce::jmax(0, stackH - headerH);

        if (rollH > 0)
        {
            miniRoll.setBounds(browserCol.removeFromBottom(rollH));
            if (headerH > 0)
                previewHeader.setBounds(browserCol.removeFromBottom(headerH));
            else
                previewHeader.setBounds({});
        }
        else
        {
            miniRoll.setBounds({});
            previewHeader.setBounds({});
        }
        miniRoll.setVisible(rollH > 0);
        miniRoll.setShowVelocity(false);
        previewHeader.setVisible(rollH > 0 && headerH > 0);

        fileList.setBounds(browserCol);
        kindGrid.setBounds(browserCol);
        fileList.setVisible(!gridView);
        kindGrid.setVisible(gridView);
        browserColW = fileList.getWidth();
    }

    if (showFiles && sidebar.isFilterOpen())
    {
        const int w = juce::jmin(metrics::scaled(300), juce::jmax(200, browserBar.getWidth() - metrics::scaled(16)));
        const int top = browserBar.getBottom() - metrics::scaled(4);
        const int maxH = juce::jmax(120, content.getHeight() - top - metrics::scaled(8));
        filterPopover.setBounds(browserBar.getRight() - metrics::scaled(8) - w, top, w,
                                juce::jmin(filterPopover.idealHeight(), maxH));
        filterPopover.setVisible(true);
        filterPopover.toFront(false);
    }
    else
    {
        filterPopover.setVisible(false);
    }

    if (passthroughVeil.isVisible())
    {
        juce::Rectangle<int> veil = fileList.getBounds().getUnion(miniRoll.getBounds());
        if (edOpen)
            veil = veil.getUnion(rollEditor.getBounds());
        passthroughVeil.setBounds(veil);
        passthroughVeil.toFront(false);
    }
}

void MidiToolkitEditor::paint(juce::Graphics& g)
{
    // Flat window fill — matches prototype (no desktop chrome behind content).
    g.fillAll(colours::bg());
}

// ── tweaks ───────────────────────────────────────────────────────────────────

namespace {

/** In-app settings — Output / Design / Info tabs sized to the default FX height. */
class SettingsPanel : public juce::Component
{
public:
    std::function<void()> onChanged;
    std::function<void()> onClose;
    std::function<void()> onSaveDefault;

    static constexpr int kTextPctChoices[] = { 70, 85, 100, 115, 130 };
    static constexpr int kPanelW = 480; // Levels: settings max width
    static constexpr int kHeaderH = 44;
    static constexpr int kTabH = 28;
    static constexpr int kFooterH = 8;

    static bool narrowWidth(int width) { return width < 360; }

    static int heightForDefaultWindow(int windowH)
    {
        return juce::jmax(320, windowH - metrics::transportH());
    }

    SettingsPanel()
    {
        appearancePopup.setItems({ "Auto", "Light", "Dark" },
                                 juce::jlimit(0, 2, tweaks().appearance.load()));
        refreshThemePopup();
        sizePopup.setItems({ "Small", "Medium", "Large" }, tweaks().size.load());
        fontPopup.setItems({ "Inter", "System", "Helvetica Neue", "Avenir Next" },
                           juce::jlimit(0, (int) FontFamily::Count - 1, tweaks().fontFamily.load()));

        juce::StringArray pctLabels;
        int pctIdx = 2;
        const int currentPct = tweaks().textScalePct.load();
        int bestDist = 1000;
        for (int i = 0; i < (int) (sizeof(kTextPctChoices) / sizeof(kTextPctChoices[0])); ++i)
        {
            pctLabels.add(juce::String(kTextPctChoices[i]) + "%");
            const int d = std::abs(kTextPctChoices[i] - currentPct);
            if (d < bestDist) { bestDist = d; pctIdx = i; }
        }
        textPopup.setItems(pctLabels, pctIdx);
        tooltipsSwitch.setToggleState(tweaks().showTooltips.load() != 0, juce::dontSendNotification);
        tooltipsSwitch.setTooltip("Show hover tips on controls");
        alternateRowsSwitch.setToggleState(tweaks().alternateRows.load() != 0, juce::dontSendNotification);
        alternateRowsSwitch.setTooltip("Shade every other file-list row");

        auto wire = [this](fx::FlatPopup& p, std::function<void(int)> apply)
        {
            p.onChange = [this, apply](int idx)
            {
                apply(idx);
                if (onChanged) onChanged();
            };
            addAndMakeVisible(p);
        };
        wire(appearancePopup, [this](int i)
        {
            tweaks().appearance.store(juce::jlimit(0, 2, i));
            refreshThemePopup();
        });
        wire(themePopup, [this](int i)
        {
            const int idx = juce::jlimit(0, kNumThemesPerFamily - 1, i);
            if (settingsThemeFamilyIsDark())
                tweaks().darkTheme.store(idx);
            else
                tweaks().lightTheme.store(idx);
        });
        wire(sizePopup, [](int i) { tweaks().size.store(i); });
        wire(textPopup, [](int i)
        {
            const int n = (int) (sizeof(kTextPctChoices) / sizeof(kTextPctChoices[0]));
            tweaks().textScalePct.store(kTextPctChoices[juce::jlimit(0, n - 1, i)]);
        });
        wire(fontPopup, [](int i) { tweaks().fontFamily.store(i); });

        tooltipsSwitch.onClick = [this]
        {
            tweaks().showTooltips.store(tooltipsSwitch.getToggleState() ? 1 : 0);
            if (onChanged) onChanged();
        };
        addAndMakeVisible(tooltipsSwitch);
        alternateRowsSwitch.onClick = [this]
        {
            tweaks().alternateRows.store(alternateRowsSwitch.getToggleState() ? 1 : 0);
            if (onChanged) onChanged();
        };
        addAndMakeVisible(alternateRowsSwitch);

        // Separation: how far section cards, sidebar, wells and lines sit from the
        // background. 100 % = Levels default; drag up to make layers stand apart.
        separationSlider.setValue(juce::jlimit(50, 300, tweaks().separationPct.load()), juce::dontSendNotification);
        separationSlider.valueText = juce::String(separationSlider.getValue()) + "%";
        separationSlider.setTooltip("How strongly panels, sections and lines stand apart from the background (double-click for 100%)");
        separationSlider.onChange = [this](int v)
        {
            tweaks().separationPct.store(juce::jlimit(50, 300, v));
            separationSlider.valueText = juce::String(v) + "%";
            if (onChanged) onChanged();
        };
        addAndMakeVisible(separationSlider);

        closeBtn.ghost = true;
        closeBtn.iconScale = 0.95f;
        closeBtn.setTooltip("Close");
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible(closeBtn);

        saveDefaultBtn.caption = "Save";
        saveDefaultBtn.setTooltip("Remember this layout, size, and columns as the default view");
        saveDefaultBtn.onClick = [this] { if (onSaveDefault) onSaveDefault(); };
        addAndMakeVisible(saveDefaultBtn);

        setTab(1); // Design
        setSize(kPanelW, contentHeightForTab(tab));
    }

    void refreshThemePopup()
    {
        juce::StringArray names;
        const bool dark = settingsThemeFamilyIsDark();
        const int base = dark ? kNumThemesPerFamily : 0;
        for (int i = 0; i < kNumThemesPerFamily; ++i)
            names.add(kThemes[(size_t) (base + i)].name);
        const int sel = dark ? tweaks().darkTheme.load() : tweaks().lightTheme.load();
        themePopup.setItems(names, juce::jlimit(0, kNumThemesPerFamily - 1, sel));
    }

    int currentTab() const { return tab; }

    static int contentHeightForTab(int t, bool narrow = false)
    {
        // Narrow (< 360 px) stacks label over control, so rows are taller.
        const int header = kHeaderH + kTabH + 8 + kFooterH + 20;
        const int rowH = (narrow ? 56 : 40) + 6;
        if (t == 0) return header + rowH + 8;
        if (t == 1) return header + rowH * 8 + 8; // appearance + theme + separation + size..alt
        return header + 56;
    }

    void setTab(int t)
    {
        tab = juce::jlimit(0, 2, t);
        const bool design = tab == 1;
        const bool output = tab == 0;
        appearancePopup.setVisible(design);
        themePopup.setVisible(design);
        separationSlider.setVisible(design);
        sizePopup.setVisible(design);
        textPopup.setVisible(design);
        fontPopup.setVisible(design);
        tooltipsSwitch.setVisible(design);
        alternateRowsSwitch.setVisible(design);
        saveDefaultBtn.setVisible(output);
        setSize(kPanelW, contentHeightForTab(tab));
        resized();
        repaint();
        if (auto* parent = getParentComponent())
            parent->resized();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto bounds = getLocalBounds().toFloat();
        // Solid panel — no frosted / translucent fill over the plugin.
        g.setColour(ds::surface100());
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(t.controlHairline);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

        auto header = getLocalBounds().removeFromTop(kHeaderH).reduced(18, 0);
        header.removeFromRight(28);
        g.setFont(uiFont(11.0f, true));
        g.setColour(t.valueText);
        g.drawText("SETTINGS", header, juce::Justification::centredLeft, false);

        g.setColour(t.divider);
        g.fillRect(18, kHeaderH, getWidth() - 36, 1);

        auto tabRow = getLocalBounds().withTrimmedTop(kHeaderH).removeFromTop(kTabH).reduced(18, 0);
        const char* tabs[] = { "Output", "Design", "Info" };
        for (int i = 0; i < 3; ++i)
        {
            auto r = tabRow.removeFromLeft(70);
            tabBounds[i] = r;
            g.setFont(uiFont(12.0f, tab == i));
            g.setColour(tab == i ? colours::text() : colours::text2());
            g.drawText(tabs[i], r, juce::Justification::centredLeft, false);
            if (tab == i)
            {
                g.setColour(ds::acc());
                g.fillRect(r.getX(), r.getBottom() - 2, r.getWidth() - 12, 2);
            }
            tabRow.removeFromLeft(8);
        }
        g.setColour(t.divider);
        g.fillRect(18, kHeaderH + kTabH, getWidth() - 36, 1);

        auto body = getLocalBounds().withTrimmedTop(kHeaderH + kTabH + 8).reduced(18, 10);
        const bool narrow = narrowWidth(getWidth());
        const int rowH = narrow ? 56 : 40;
        const int rowGap = 6;

        auto paintRow = [&](const char* label, const char* hint)
        {
            auto row = body.removeFromTop(rowH);
            auto textCol = narrow ? row.removeFromTop(28) : row.withTrimmedRight(150);
            g.setColour(colours::text());
            g.setFont(uiFont(12.0f, false));
            g.drawText(label, textCol.removeFromTop(hint[0] ? 16 : textCol.getHeight()),
                       juce::Justification::centredLeft, false);
            if (hint[0])
            {
                g.setColour(colours::text2());
                g.setFont(uiFont(10.5f, false));
                g.drawText(hint, textCol, juce::Justification::centredLeft, false);
            }
            body.removeFromTop(rowGap);
        };

        if (tab == 0)
        {
            paintRow("Default view", "Open with this layout next time");
        }
        else if (tab == 1)
        {
            paintRow("Appearance", "Auto follows the system light/dark setting");
            paintRow("Theme", "Ten light and ten dark colour pairs");
            paintRow("Separation", "How far panels and sections stand apart");
            paintRow("UI size", "Scales spacing, panels, and controls");
            paintRow("Text size", "Scales type and icons only");
            paintRow("Font", "Interface typeface; values stay mono");
            paintRow("Show tooltips", "");
            paintRow("Alternate rows", "Shade every other file-list row");
        }
        else
        {
            g.setFont(uiFont(15.0f, true));
            g.setColour(colours::text());
            g.drawText(build_info::version(), body.removeFromTop(24),
                       juce::Justification::centredLeft, true);
            body.removeFromTop(6);
            g.setFont(uiFont(12.5f, false));
            g.setColour(colours::text2());
            g.drawText(juce::String(build_info::buildTime()) + "  ·  " + build_info::gitHash(),
                       body.removeFromTop(20), juce::Justification::centredLeft, true);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 3; ++i)
            if (tabBounds[i].contains(e.getPosition()))
            {
                setTab(i);
                return;
            }
    }

    void resized() override
    {
        const int iconBtn = metrics::chromeIconButton();
        closeBtn.setBounds(juce::Rectangle<int>(getWidth() - 18 - iconBtn, 0, iconBtn, kHeaderH)
                               .withSizeKeepingCentre(iconBtn, iconBtn));

        const bool narrow = narrowWidth(getWidth());
        auto body = getLocalBounds().withTrimmedTop(kHeaderH + kTabH + 8).reduced(18, 10);
        const int rowH = narrow ? 56 : 40;
        const int rowGap = 6;
        const int ctrlH = metrics::scaled(26);
        auto placePopup = [&](fx::FlatPopup& p)
        {
            auto row = body.removeFromTop(rowH);
            if (narrow)
                p.setBounds(row.removeFromBottom(ctrlH));
            else
            {
                const int w = juce::jmin(juce::jmax(p.idealWidth(), 140), row.getWidth() / 2);
                p.setBounds(row.removeFromRight(w).withSizeKeepingCentre(w, ctrlH));
            }
            body.removeFromTop(rowGap);
        };

        appearancePopup.setBounds({});
        themePopup.setBounds({});
        separationSlider.setBounds({});
        sizePopup.setBounds({});
        textPopup.setBounds({});
        fontPopup.setBounds({});
        tooltipsSwitch.setBounds({});
        alternateRowsSwitch.setBounds({});
        saveDefaultBtn.setBounds({});

        if (tab == 0)
        {
            auto saveRow = body.removeFromTop(rowH);
            const int saveW = saveDefaultBtn.captionWidth();
            if (narrow)
                saveDefaultBtn.setBounds(saveRow.removeFromBottom(ctrlH).removeFromLeft(saveW));
            else
                saveDefaultBtn.setBounds(saveRow.removeFromRight(saveW).withSizeKeepingCentre(saveW, ctrlH));
        }
        else if (tab == 1)
        {
            placePopup(appearancePopup);
            placePopup(themePopup);
            {
                auto row = body.removeFromTop(rowH);
                if (narrow)
                    separationSlider.setBounds(row.removeFromBottom(ctrlH + 4));
                else
                {
                    const int w = juce::jmin(220, row.getWidth() / 2);
                    separationSlider.setBounds(row.removeFromRight(w).withSizeKeepingCentre(w, ctrlH + 4));
                }
                body.removeFromTop(rowGap);
            }
            placePopup(sizePopup);
            placePopup(textPopup);
            placePopup(fontPopup);
            auto tipRow = body.removeFromTop(rowH);
            const int sw = tooltipsSwitch.idealWidth();
            if (narrow)
                tooltipsSwitch.setBounds(tipRow.removeFromBottom(18).removeFromLeft(sw));
            else
                tooltipsSwitch.setBounds(tipRow.removeFromRight(sw).withSizeKeepingCentre(sw, 18));
            auto altRow = body.removeFromTop(rowH);
            const int aw = alternateRowsSwitch.idealWidth();
            if (narrow)
                alternateRowsSwitch.setBounds(altRow.removeFromBottom(18).removeFromLeft(aw));
            else
                alternateRowsSwitch.setBounds(altRow.removeFromRight(aw).withSizeKeepingCentre(aw, 18));
        }
    }

private:
    int tab = 1;
    juce::Rectangle<int> tabBounds[3];
    fx::FlatPopup appearancePopup, themePopup, sizePopup, textPopup, fontPopup;
    fx::FlatSwitch tooltipsSwitch;
    fx::FlatSwitch alternateRowsSwitch;
    fx::FlatSliderRow separationSlider { "", 50, 300, 100, false };
    IconBtn closeBtn { icons::x, "Close" };
    IconBtn saveDefaultBtn { icons::save, "Save" };
};

} // namespace

void MidiToolkitEditor::showTweaksMenu()
{
    if (auto* existing = content.findChildWithID("settingsOverlay"))
    {
        content.removeChildComponent(existing);
        delete existing;
        return;
    }

    struct Overlay : juce::Component
    {
        SettingsPanel panel;
        juce::Viewport view;
        std::function<void()> onDismiss;

        Overlay()
        {
            view.setViewedComponent(&panel, false);
            view.setScrollBarsShown(true, false);
            view.setScrollBarThickness(10);
            addAndMakeVisible(view);
        }

        void resized() override
        {
            // Fit inside the current window — never grow the plugin to open Settings.
            const int margin = 12;
            const int minW = 220;
            const int minH = 200; // passthrough with sections folded
            const int w = juce::jmin(SettingsPanel::kPanelW,
                                     juce::jmax(minW, getWidth() - margin * 2));
            const int availH = juce::jmax(minH, getHeight() - margin * 2);
            // Height follows the layout the panel will use at this width;
            // capped to the window and scrolled when it doesn't fit.
            const int contentH = SettingsPanel::contentHeightForTab(
                panel.currentTab(), SettingsPanel::narrowWidth(w));
            const int h = juce::jmin(contentH, availH);
            view.setBounds(getLocalBounds().withSizeKeepingCentre(w, h));
            view.setScrollBarsShown(contentH > h, false);
            panel.setSize(w, contentH);
        }

        void paint(juce::Graphics& g) override
        {
            g.setColour(juce::Colours::black.withAlpha(0.18f));
            g.fillAll();
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (!view.getBounds().contains(e.getPosition()) && onDismiss)
                onDismiss();
        }
    };

    auto* overlay = new Overlay();
    overlay->setComponentID("settingsOverlay");
    overlay->panel.onChanged = [this]
    {
        persistBrowserSession();
        lnf.refreshColours();
        sendLookAndFeelChange();
        applyLayoutState();
        resized();
        repaint();
    };
    overlay->onDismiss = [this, overlay]
    {
        content.removeChildComponent(overlay);
        delete overlay;
    };
    overlay->panel.onClose = overlay->onDismiss;
    overlay->panel.onSaveDefault = [this] { saveCurrentViewAsDefault(); };
    overlay->setBounds(content.getLocalBounds());
    content.addAndMakeVisible(overlay);
    overlay->toFront(true);
}

void MidiToolkitEditor::refreshSidebar()
{
    const auto activePath = rootDir.isDirectory() ? rootDir.getFullPathName() : juce::String();
    sidebar.setSavedDirs(processorRef.savedBrowserDirs, activePath);
    sidebar.setSavedSearches(processorRef.savedSearches, activeSavedSearchIdx);
    sidebar.setBrowseMode(browseMode);
    sidebar.setStarredFilter(starredFilter);
    if (browseMode != 0)
        sidebar.setCurrentClipCount(0);
    sidebar.setStarredCount(processorRef.starredFiles.size());
    sidebar.setCurrentFolderStarred(activePath.isNotEmpty()
                                    && processorRef.isStarredFolder(activePath));
}

// ── ticking ──────────────────────────────────────────────────────────────────

bool MidiToolkitEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && sidebar.keyPressed(key))
        return true;

    if (key == juce::KeyPress::spaceKey)
    {
        // Synced: the mac monitor already handed space to the host window.
        // If it didn't (embedded view, or another platform), leave the key
        // unhandled so the host can use it for play/stop.
        if (processorRef.syncToHost.load())
            return false;

        const bool arm = !processorRef.previewArmed.load();
        processorRef.previewArmed.store(arm);
        if (!arm)
            processorRef.freerunBeat.store(0.0);
        return true;
    }

    // E toggles the editor pane; F toggles effects; P toggles passthrough.
    if (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E')
    {
        toggleEditorFold();
        return true;
    }
    if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
    {
        toggleEffectsFold();
        return true;
    }
    if (key.getTextCharacter() == 'p' || key.getTextCharacter() == 'P')
    {
        togglePassthrough();
        return true;
    }

    if (processorRef.passthrough)
        return false;

    if (rollEditor.isVisible() && rollEditor.hasSelection()
        && (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey))
    {
        rollEditor.deleteSelectedNotes();
        return true;
    }

    const bool arrowNav = key == juce::KeyPress::upKey || key == juce::KeyPress::downKey
        || key == juce::KeyPress::pageUpKey || key == juce::KeyPress::pageDownKey
        || key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey
        || key == juce::KeyPress::returnKey;

    if (arrowNav)
    {
        // Sticky pane: arrows stay with the last clicked window.
        if (keyNavTarget == KeyNavTarget::Editor)
        {
            if (rollEditor.isVisible() && rollEditor.hasSelection() && key.getModifiers().isShiftDown())
            {
                if (rollEditor.keyPressed(key))
                    return true;
            }
            // Consume arrows in the editor so they don't cycle focus elsewhere.
            return true;
        }
        if (keyNavTarget == KeyNavTarget::Effects)
            return true;   // effects are mouse-driven; don't steal browser rows

        if (gridView)
        {
            kindGrid.grabKeyboardFocus();
            return kindGrid.keyPressed(key);
        }
        fileList.grabBrowseFocus();
        return fileList.keyPressed(key);
    }

    if (keyNavTarget == KeyNavTarget::Editor && rollEditor.isVisible() && rollEditor.keyPressed(key))
        return true;

    if (keyNavTarget == KeyNavTarget::Browser)
    {
        if (gridView)
        {
            if (!kindGrid.hasKeyboardFocus(true))
                kindGrid.grabKeyboardFocus();
            return kindGrid.keyPressed(key);
        }
        if (!fileList.hasKeyboardFocus(true))
            fileList.grabBrowseFocus();
        return fileList.keyPressed(key);
    }

    return false;
}

void MidiToolkitEditor::claimKeyNav(KeyNavTarget target)
{
    keyNavTarget = target;
    // Hosts often only deliver keys to the focused plugin component. Hold focus
    // on the editor shell and route by sticky target instead of child focus.
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (focused != this && !isParentOf(focused))
            focused->giveAwayKeyboardFocus();
    grabKeyboardFocus();
    if (target == KeyNavTarget::Browser)
    {
        if (gridView)
            kindGrid.grabKeyboardFocus();
        else
            fileList.grabKeyboardFocus();
    }
    else if (target == KeyNavTarget::Editor && rollEditor.isVisible())
        rollEditor.grabKeyboardFocus();
}

void MidiToolkitEditor::syncTooltipHighlight()
{
    if (! tooltips.isVisible() || tooltips.getWidth() <= 0)
    {
        setTooltipHighlightTarget(nullptr);
        return;
    }
    const auto screen = juce::Desktop::getMousePosition();
    auto* hit = getComponentAt(getLocalPoint(nullptr, screen));
    for (auto* c = hit; c != nullptr; c = c->getParentComponent())
        if (dynamic_cast<juce::TooltipClient*>(c) != nullptr)
        {
            setTooltipHighlightTarget(c);
            return;
        }
    setTooltipHighlightTarget(hit);
}

void MidiToolkitEditor::timerCallback()
{
    syncTooltipHighlight();
    refreshBrowserBar();

    if (sessionDirty
        && juce::Time::getMillisecondCounterHiRes() - sessionDirtyAtMs > 400.0)
        flushBrowserSession();

    if (layoutTargetW > 0 && std::abs(getWidth() - layoutTargetW) > 1)
    {
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - layoutAnimStartMs;
        const float t = juce::jlimit(0.0f, 1.0f, (float) (elapsed / kLayoutAnimMs));
        // Ease-out cubic
        const float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        const int w = juce::roundToInt((float) layoutAnimFromW
                                       + e * (float) (layoutTargetW - layoutAnimFromW));
        setSize(w, juce::jmax(getHeight(), lastWindowH));
        if (t >= 1.0f)
            layoutTargetW = 0;
    }

    // Transport state is read from processor atomics (updated in processBlock).
    // Do not call getPlayHead()->getPosition() here — AU hosts can crash
    // when the editor timer invokes a stale host callback.

    const bool sounding = processorRef.isPreviewSounding();
    transport.setPlaying(sounding);
    fileList.setPlaying(sounding);

    // Keep the BPM readout live with the host (× multiplier when synced).
    transport.setHostBpm(processorRef.hostBpm.load());
    transport.setBpmMultiplier(processorRef.bpmMultiplier.load());

    double step = 0.0;
    if (recording)
    {
        const auto snap = processorRef.recordSnapshot();
        refreshRecordingRoll(snap);
        step = snap.lengthBeats * 4.0;
    }
    else if (const auto* clip = hasRecording ? &recordedClip : selectedClip())
    {
        const auto resolved = resolveClip(*clip,
                                          hasRecording ? effectsInspector.getEdit() : selectedEdit(),
                                          formSelectionIds());
        const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        const double lenBeats = juce::jmax(0.25, (double) (resolved.bars * kStepsPerBar) / 4.0 * stretch);
        const double mult = juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        double loopStart = processorRef.previewLoopStartBeat.load();
        double loopEnd = processorRef.previewLoopEndBeat.load();
        if (loopEnd <= loopStart + 1.0e-9)
        {
            loopStart = 0.0;
            loopEnd = lenBeats;
        }
        const double loopLen = juce::jmax(0.25, loopEnd - loopStart);

        double beat = 0.0;
        if (processorRef.syncToHost.load())
        {
            const double scaled = processorRef.hostBeatPos.load() * mult;
            beat = loopStart + std::fmod(std::fmod(scaled, loopLen) + loopLen, loopLen);
        }
        else
        {
            beat = processorRef.freerunBeat.load();
            if (beat < loopStart || beat >= loopEnd)
                beat = loopStart + std::fmod(std::max(0.0, beat - loopStart), loopLen);
        }
        // Playhead is drawn in unstretched step space.
        step = beat / stretch * 4.0;
    }
    rollEditor.setPlayheadStep(step, recording || sounding);
    miniRoll.setPlayheadStep(step, sounding);
    if (gridView)
        kindGrid.setPlayhead(sounding && !previewMuted, (float) miniRoll.playheadFraction());
}

} // namespace pflow
