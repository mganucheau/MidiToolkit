#include "PluginEditor.h"
#include "FolderIndex.h"
#include "GeneratorEngines.h"
#include <map>
#include <thread>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
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

uint64_t clipDedupeKey(const StepClip& clip)
{
    if (!clip.fileMetaKnown)
        return 0;
    uint64_t h = (uint64_t) clip.fileBytes;
    h ^= (uint64_t) clip.fileModMs * 0x9E3779B97F4A7C15ull;
    h ^= (uint64_t) (uint32_t) clip.name.hashCode() * 0xBF58476D1CE4E5B9ull;
    return h == 0 ? 1 : h;
}

bool clipNameMatchesQuery(const StepClip& clip, const juce::String& trimmedQuery)
{
    return trimmedQuery.isEmpty() || clip.name.containsIgnoreCase(trimmedQuery);
}

int visibleWorkspaceWidth(bool sidebarCollapsed, bool editorOpen, bool effectsOpen, int browserW,
                          bool generatorPane = false)
{
    // One editor pane. openRollMaxWidth() is twice that and doubled the default roll.
    const int rollW = editorOpen ? metrics::editorPaneWidth() : 0;
    if (generatorPane)
    {
        return metrics::effectsPaneWidth()
             + rollW
             + (effectsOpen ? metrics::effectsPaneWidth() : 0);
    }
    const int side = sidebarCollapsed ? metrics::sidebarRailWidth()
                                      : metrics::sidebarExpandedWidth();
    return side
         + browserW
         + rollW
         + (effectsOpen ? metrics::effectsPaneWidth() : 0);
}

bool clipMatchesQuery(const juce::File& file, const BrowserSearch& s)
{
    return browserNameMatches(file.getFileNameWithoutExtension(), s.query);
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

    auto contentKey = [](const StepClip& clip) -> juce::String
    {
        if (clip.filePath.isEmpty())
            return {};
        if (clip.fileMetaKnown)
            return juce::File(clip.filePath).getFileName().toLowerCase()
                 + "|" + juce::String(clip.fileBytes)
                 + "|" + juce::String(clip.fileModMs);
        const juce::File f(clip.filePath);
        if (!f.existsAsFile())
            return {};
        return f.getFileName().toLowerCase()
             + "|" + juce::String(f.getSize())
             + "|" + juce::String(f.getLastModificationTime().toMilliseconds());
    };

    std::map<juce::String, juce::String> keyToPrimary;
    for (const auto& clip : clips)
    {
        const juce::String key = contentKey(clip);
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

bool generatorContentChanged(const GrooveParams& a, const GrooveParams& b, int part)
{
    switch (part)
    {
        case (int) gen::Part::Arp:
            return a.arpEnabled != b.arpEnabled
                || a.arpRateIndex != b.arpRateIndex
                || a.arpDensity != b.arpDensity
                || a.arpMotionIndex != b.arpMotionIndex
                || a.arpShapeIndex != b.arpShapeIndex
                || a.arpStyleIndex != b.arpStyleIndex
                || a.arpGenreIndex != b.arpGenreIndex
                || a.arpLengthIndex != b.arpLengthIndex
                || a.arpFormIndex != b.arpFormIndex
                || a.arpVariation != b.arpVariation
                || a.arpNonce != b.arpNonce
                || a.arpOctaves != b.arpOctaves
                || a.arpGate != b.arpGate;
        case (int) gen::Part::Melody:
            return a.melodyEnabled != b.melodyEnabled
                || a.melodyTypeIndex != b.melodyTypeIndex
                || a.melodyStyleIndex != b.melodyStyleIndex
                || a.melodyGenreIndex != b.melodyGenreIndex
                || a.melodyContourIndex != b.melodyContourIndex
                || a.melodySpeedIndex != b.melodySpeedIndex
                || a.melodyDensity != b.melodyDensity
                || a.melodyLengthIndex != b.melodyLengthIndex
                || a.melodyFormIndex != b.melodyFormIndex
                || a.melodyVariation != b.melodyVariation
                || a.melodyNonce != b.melodyNonce;
        case (int) gen::Part::Chords:
            return a.chordsEnabled != b.chordsEnabled
                || a.chordTypeIndex != b.chordTypeIndex
                || a.chordColorIndex != b.chordColorIndex
                || a.chordVoicingIndex != b.chordVoicingIndex
                || a.chordRhythmIndex != b.chordRhythmIndex
                || a.chordGenreIndex != b.chordGenreIndex
                || a.chordMotionIndex != b.chordMotionIndex
                || a.chordSpeedIndex != b.chordSpeedIndex
                || a.chordDensity != b.chordDensity
                || a.chordLengthIndex != b.chordLengthIndex
                || a.chordFormIndex != b.chordFormIndex
                || a.chordVariation != b.chordVariation
                || a.chordNonce != b.chordNonce;
        case (int) gen::Part::Bass:
            return a.bassEnabled != b.bassEnabled
                || a.bassTypeIndex != b.bassTypeIndex
                || a.bassStyleIndex != b.bassStyleIndex
                || a.bassGenreIndex != b.bassGenreIndex
                || a.bassMotionIndex != b.bassMotionIndex
                || a.bassSpeedIndex != b.bassSpeedIndex
                || a.bassDensity != b.bassDensity
                || a.bassLengthIndex != b.bassLengthIndex
                || a.bassFormIndex != b.bassFormIndex
                || a.bassVariation != b.bassVariation
                || a.bassNonce != b.bassNonce;
        case (int) gen::Part::Drums:
            return a.drumsEnabled != b.drumsEnabled
                || a.drumStyleIndex != b.drumStyleIndex
                || a.drumSpeedIndex != b.drumSpeedIndex
                || a.drumDensity != b.drumDensity
                || a.drumFill != b.drumFill
                || a.drumLengthIndex != b.drumLengthIndex
                || a.drumFormIndex != b.drumFormIndex
                || a.drumVariation != b.drumVariation
                || a.drumNonce != b.drumNonce;
        default:
            return true;
    }
}

/** Keep notes outside the selection. Notes from `fresh` that start inside the
    selected span replace the selected notes. Toolkit stamps move with them. */
bool spliceGeneratedSelection(StepClip& fresh, const StepClip& previous,
                              const std::set<int>& oldSel, std::set<int>& newSel,
                              ClipEdit& edit, const GrooveParams& groove)
{
    double t0 = 1.0e9;
    double t1 = -1.0;
    int maxId = 0;
    for (const auto& n : previous.notes)
        maxId = std::max(maxId, n.id);

    std::vector<RollNote> kept;
    kept.reserve(previous.notes.size());
    for (const auto& n : previous.notes)
    {
        if (oldSel.count(n.id) != 0)
        {
            t0 = std::min(t0, n.start);
            t1 = std::max(t1, n.start + std::max(0.0, n.len));
        }
        else
            kept.push_back(n);
    }
    if (!(t1 > t0))
        return false;

    NoteGroove inherit{};
    bool haveInherit = false;
    for (int id : oldSel)
    {
        if (const auto it = edit.noteGroove.find(id); it != edit.noteGroove.end())
        {
            inherit = it->second;
            haveInherit = true;
            edit.noteGroove.erase(it);
        }
        edit.notePitch.erase(id);
        edit.moves.erase(id);
        edit.velocities.erase(id);
        edit.deleted.erase(id);
    }
    if (!haveInherit && edit.noteGroove.empty() && !clipFxIsDefault(groove))
    {
        inherit = noteGrooveFrom(groove);
        haveInherit = true;
        for (const auto& n : kept)
            edit.noteGroove[n.id] = inherit;
    }

    for (const auto& n : fresh.notes)
    {
        if (n.start < t0 - 1.0e-6 || n.start >= t1 - 1.0e-6)
            continue;
        auto copy = n;
        copy.id = ++maxId;
        newSel.insert(copy.id);
        if (haveInherit)
            edit.noteGroove[copy.id] = inherit;
        kept.push_back(std::move(copy));
    }

    std::stable_sort(kept.begin(), kept.end(), [](const RollNote& a, const RollNote& b)
    {
        if (std::abs(a.start - b.start) > 1.0e-9) return a.start < b.start;
        if (a.pitch != b.pitch) return a.pitch < b.pitch;
        return a.id < b.id;
    });

    fresh.notes = std::move(kept);
    fresh.noteCount = (int) fresh.notes.size();
    fresh.bars = previous.bars;
    fresh.filePath = previous.filePath;
    fresh.name = previous.name;
    fresh.kind = previous.kind;
    fresh.complexity = previous.complexity;
    fresh.root = previous.root;
    if (previous.bpm > 0.0)
        fresh.bpm = previous.bpm;
    return true;
}

} // namespace

MidiToolkitEditor::MidiToolkitEditor(MidiToolkitProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    // Keep processor / library session (theme, panes, clip). Do not reset here —
    // Ableton destroys and recreates the editor when the window loses focus.
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
    sidebar.onClaimKeys = [this] { claimKeyNav(KeyNavTarget::Sidebar); };
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
            folder.filter = sidebar.listingFilter();
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
        rescanFolder(true, {}, true);
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
            folder.filter = sidebar.listingFilter();
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
    sidebar.onFilterChanged = [this] { applyFilterDraft(); };
    sidebar.onFilterDraftChanged = [this]
    {
        if (sidebar.isFilterOpen())
            kickFilterCount();
    };
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
    // Search + Filters live under the Browser/Generator header (LeftPaneTabs).
    content.addChildComponent(sidebar.searchComponent());
    browserBar.onToggleFilters = [this] { sidebar.setFilterOpen(!sidebar.isFilterOpen()); };
    browserBar.onToggleStarFolder = [this]
    {
        if (!rootDir.isDirectory()) return;
        processorRef.toggleStarredFolder(rootDir.getFullPathName());
        refreshSidebar();
        refreshBrowserBar();
    };
    browserBar.onRevealInFinder = [this]
    {
        if (rootDir.isDirectory())
            rootDir.revealToUser();
    };
    browserBar.onExportLibraryFolder = [this] { exportLibraryFolder(); };
    browserBar.onSelectAllFiles = [this]
    {
        fileList.selectAllFiles();
        kindGrid.repaint();
    };
    browserBar.onExportListedFiles = [this] { exportListedFilesAsZip(); };
    browserBar.onResetFilters = [this]
    {
        sidebar.resetFilters();
        applyFilterDraft();
        layoutContent();
    };
    browserBar.onToggleSubfolders = [this]
    {
        const bool on = !sidebar.getIncludeSubdirs();
        sidebar.setIncludeSubdirs(on);
        sidebar.noteCommittedSubdirs(on);
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
        kindGrid.setKindMask(sidebar.listingFilter().kindMask);
        kindGrid.refresh();
        if (gridView)
            kindGrid.scrollSelectionToTop();
        applyLayoutState();
        layoutContent();
        if (gridView) kindGrid.grabKeyboardFocus();
    };
    sidebar.onSortChanged = [](BrowserSortColumn) {};
    sidebar.onSortDirectionChanged = [](bool) {};
    kindGrid.setSource(&fileList);
    kindGrid.setKindMask(sidebar.listingFilter().kindMask);
    kindGrid.onSelect = [this](int idx)
    {
        claimKeyNav(KeyNavTarget::Browser);
        fileList.setSelectedIndex(idx, juce::sendNotification);
        kindGrid.ensureSelectedVisible();
        kindGrid.repaint();
    };
    kindGrid.onSelectEntries = [this](const std::vector<int>& ids, int primary)
    {
        claimKeyNav(KeyNavTarget::Browser);
        fileList.setSelectedEntries(ids, primary);
        kindGrid.repaint();
    };
    kindGrid.onExportSelection = [this] { exportLibraryFolder(); };
    kindGrid.onToggleStar = [this](int idx)
    {
        if (fileList.onToggleStar) fileList.onToggleStar(idx);
        kindGrid.refresh();
    };
    kindGrid.onOpen = [this](int idx) { if (fileList.onOpenInPianoRoll) fileList.onOpenInPianoRoll(idx); };
    kindGrid.onDragFile = [this](const juce::File& f) { if (fileList.onDragFile) fileList.onDragFile(f); };
    kindGrid.onActivated = [this] { claimKeyNav(KeyNavTarget::Browser); };
    kindGrid.onSortBy = [this](BrowserSortColumn col)
    {
        sidebar.setSortColumn(col);
        fileList.setSort(col, sidebar.sortIsAscending());
        appliedSortColumn = col;
        appliedSortAscending = sidebar.sortIsAscending();
        kindGrid.refresh();
    };
    kindGrid.onHideColumn = [this](ClipKind kind)
    {
        sidebar.hideKindColumn(kind);
    };
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
    filterPopover.onApply = [this] { applyFilterDraft(); };
    filterPopover.onReset = [this]
    {
        sidebar.resetFilters();
        filterPopover.setShowChanged(false);
        sidebar.setFilterShowChangedOnly(false);
        refreshBrowserBar();
    };
    filterPopover.onSaveDefault = [this]
    {
        applyFilterDraft();
        const auto path = rootDir.isDirectory() ? rootDir.getFullPathName()
                                                : processorRef.lastBrowserDir;
        if (path.isNotEmpty())
        {
            SavedLibraryFolder folder;
            folder.path = path;
            folder.scanSubdirs = sidebar.getIncludeSubdirs();
            folder.filter = sidebar.listingFilter();
            folder.filter.subdirs = folder.scanSubdirs;
            processorRef.saveLibraryFolder(folder, true);
        }
        refreshBrowserBar();
    };
    filterPopover.onShowChanged = [this](bool on)
    {
        sidebar.setFilterShowChangedOnly(on);
        layoutContent();
    };
    filterPopover.onSaveSearch = [this] { if (sidebar.onSaveCurrentSearch) sidebar.onSaveCurrentSearch(); };
    filterDismissShield.onDismiss = [this] { sidebar.setFilterOpen(false); };
    content.addChildComponent(filterDismissShield);
    content.addChildComponent(filterPopover);
    sidebar.onFilterOpenChanged = [this](bool open)
    {
        if (!open)
        {
            cancelFilterCount();
            revertFilterDraft();
        }
        const bool show = open && leftPaneTab == 0;
        filterDismissShield.setVisible(show);
        filterPopover.setVisible(show);
        layoutContent();
        if (show)
        {
            filterDismissShield.toFront(false);
            filterPopover.toFront(false);
            filterPopover.grabKeyboardFocus();
            kickFilterCount();
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
        kindGrid.setKindMask(sidebar.listingFilter().kindMask);
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
        const bool isDir = row.isDirectory || juce::File(path).isDirectory();
        if (isDir)
            processorRef.toggleStarredFolder(path);
        else
            processorRef.toggleStarred(path);

        // Flip the star in-place so the click feels instant; only rebuild when
        // the starred lists themselves need to change membership.
        if (browseMode == 1 && !isDir)
            loadStarredClips();
        else if (browseMode == 3 && isDir)
            loadStarredFolders();
        else if (const auto* cur = fileList.entryAt(entryIdx))
        {
            auto e = *cur;
            e.starred = isDir ? processorRef.isStarredFolder(path)
                              : processorRef.isStarred(path);
            fileList.updateEntry(entryIdx, e);
            kindGrid.refresh();
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
    fileList.onExportFilesAsZip = [this](std::vector<juce::File> files)
    {
        exportFilesAsZip(std::move(files), "Save as Zip");
    };
    fileList.onExportSelection = [this] { exportLibraryFolder(); };
    fileList.onExportListedAsZip = [this] { exportListedFilesAsZip(); };
    fileList.onSetKind = [this](const juce::File& file, ClipKind kind)
    {
        const auto path = file.getFullPathName();
        for (auto& clip : clips)
        {
            if (clip.filePath != path)
                continue;
            clip.kind = kind;
            clip.kindPinned = true;
            clip.kindConfidence = 100;
        }
        if (rootDir.isDirectory())
        {
            auto idx = FolderIndex::load(rootDir, recursiveBrowse)
                           .value_or(FolderIndex::fromClips(rootDir, recursiveBrowse, clips));
            if (idx.rootPath.isEmpty())
                idx = FolderIndex::fromClips(rootDir, recursiveBrowse, clips);
            idx.pinKind(path, kind);
            idx.save();
        }
        rebuildEntries();
    };
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
        // Keep every metadata column on screen by growing the browser pane with the name column.
        browserPaneW = juce::jmax(browserPaneW, fileList.idealContentWidth());
        processorRef.browserPaneW = browserPaneW;
        persistBrowserSession();
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
        showScopeSettings();
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
        updateMiniPreview();
        pushPreviewToProcessor();
    };
    rollEditor.onSelectionChanged = [this]
    {
        showScopeSettings();
        updateMiniPreview();
        pushPreviewToProcessor();
    };
    rollEditor.onActivated = [this] { claimKeyNav(KeyNavTarget::Editor); };
    rollEditor.onVelocityFold = [this] { applyWindowSize(false); };
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
        mirrorInspectorToHostParams(!applyingHostParams);
        if (recording)
        {
            pushLiveFxToProcessor();
            processorRef.setRecordMonitorFx(processorRef.effectsOpen);
            return;
        }
        if (hasRecording)
        {
            // Generator knobs rewrite the pattern. Toolkit knobs (complexity,
            // swing, …) fall through and stamp only the selected notes.
            if (recordedClip.filePath.startsWith("generator://"))
            {
                if (lastGeneratedPart < 0)
                {
                    const auto path = recordedClip.filePath.fromFirstOccurrenceOf("generator://", false, false);
                    if (path == "arpeggio") lastGeneratedPart = (int) gen::Part::Arp;
                    else if (path == "melody") lastGeneratedPart = (int) gen::Part::Melody;
                    else if (path == "chords") lastGeneratedPart = (int) gen::Part::Chords;
                    else if (path == "bass") lastGeneratedPart = (int) gen::Part::Bass;
                    else if (path == "drums") lastGeneratedPart = (int) gen::Part::Drums;
                }
                if (lastGeneratedPart >= 0
                    && generatorContentChanged(rollEditor.currentGroove(), k, lastGeneratedPart))
                {
                    regenerateGeneratedClip(k);
                    return;
                }
            }
            auto edit = effectsInspector.getEdit();
            if (edit.noteGroove.empty())
                edit.noteGroove = rollEditor.currentEdit().noteGroove;
            if (edit.noteGroove.empty() && !clipFxIsDefault(rollEditor.currentGroove()))
                for (const auto& n : recordedClip.notes)
                    edit.noteGroove[n.id] = noteGrooveFrom(rollEditor.currentGroove());
            const auto stamped = noteGrooveFrom(k);
            for (int id : rollEditor.noteIdsInScope())
                edit.noteGroove[id] = stamped;
            effectsInspector.setEdit(edit, juce::dontSendNotification);
            rollEditor.setClip(recordedClip, edit, k);
            pushPreviewToProcessor();
            updateMiniPreview();
            return;
        }
        if (const auto* clip = selectedClip())
        {
            GrooveParams applied = k;
            if ((processorRef.sectionLocks & toolkitLock::AnyGroove) != 0)
            {
                captureGrooveSectionLock(k, processorRef.lockedGroove, processorRef.sectionLocks,
                                         clip->complexity);
                applyGrooveSectionLock(processorRef.lockedGroove, applied, processorRef.sectionLocks);
            }
            processorRef.grooveFor(clip->filePath) = applied;
            auto& stored = processorRef.editFor(clip->filePath);
            if (stored.noteGroove.empty() && !clipFxIsDefault(rollEditor.currentGroove()))
                for (const auto& n : clip->notes)
                    stored.noteGroove[n.id] = noteGrooveFrom(rollEditor.currentGroove());
            const auto stamped = noteGrooveFrom(applied);
            for (int id : rollEditor.noteIdsInScope())
                stored.noteGroove[id] = stamped;
            rollEditor.setClip(*clip, stored, applied);
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
        mirrorInspectorToHostParams(!applyingHostParams);
        if (recording)
        {
            pushLiveFxToProcessor();
            processorRef.setRecordMonitorFx(processorRef.effectsOpen);
            return;
        }
        if (hasRecording)
        {
            auto next = e;
            if (next.notePitch.empty())
                next.notePitch = rollEditor.currentEdit().notePitch;
            if (next.noteGroove.empty())
                next.noteGroove = rollEditor.currentEdit().noteGroove;
            if (next.notePitch.empty()
                && pitchSettingsActive(capturePitchSettings(rollEditor.currentEdit())))
                for (const auto& n : recordedClip.notes)
                    next.notePitch[n.id] = capturePitchSettings(rollEditor.currentEdit());
            const auto stamped = capturePitchSettings(next);
            for (int id : rollEditor.noteIdsInScope())
                next.notePitch[id] = stamped;
            effectsInspector.setEdit(next, juce::dontSendNotification);
            rollEditor.setClip(recordedClip, next, effectsInspector.getGroove());
            pushPreviewToProcessor();
            updateMiniPreview();
            return;
        }
        if (const auto* clip = selectedClip())
        {
            const ClipEdit previous = processorRef.editFor(clip->filePath);
            ClipEdit next = e;
            next.noteGroove = previous.noteGroove;
            next.notePitch = previous.notePitch;
            if (next.notePitch.empty() && pitchSettingsActive(capturePitchSettings(previous)))
                for (const auto& n : clip->notes)
                    next.notePitch[n.id] = capturePitchSettings(previous);
            const auto stamped = capturePitchSettings(next);
            for (int id : rollEditor.noteIdsInScope())
                next.notePitch[id] = stamped;
            processorRef.editFor(clip->filePath) = next;
            captureLocksFromEdit(previous, next, clip);
            rollEditor.setClip(*clip, next, selectedGroove());
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
                e.noteFilterType = NoteFilterType::Off;
                e.drumMap = false;
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
        mirrorInspectorToHostParams(!applyingHostParams);
    };

    // Host automation (Ableton etc.) → inspector, through the handlers above.
    processorRef.onHostParamsChanged = [this](const std::vector<int>& bindings)
    {
        applyHostParams(bindings);
    };
    processorRef.onHostBrowserIndex = [this](int position)
    {
        if (!selectFilePosition(position))
            processorRef.pendingBrowserIndex.store(position);   // list not ready — timer retries
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
        applyWindowSize(false);
    };
    content.addAndMakeVisible(effectsInspector);

    leftPaneTabs.onTab = [this](int tab) { setLeftPaneTab(tab); };
    leftPaneTabs.onToggleGrid = [this]
    {
        gridView = !gridView;
        leftPaneTabs.setGrid(gridView);
        browserBar.setGrid(gridView);
        kindGrid.setKindMask(sidebar.listingFilter().kindMask);
        kindGrid.refresh();
        if (gridView)
            kindGrid.scrollSelectionToTop();
        applyLayoutState();
        layoutContent();
        if (gridView) kindGrid.grabKeyboardFocus();
    };
    leftPaneTabs.onToggleSearch = [this]
    {
        browserSearchOpen = !browserSearchOpen;
        leftPaneTabs.setSearchOpen(browserSearchOpen);
        layoutContent();
        sidebar.setSearchFormOpen(browserSearchOpen);
    };
    leftPaneTabs.onToggleFilters = [this] { sidebar.setFilterOpen(!sidebar.isFilterOpen()); };
    content.addAndMakeVisible(leftPaneTabs);
    generatorViewport.setViewedComponent(&generatorBody, false);
    generatorViewport.setScrollBarsShown(true, false);
    generatorViewport.setScrollBarThickness(10);
    content.addChildComponent(generatorViewport);
    effectsInspector.setExternalGeneratorHost(&generatorBody);

    passthroughVeil.setVisible(false);
    content.addChildComponent(passthroughVeil);

    restoreBrowserSession();
    sidebar.commitListingFilter();
    appliedSortColumn = sidebar.getSortColumn();
    appliedSortAscending = sidebar.sortIsAscending();
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
    // Parameters are the live Toolkit state until a clip loads (host may have moved them
    // while the editor was closed), so adopt them before the first snapshot is pushed.
    adoptHostParamsIntoInspector();
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
    // Back to headless: the processor folds host parameter moves into LiveFxState itself.
    processorRef.onHostParamsChanged = nullptr;
    processorRef.onHostBrowserIndex = nullptr;
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
    rollEditor.noteColour = ds::note();
    miniRoll.setNoteColour(ds::note());
    rollEditor.repaint();
    miniRoll.repaint();
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
    claimKeyNav(KeyNavTarget::Browser);
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
    processorRef.browserSessionSearch = sidebar.listingFilter();
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

juce::String MidiToolkitEditor::listingCacheKey(const juce::String& path, bool recursive)
{
    return path + (recursive ? "\n1" : "\n0");
}

void MidiToolkitEditor::setBrowseScanPhase(const juce::String& phase)
{
    fileList.setScanPhase(phase);
    sidebar.setScanPhase(phase);
}

void MidiToolkitEditor::stashCurrentListing()
{
    if (!folderScanned || clips.empty() || listingRoot.isEmpty())
        return;
    CachedFolderListing cached;
    cached.clips = std::move(clips);
    cached.subdirs = std::move(cachedSubdirs);
    clips.clear();
    cachedSubdirs.clear();
    folderScanned = false;
    listingCache[listingCacheKey(listingRoot, listingRecursive)] = std::move(cached);
}

void MidiToolkitEditor::applyLoadedListing(std::vector<StepClip> loaded,
                                           std::vector<juce::File> subdirs,
                                           const juce::File& dir, bool recursive,
                                           const juce::String& previousPath)
{
    if (!rootDir.isDirectory() || rootDir.getFullPathName() != dir.getFullPathName())
        return;

    sidebarScanning = false;
    sidebar.setScanning(false);
    fileList.setSearching(false);
    clips = std::move(loaded);
    folderScanned = true;
    listingRoot = dir.getFullPathName();
    listingRecursive = recursive;
    sidebar.setScanFinished(true);
    cachedMidiFiles.clear();
    cachedSubdirs = std::move(subdirs);
    auto folderName = dir.getFileName();
    if (folderName.isEmpty()) folderName = dir.getFullPathName();
    fileList.setFolderName(folderName);
    fileList.setEmptyHint(clips.empty() ? "No files" : juce::String());

    // Let the overlay paint away before sorting a huge listing on this thread.
    const int epoch = ++listingApplyEpoch;
    juce::Component::SafePointer<MidiToolkitEditor> safe(this);
    juce::MessageManager::callAsync([safe, epoch, previousPath]()
    {
        if (safe == nullptr || epoch != safe->listingApplyEpoch.load())
            return;
        safe->rebuildEntries();
        safe->selectPathOrFirst(previousPath);
        safe->refreshSidebar();
        safe->persistBrowserSession();
    });
}

void MidiToolkitEditor::rescanFolder(bool keepSelection, const juce::String& preferredPath,
                                     bool forceRescan)
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

    ++listingApplyEpoch;
    const auto newKey = listingCacheKey(dir.getFullPathName(), recursive);
    const bool alreadyShowing = folderScanned && !clips.empty()
                                && listingRoot == dir.getFullPathName()
                                && listingRecursive == recursive;
    if (!forceRescan && alreadyShowing)
    {
        fileList.setSearching(false);
        sidebarScanning = false;
        sidebar.setScanning(false);
        fileList.setEmptyHint(clips.empty() ? "No files" : juce::String());
        rebuildEntries();
        if (previousPath.isNotEmpty())
            selectPathOrFirst(previousPath);
        return;
    }

    if (forceRescan)
        listingCache.erase(newKey);
    else if (auto it = listingCache.find(newKey); it != listingCache.end())
    {
        auto cached = std::move(it->second);
        listingCache.erase(it);
        if (listingRoot != dir.getFullPathName() || listingRecursive != recursive)
            stashCurrentListing();
        selectedIdx = -1;
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
        applyLoadedListing(std::move(cached.clips), std::move(cached.subdirs),
                           dir, recursive, previousPath);
        return;
    }

    if (listingRoot != dir.getFullPathName() || listingRecursive != recursive)
        stashCurrentListing();

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

    // Metadata scan stays in the background. A saved listing is shown first.
    const int gen = beginBackgroundClipLoad(true);
    if (!forceRescan)
        setBrowseScanPhase("LOADING SAVED LISTING");
    juce::Component::SafePointer<MidiToolkitEditor> safe(this);
    juce::Thread::launch([safe, dir, recursive, gen, previousPath, forceRescan]()
    {
        std::vector<StepClip> found;
        try
        {
            if (!forceRescan)
            {
                if (auto idx = FolderIndex::load(dir, recursive); idx && !idx->files.empty())
                {
                    auto loaded = FolderIndex::toClips(*idx);
                    std::vector<juce::File> childDirs;
                    if (!recursive)
                        childDirs = listDirectoryQuick(dir, 0).directories;
                    juce::MessageManager::callAsync([safe, gen, loaded = std::move(loaded),
                                                     childDirs = std::move(childDirs), dir,
                                                     recursive, previousPath]() mutable
                    {
                        if (safe == nullptr || gen != safe->folderScanGeneration.load())
                            return;
                        if (!safe->rootDir.isDirectory()
                            || safe->rootDir.getFullPathName() != dir.getFullPathName())
                            return;
                        safe->applyLoadedListing(std::move(loaded), std::move(childDirs),
                                                 dir, recursive, previousPath);
                    });
                    return;
                }
                if (safe != nullptr)
                {
                    juce::MessageManager::callAsync([safe, gen]()
                    {
                        if (safe == nullptr || gen != safe->folderScanGeneration.load())
                            return;
                        safe->setBrowseScanPhase({});
                    });
                }
            }
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
                            stub.fileBytes = file.getSize();
                            stub.fileModMs = file.getLastModificationTime().toMilliseconds();
                            stub.fileMetaKnown = true;
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
                if (safe != nullptr)
                {
                    juce::MessageManager::callAsync([safe, gen, total]()
                    {
                        if (safe == nullptr || gen != safe->folderScanGeneration.load())
                            return;
                        safe->setBrowseScanPhase("SAVING LISTING");
                        safe->sidebar.setScanProgress(total, total);
                        safe->fileList.setScanProgress(total, total);
                    });
                }
                auto idx = FolderIndex::fromClips(dir, recursive, found);
                if (auto previous = FolderIndex::load(dir, recursive))
                    idx.retainPinnedFrom(*previous);
                idx.save();
            }
        }
        catch (...)
        {
            found.clear();
        }

        std::vector<juce::File> childDirs;
        if (!recursive && !found.empty())
            childDirs = listDirectoryQuick(dir, 0).directories;

        juce::MessageManager::callAsync([safe, results = std::move(found),
                                         childDirs = std::move(childDirs), gen, previousPath,
                                         dir, recursive]() mutable
        {
            if (safe == nullptr || gen != safe->folderScanGeneration.load())
                return;
            if (!safe->rootDir.isDirectory()
                || safe->rootDir.getFullPathName() != dir.getFullPathName())
                return;

            if (results.empty() && !safe->cachedMidiFiles.empty())
            {
                // The directory listing already has MIDI names. An empty parse
                // must not replace that list with a blank browser.
                safe->sidebarScanning = false;
                safe->sidebar.setScanning(false);
                safe->fileList.setSearching(false);
                safe->folderScanned = false;
                safe->rebuildEntries();
                safe->refreshSidebar();
                return;
            }

            safe->applyLoadedListing(std::move(results), std::move(childDirs),
                                     dir, recursive, previousPath);
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

void MidiToolkitEditor::applyFilterDraft()
{
    const bool subdirs = sidebar.getFilter().subdirs;
    const bool subdirsChanged = subdirs != recursiveBrowse;
    const auto sortCol = sidebar.getSortColumn();
    const bool sortAsc = sidebar.sortIsAscending();
    const bool sortChanged = sortCol != appliedSortColumn || sortAsc != appliedSortAscending;

    sidebar.commitListingFilter();
    if (subdirsChanged)
        sidebar.setIncludeSubdirs(subdirs);

    if (sortChanged)
    {
        appliedSortColumn = sortCol;
        appliedSortAscending = sortAsc;
        // The rebuild sorts once. Sorting the current huge list first freezes the host.
        fileList.setSortKey(sortCol, sortAsc);
    }

    if (subdirsChanged)
    {
        if (sidebar.onScanSubdirs)
            sidebar.onScanSubdirs();
    }
    else
    {
        const bool namesOnly = browseMode == 0 && !recursiveBrowse
                               && (clips.empty() || !folderScanned);
        const int n = browseMode == 3 ? 0
                      : (namesOnly ? (int) cachedMidiFiles.size() : (int) clips.size());
        if (n > 30000)
        {
            const juce::String previousPath =
                (selectedIdx >= 0 && selectedIdx < (int) clips.size())
                    ? clips[(size_t) selectedIdx].filePath : juce::String();
            beginCooperativeFilterApply(previousPath);
        }
        else
            applyBrowserFilter();
    }

    refreshBrowserBar();
}

void MidiToolkitEditor::revertFilterDraft()
{
    if (revertingFilter)
        return;
    const juce::ScopedValueSetter<bool> guard(revertingFilter, true);
    sidebar.setCriteria(sidebar.listingFilter());
    sidebar.setSortColumn(appliedSortColumn);
    sidebar.setSortAscending(appliedSortAscending);
}

void MidiToolkitEditor::kickFilterCount()
{
    ++filterCountGen;
    filterCountActive = false;
    filterCountCursor = 0;
    filterCountMatched = 0;
    filterCountTotal = 0;
    filterCountSeen.clear();
    if (!sidebar.isFilterOpen() || filterApplyActive)
        return;

    filterCountDraft = sidebar.getFilter();
    filterCountQuery = filterCountDraft.query.trim();
    const bool namesOnly = browseMode == 0 && !recursiveBrowse
                           && (clips.empty() || !folderScanned);
    const int n = browseMode == 3 ? (int) processorRef.starredFolders.size()
                  : (namesOnly ? (int) cachedMidiFiles.size() : (int) clips.size());
    if (filterCountDraft.removeDuplicates && !namesOnly && browseMode != 3)
        filterCountSeen.reserve((size_t) juce::jmin(n, 65536));

    filterCountActive = true;
    if (n <= 12000)
    {
        while (filterCountActive)
            pumpFilterCount();
        return;
    }
    filterPopover.setCountText("Counting…");
}

void MidiToolkitEditor::cancelFilterCount()
{
    ++filterCountGen;
    filterCountActive = false;
    filterCountSeen.clear();
}

void MidiToolkitEditor::pumpFilterCount()
{
    if (!filterCountActive || !sidebar.isFilterOpen())
    {
        filterCountActive = false;
        return;
    }

    const int gen = filterCountGen;
    const bool namesOnly = browseMode == 0 && !recursiveBrowse
                           && (clips.empty() || !folderScanned);
    const int n = browseMode == 3 ? (int) processorRef.starredFolders.size()
                  : (namesOnly ? (int) cachedMidiFiles.size() : (int) clips.size());
    const auto start = juce::Time::getMillisecondCounter();
    while (filterCountCursor < n && juce::Time::getMillisecondCounter() - start < 8)
    {
        const int end = juce::jmin(n, filterCountCursor + 512);
        for (int i = filterCountCursor; i < end; ++i)
        {
            if (browseMode == 3)
            {
                const auto& path = processorRef.starredFolders[(size_t) i];
                const juce::File d(path);
                if (!d.isDirectory())
                    continue;
                ++filterCountTotal;
                const auto folderName = d.getFileName().isNotEmpty() ? d.getFileName() : path;
                if (filterCountQuery.isEmpty() || folderName.containsIgnoreCase(filterCountQuery))
                    ++filterCountMatched;
            }
            else if (namesOnly)
            {
                ++filterCountTotal;
                const auto stem = cachedMidiFiles[(size_t) i].getFileNameWithoutExtension();
                if (filterCountQuery.isEmpty() || stem.containsIgnoreCase(filterCountQuery))
                    ++filterCountMatched;
            }
            else
            {
                const auto& clip = clips[(size_t) i];
                if (starredFilter && browseMode != 1 && !processorRef.isStarred(clip.filePath))
                    continue;
                ++filterCountTotal;
                if (browseMode != 1 && !clipMatchesFilter(clip, filterCountDraft))
                    continue;
                if (!clipNameMatchesQuery(clip, filterCountQuery))
                    continue;
                if (filterCountDraft.removeDuplicates && clip.fileMetaKnown)
                {
                    if (!filterCountSeen.insert(clipDedupeKey(clip)).second)
                        continue;
                }
                ++filterCountMatched;
            }
        }
        filterCountCursor = end;
    }

    if (gen != filterCountGen)
        return;
    if (filterCountCursor < n)
    {
        filterPopover.setCountText("Counting " + juce::String(filterCountCursor)
                                   + " / " + juce::String(n));
        return;
    }

    filterCountActive = false;
    filterCountSeen.clear();
    const int shownTotal = juce::jmax(filterCountTotal, filterCountMatched);
    filterPopoverCount = juce::String(filterCountMatched) + " of " + juce::String(shownTotal)
                       + (shownTotal == 1 ? " file" : " files");
    filterPopover.setCountText(filterPopoverCount);
}

void MidiToolkitEditor::applyBrowserFilter()
{
    const juce::String previousPath =
        (selectedIdx >= 0 && selectedIdx < (int) clips.size())
            ? clips[(size_t) selectedIdx].filePath : juce::String();

    rebuildEntries();

    finishBrowserFilterApply(previousPath);
}

void MidiToolkitEditor::finishBrowserFilterApply(const juce::String& previousPath)
{
    int nextSel = selectedIdx;
    if (nextSel >= 0 && displayForClip(nextSel) < 0)
        nextSel = -1;
    if (nextSel < 0 && previousPath.isNotEmpty())
    {
        int clipIdx = -1;
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath)
            {
                clipIdx = i;
                break;
            }
        if (clipIdx >= 0 && displayForClip(clipIdx) >= 0)
            nextSel = clipIdx;
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
        folder.filter = sidebar.listingFilter();
        folder.filter.subdirs = folder.scanSubdirs;
        processorRef.saveLibraryFolder(folder, true);
    }
    persistBrowserSession();
    publishListedFilterCount();
}

void MidiToolkitEditor::noteListedFileCount()
{
    listedFileCount = 0;
    for (const auto& row : displayRows)
        if (!row.isDirectory)
            ++listedFileCount;
}

void MidiToolkitEditor::publishListedFilterCount()
{
    const int files = listedFileCount;
    const int total = juce::jmax(files, (int) clips.size());
    filterPopoverCount = juce::String(files) + " of " + juce::String(total)
                       + (total == 1 ? " file" : " files");
    if (sidebar.isFilterOpen())
        filterPopover.setCountText(filterPopoverCount);
}

bool MidiToolkitEditor::isStarredFast(const juce::String& path) const
{
    if (filterApplyStars.empty() || filterApplyStars.count((uint32_t) path.hashCode()) == 0)
        return false;
    return processorRef.starredFiles.contains(path);
}

void MidiToolkitEditor::cancelCooperativeFilterApply()
{
    ++filterApplyGen;
    filterApplyActive = false;
    if (filterApplyOverlay)
    {
        filterApplyOverlay = false;
        fileList.setSearching(false);
    }
}

void MidiToolkitEditor::beginCooperativeFilterApply(const juce::String& previousPath)
{
    cancelFilterCount();
    ++filterApplyGen;
    filterApplyActive = true;
    filterApplyOverlay = true;
    filterApplyPreviousPath = previousPath;
    filterApplyFilter = sidebar.listingFilter();
    filterApplyQuery = filterApplyFilter.query.trim();
    filterApplyNamesOnly = browseMode == 0 && !recursiveBrowse
                           && (clips.empty() || !folderScanned);
    filterApplyPhase = filterApplyNamesOnly ? 2 : 0;
    filterApplyCursor = 0;
    filterApplyPassing.clear();
    filterApplyEntries.clear();
    filterApplyRows.clear();
    filterApplyFirst.clear();
    filterApplyHidden.clear();
    filterApplyLocs.clear();
    filterApplyStars.clear();
    searchDuplicateLocations.clear();
    filterApplyStars.reserve((size_t) processorRef.starredFiles.size());
    for (const auto& path : processorRef.starredFiles)
        filterApplyStars.insert((uint32_t) path.hashCode());
    const int n = filterApplyNamesOnly ? (int) cachedMidiFiles.size() : (int) clips.size();
    if (!filterApplyNamesOnly)
        filterApplyPassing.reserve((size_t) juce::jmin(n, 65536));
    fileList.setScanPhase("Filtering");
    fileList.setScanProgress(0, juce::jmax(1, n));
    fileList.setSearching(true);
}

void MidiToolkitEditor::installFilteredEntries(std::vector<FileListEntry> entries,
                                               std::vector<int> order)
{
    noteListedFileCount();
    if (browseMode == 0)
        sidebar.setCurrentClipCount(listedFileCount);
    sidebar.setFilterHistograms(clips);
    if (filterApplyOverlay)
    {
        filterApplyOverlay = false;
        fileList.setSearching(false);
    }
    if (order.size() == entries.size())
        fileList.setEntriesInOrder(std::move(entries), std::move(order));
    else
        fileList.setEntries(std::move(entries));
}

void MidiToolkitEditor::pumpFilterApply()
{
    if (!filterApplyActive)
        return;

    const int gen = filterApplyGen;
    const int n = filterApplyNamesOnly ? (int) cachedMidiFiles.size() : (int) clips.size();
    const auto start = juce::Time::getMillisecondCounter();
    const bool pitchLocked = (processorRef.sectionLocks & toolkitLock::Pitch) != 0;
    auto budget = [&] { return juce::Time::getMillisecondCounter() - start < 8; };

    if (filterApplyPhase == 0)
    {
        while (filterApplyCursor < n && budget())
        {
            const int end = juce::jmin(n, filterApplyCursor + 400);
            for (int i = filterApplyCursor; i < end; ++i)
            {
                const auto& clip = clips[(size_t) i];
                if (starredFilter && browseMode != 1 && !isStarredFast(clip.filePath))
                    continue;
                if (browseMode != 1 && !clipMatchesFilter(clip, filterApplyFilter))
                    continue;
                if (!clipNameMatchesQuery(clip, filterApplyQuery))
                    continue;
                filterApplyPassing.push_back(i);
            }
            filterApplyCursor = end;
        }
        fileList.setScanProgress(filterApplyCursor, juce::jmax(1, n));
        if (gen != filterApplyGen)
            return;
        if (filterApplyCursor < n)
            return;
        filterApplyPhase = 1;
        filterApplyCursor = 0;
        filterApplyFirst.reserve(filterApplyPassing.size());
    }

    if (filterApplyPhase == 1)
    {
        if (!filterApplyFilter.removeDuplicates)
        {
            filterApplyPhase = 2;
            filterApplyCursor = 0;
        }
        else
        {
            const int pn = (int) filterApplyPassing.size();
            while (filterApplyCursor < pn && budget())
            {
                const int end = juce::jmin(pn, filterApplyCursor + 400);
                for (int p = filterApplyCursor; p < end; ++p)
                {
                    const int i = filterApplyPassing[(size_t) p];
                    const auto& clip = clips[(size_t) i];
                    if (!clip.fileMetaKnown)
                        continue;
                    const auto key = clipDedupeKey(clip);
                    const auto [it, inserted] = filterApplyFirst.emplace(key, i);
                    if (inserted)
                        continue;
                    const auto& prim = clips[(size_t) it->second];
                    if (prim.fileBytes != clip.fileBytes || prim.fileModMs != clip.fileModMs
                        || !prim.name.equalsIgnoreCase(clip.name))
                        continue;
                    filterApplyHidden.insert(i);
                    auto& locs = filterApplyLocs[it->second];
                    if (locs.isEmpty())
                        locs.add(prim.filePath);
                    locs.add(clip.filePath);
                    searchDuplicateLocations[prim.filePath] = locs;
                }
                filterApplyCursor = end;
            }
            fileList.setScanProgress(filterApplyCursor, juce::jmax(1, pn));
            if (gen != filterApplyGen)
                return;
            if (filterApplyCursor < pn)
                return;
            filterApplyPhase = 2;
            filterApplyCursor = 0;
        }
    }

    if (filterApplyPhase == 2)
    {
        if (browseMode == 0 && rootDir.isDirectory() && !recursiveBrowse)
        {
            for (const auto& d : cachedSubdirs)
            {
                if (filterApplyQuery.isNotEmpty() && !d.getFileName().containsIgnoreCase(filterApplyQuery))
                    continue;
                FileListEntry e;
                e.file = d;
                e.name = d.getFileName();
                e.isDirectory = true;
                e.starred = processorRef.isStarredFolder(d.getFullPathName());
                filterApplyEntries.push_back(e);
                filterApplyRows.push_back({ true, -1, d });
            }
        }
        filterApplyPhase = 3;
        filterApplyCursor = 0;
        const int keep = filterApplyNamesOnly ? n : (int) filterApplyPassing.size();
        filterApplyEntries.reserve(filterApplyEntries.size() + (size_t) keep);
        filterApplyRows.reserve(filterApplyRows.size() + (size_t) keep);
    }

    if (filterApplyPhase == 3)
    {
        if (filterApplyNamesOnly)
        {
            while (filterApplyCursor < n && budget())
            {
                const int end = juce::jmin(n, filterApplyCursor + 200);
                for (int i = filterApplyCursor; i < end; ++i)
                {
                    const auto& f = cachedMidiFiles[(size_t) i];
                    const auto stem = f.getFileNameWithoutExtension();
                    if (filterApplyQuery.isNotEmpty() && !stem.containsIgnoreCase(filterApplyQuery))
                        continue;
                    FileListEntry e;
                    e.file = f;
                    e.name = stem;
                    e.starred = isStarredFast(f.getFullPathName());
                    e.kind = ClipKind::Unsure;
                    filterApplyEntries.push_back(std::move(e));
                    filterApplyRows.push_back({ false, -1, f });
                }
                filterApplyCursor = end;
            }
            fileList.setScanProgress(filterApplyCursor, juce::jmax(1, n));
            if (filterApplyCursor < n)
                return;
        }
        else
        {
            const int pn = (int) filterApplyPassing.size();
            while (filterApplyCursor < pn && budget())
            {
                const int end = juce::jmin(pn, filterApplyCursor + 200);
                for (int p = filterApplyCursor; p < end; ++p)
                {
                    const int i = filterApplyPassing[(size_t) p];
                    if (filterApplyHidden.count(i) != 0)
                        continue;
                    const auto& clip = clips[(size_t) i];
                    const bool edited = pitchLocked && !editIsClean(processorRef.editFor(clip.filePath));
                    auto entry = entryForClip(clip, juce::File(clip.filePath), edited, isStarredFast(clip.filePath));
                    if (auto it = filterApplyLocs.find(i); it != filterApplyLocs.end() && it->second.size() > 1)
                        entry.locations = it->second;
                    filterApplyEntries.push_back(std::move(entry));
                    filterApplyRows.push_back({ false, i, juce::File(clip.filePath) });
                }
                filterApplyCursor = end;
            }
            fileList.setScanProgress(filterApplyCursor, juce::jmax(1, pn));
            if (gen != filterApplyGen)
                return;
            if (filterApplyCursor < pn)
                return;
        }

        filterApplyPhase = 4;
    }

    if (filterApplyPhase != 4 || gen != filterApplyGen)
        return;

    filterApplyActive = false;
    if (filterApplyQuery.isNotEmpty())
    {
        bool anyFile = false;
        for (const auto& e : filterApplyEntries)
            if (!e.isDirectory) { anyFile = true; break; }
        if (!anyFile)
            fileList.setEmptyHint("No matches");
    }

    auto entries = std::move(filterApplyEntries);
    auto rows = std::move(filterApplyRows);
    filterApplyPassing.clear();
    filterApplyFirst.clear();
    filterApplyHidden.clear();
    filterApplyLocs.clear();
    filterApplyStars.clear();

    if (entries.size() <= 20000)
    {
        displayRows = std::move(rows);
        installFilteredEntries(std::move(entries), {});
        finishBrowserFilterApply(filterApplyPreviousPath);
        return;
    }

    fileList.setScanPhase("Sorting");
    struct Payload
    {
        std::vector<FileListEntry> entries;
        std::vector<DisplayRow> rows;
        std::vector<int> order;
        BrowserSortColumn column = BrowserSortColumn::Name;
        bool ascending = true;
    };
    auto payload = std::make_shared<Payload>();
    payload->entries = std::move(entries);
    payload->rows = std::move(rows);
    payload->column = fileList.getSortColumn();
    payload->ascending = fileList.isSortAscending();
    const auto previousPath = filterApplyPreviousPath;
    juce::Component::SafePointer<MidiToolkitEditor> safe(this);
    juce::Thread::launch([payload, safe, gen, previousPath]
    {
        const int count = (int) payload->entries.size();
        payload->order.resize((size_t) count);
        std::iota(payload->order.begin(), payload->order.end(), 0);
        std::sort(payload->order.begin(), payload->order.end(), [&](int a, int b)
        {
            return fileListEntryComesBefore(payload->entries[(size_t) a],
                                            payload->entries[(size_t) b],
                                            payload->column, payload->ascending, false);
        });
        juce::MessageManager::callAsync([payload, safe, gen, previousPath]
        {
            if (safe == nullptr || gen != safe->filterApplyGen)
                return;
            safe->displayRows = std::move(payload->rows);
            safe->installFilteredEntries(std::move(payload->entries), std::move(payload->order));
            safe->finishBrowserFilterApply(previousPath);
        });
    });
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
    if (filterApplyActive || filterApplyOverlay)
        cancelCooperativeFilterApply();

    displayRows.clear();
    std::vector<FileListEntry> entries;

    const auto filter = sidebar.listingFilter();
    const auto query = filter.query.trim();
    const bool pitchLocked = (processorRef.sectionLocks & toolkitLock::Pitch) != 0;
    std::unordered_set<uint32_t> starHashes;
    starHashes.reserve((size_t) processorRef.starredFiles.size());
    for (const auto& path : processorRef.starredFiles)
        starHashes.insert((uint32_t) path.hashCode());
    const auto starred = [&](const juce::String& path)
    {
        if (starHashes.empty() || starHashes.count((uint32_t) path.hashCode()) == 0)
            return false;
        return processorRef.starredFiles.contains(path);
    };

    // Build the clip pool that passes the FILTER (search already narrowed by query).
    std::vector<int> passing;
    passing.reserve(clips.size());
    for (int i = 0; i < (int) clips.size(); ++i)
    {
        const auto& clip = clips[(size_t) i];
        // Starred library mode already lists only favourites — don't re-filter stars.
        if (starredFilter && browseMode != 1 && !starred(clip.filePath))
            continue;
        // Favorites views keep every starred path; FILTER chips don't hide them.
        if (browseMode != 1 && !clipMatchesFilter(clip, filter))
            continue;
        // Name search applies to every listing, including favorites.
        if (!clipNameMatchesQuery(clip, query))
            continue;
        passing.push_back(i);
    }

    std::unordered_set<int> hiddenDupes;
    std::unordered_map<int, juce::StringArray> dupeLocs;
    searchDuplicateLocations.clear();
    if (filter.removeDuplicates && !passing.empty())
    {
        std::unordered_map<uint64_t, int> firstOf;
        firstOf.reserve(passing.size());
        for (int i : passing)
        {
            const auto& clip = clips[(size_t) i];
            if (!clip.fileMetaKnown)
                continue;
            const auto key = clipDedupeKey(clip);
            const auto [it, inserted] = firstOf.emplace(key, i);
            if (inserted)
                continue;
            const auto& prim = clips[(size_t) it->second];
            if (prim.fileBytes != clip.fileBytes || prim.fileModMs != clip.fileModMs
                || !prim.name.equalsIgnoreCase(clip.name))
                continue;
            hiddenDupes.insert(i);
            auto& locs = dupeLocs[it->second];
            if (locs.isEmpty())
                locs.add(prim.filePath);
            locs.add(clip.filePath);
            searchDuplicateLocations[prim.filePath] = locs;
        }
    }

    if (browseMode == 0 && rootDir.isDirectory() && !recursiveBrowse)
    {
        for (const auto& d : cachedSubdirs)
        {
            if (!browserNameMatches(d.getFileName(), filter.query))
                continue;
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
            const auto folderName = d.getFileName().isNotEmpty() ? d.getFileName() : path;
            if (!browserNameMatches(folderName, filter.query))
                continue;
            FileListEntry e;
            e.file = d;
            e.name = folderName;
            e.isDirectory = true;
            e.starred = true;
            entries.push_back(e);
            displayRows.push_back({ true, -1, d });
        }
        if (query.isNotEmpty() && entries.empty())
            fileList.setEmptyHint("No matches");
        fileList.setEntries(std::move(entries));
        fileList.setFolderName("Favorite Folders");
        noteListedFileCount();
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
            if (!browserNameMatches(f.getFileNameWithoutExtension(), query))
                continue;
            const auto path = f.getFullPathName();
            int clipIdx = -1;
            for (int i = 0; i < (int) clips.size(); ++i)
                if (clips[(size_t) i].filePath == path) { clipIdx = i; break; }

            FileListEntry e;
            if (clipIdx >= 0)
            {
                const auto& clip = clips[(size_t) clipIdx];
                const bool edited = pitchLocked && !editIsClean(processorRef.editFor(clip.filePath));
                e = entryForClip(clip, f, edited, starred(clip.filePath));
            }
            else
            {
                e.file = f;
                e.name = f.getFileNameWithoutExtension();
                e.starred = starred(path);
                if (auto tagged = kindKeywordFromFileName(path))
                    e.kind = *tagged;
                else if (auto tagged = kindKeywordFromFileName(e.name))
                    e.kind = *tagged;
                else
                    e.kind = ClipKind::Unsure;
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
            if (hiddenDupes.count(i) != 0)
                continue;

            const bool edited = pitchLocked && !editIsClean(processorRef.editFor(clip.filePath));
            auto entry = entryForClip(clip, juce::File(clip.filePath), edited, starred(clip.filePath));
            if (auto it = dupeLocs.find(i); it != dupeLocs.end() && it->second.size() > 1)
                entry.locations = it->second;
            entries.push_back(std::move(entry));
            displayRows.push_back({ false, i, juce::File(clip.filePath) });
        }
    }

    if (query.isNotEmpty())
    {
        bool anyFile = false;
        for (const auto& row : displayRows)
            if (!row.isDirectory) { anyFile = true; break; }
        if (!anyFile)
            fileList.setEmptyHint("No matches");
    }
    fileList.setEntries(std::move(entries));
    noteListedFileCount();
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
    effectsInspector.setLiveStreamMode(processorRef.passthrough);   // Input mode hides Playback + Form
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
        if (clip != nullptr)
        {
            effectsInspector.setEdit(selectedEdit(), juce::dontSendNotification);
            effectsInspector.setGroove(selectedGroove(), juce::dontSendNotification);
            showScopeSettings();
        }
        else
        {
            // No clip: the host parameters hold the live Toolkit state (effect mode).
            adoptHostParamsIntoInspector();
        }
    }
    effectsInspector.setBpmMultiplier(processorRef.bpmMultiplier.load(), juce::dontSendNotification);
    effectsInspector.setSectionLocks(processorRef.sectionLocks);
    // Parameters mirror the active clip's Toolkit / Generator state.
    if (clip != nullptr)
        mirrorInspectorToHostParams(false);
    updateMiniPreview();
}

void MidiToolkitEditor::mirrorInspectorToHostParams(bool userGesture)
{
    processorRef.hostParams().pushFrom(effectsInspector.getGroove(), effectsInspector.getEdit(),
                                       effectsInspector.getBpmMultiplier(), userGesture);
}

void MidiToolkitEditor::adoptHostParamsIntoInspector()
{
    GrooveParams groove = selectedGroove();
    ClipEdit edit = selectedEdit();
    double bpmMult = effectsInspector.getBpmMultiplier();   // tempo stays processor-owned
    std::vector<int> all((size_t) processorRef.hostParams().numBindings());
    for (int i = 0; i < (int) all.size(); ++i)
        all[(size_t) i] = i;
    processorRef.hostParams().applyBindings(all, groove, edit, bpmMult);
    effectsInspector.setGroove(groove, juce::dontSendNotification);
    effectsInspector.setEdit(edit, juce::dontSendNotification);
}

void MidiToolkitEditor::applyHostParams(const std::vector<int>& bindings)
{
    auto groove = effectsInspector.getGroove();
    auto edit = effectsInspector.getEdit();
    double bpmMult = effectsInspector.getBpmMultiplier();
    const auto applied = processorRef.hostParams().applyBindings(bindings, groove, edit, bpmMult);
    if (!applied.any())
        return;

    // Load the inspector first, then fire the same handlers a UI edit would, so
    // locks, regeneration, preview and LiveFxState (pushLiveFxToProcessor) all follow.
    if (applied.tempo)
        effectsInspector.setBpmMultiplier(bpmMult, juce::dontSendNotification);
    if (applied.groove)
        effectsInspector.setGroove(groove, juce::dontSendNotification);
    if (applied.edit)
        effectsInspector.setEdit(edit, juce::dontSendNotification);

    const bool wasApplying = applyingHostParams;
    applyingHostParams = true;
    if (applied.tempo && effectsInspector.onBpmMultiplierChanged)
        effectsInspector.onBpmMultiplierChanged(bpmMult);
    if (applied.groove && effectsInspector.onGrooveChanged)
        effectsInspector.onGrooveChanged(groove);
    if (applied.edit && effectsInspector.onEditChanged)
        effectsInspector.onEditChanged(edit);
    applyingHostParams = wasApplying;

    // Exclusive generators / articulation knobs can move parameters the host did not touch.
    mirrorInspectorToHostParams(false);
}

int MidiToolkitEditor::selectedFilePosition() const
{
    const int d = displayForClip(selectedIdx);
    if (d < 0)
        return -1;
    int position = 0;
    for (int i = 0; i < d; ++i)
        if (!displayRows[(size_t) i].isDirectory)
            ++position;
    return position;
}

bool MidiToolkitEditor::selectFilePosition(int position)
{
    if (recording || position < 0)
        return false;
    const DisplayRow* target = nullptr;
    int n = 0;
    for (const auto& row : displayRows)
    {
        if (row.isDirectory)
            continue;
        target = &row;                 // clamp: the last file when the host index overshoots
        if (n++ == position)
            break;
    }
    if (target == nullptr)
        return false;

    if (target->clipIndex < 0)
        ensureAndSelectFile(target->file);
    else if (target->clipIndex != selectedIdx)
        selectIndex(target->clipIndex);
    return true;
}

void MidiToolkitEditor::publishBrowserIndexToHost()
{
    // Host asked for a file before the list was ready: do not overwrite its value.
    if (processorRef.pendingBrowserIndex.load() >= 0)
        return;
    const auto* clip = selectedClip();
    if (clip == nullptr)
        return;
    const int position = selectedFilePosition();
    if (position < 0)
        return;
    // Only when the selection or its list position changed — a host move that is still
    // loading (async note parse) must not be written back to the old file.
    if (clip->filePath == lastPublishedBrowserPath && position == lastPublishedBrowserPos)
        return;
    lastPublishedBrowserPath = clip->filePath;
    lastPublishedBrowserPos = position;

    int files = 0;
    for (const auto& row : displayRows)
        if (!row.isDirectory)
            ++files;
    // A host value past the end of the list already means "last file".
    const int hostPosition = juce::jmin(processorRef.hostParams().browserIndex(), files - 1);
    if (hostPosition != position)
        processorRef.hostParams().pushBrowserIndex(position, false);
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
    auto edit = effectsInspector.getEdit();
    const StepClip previous = recordedClip;
    const auto oldSel = rollEditor.selectedNoteIds();
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
    auto signatureOf = [](const StepClip& clip)
    {
        std::set<std::pair<int, int>> keys;
        for (const auto& n : clip.notes)
            keys.insert({ (int) std::lround(n.start), n.pitch });
        return keys;
    };
    const bool compareToPrevious = !firstGenerate && previous.filePath.startsWith("generator://");
    const auto previousKeys = compareToPrevious ? signatureOf(previous) : std::set<std::pair<int, int>>{};
    GrooveParams rolling = groove;
    bool bumpedSeed = false;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        recordedClip = gen::renderPartClip(part, rolling, keyRoot, edit.mode,
                                           chordCount > 0 ? chordBuf : nullptr, chordCount);
        if (part != gen::Part::Drums && edit.root >= 0)
            for (auto& n : recordedClip.notes)
                n.pitch = fitToScale(n.pitch, edit.root, edit.mode);
        if (!compareToPrevious || signatureOf(recordedClip) != previousKeys)
            break;
        bumpedSeed = true;
        switch (part)
        {
            case gen::Part::Arp:    rolling.arpNonce += 997; break;
            case gen::Part::Melody: rolling.melodyNonce += 997; break;
            case gen::Part::Chords: rolling.chordNonce += 997; break;
            case gen::Part::Bass:   rolling.bassNonce += 997; break;
            case gen::Part::Drums:  rolling.drumNonce += 997; break;
            case gen::Part::Count:  break;
        }
    }
    if (bumpedSeed)
        effectsInspector.setGroove(rolling, juce::dontSendNotification);
    std::set<int> newSel;
    const bool spliced = !firstGenerate && !oldSel.empty()
        && spliceGeneratedSelection(recordedClip, previous, oldSel, newSel, edit, rolling);
    if (spliced)
        effectsInspector.setEdit(edit, juce::dontSendNotification);
    const double bpm = processorRef.hostBpm.load();
    if (recordedClip.bpm <= 0.0)
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
    // Hard preview update so notes from the previous pattern are note-off'd —
    // soft updates would leave hanging notes until the DAW is restarted.
    {
        const auto preview = buildRenderedClip();
        processorRef.setPreviewState(preview, !preview.notes.empty(), previewMuted, false, false);
        pushLiveFxToProcessor();
    }
    updateMiniPreview();
    if (firstGenerate)
        applyLayoutState();
    if (!recordedClip.notes.empty())
        rollEditor.setClip(recordedClip, effectsInspector.getEdit(), effectsInspector.getGroove());
    // Restore after the roll is built so the highlight doesn't scope this pass.
    if (spliced && !newSel.empty())
        rollEditor.setSelectedNoteIds(std::move(newSel));
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
        // Unlocked: remember the analyzed key for melodic clips. Octave range
        // stays Off, and a drum clip is not given a key — both of those were
        // turning filters on and moving kit notes.
        auto& e = processorRef.editFor(clip.filePath);
        prepareBrowsePitchEdit(e, clip);
        if (haveCarriedScale && clip.kind != ClipKind::Drums)
        {
            e.fitScale = carriedScale.fitScale;
            e.mapToRoot = carriedScale.mapToRoot;
            e.root = carriedScale.root;
            e.mode = carriedScale.mode;
        }
    }
    havePendingCarriedScale = false;
    stampLockedSections(true);

    {
        auto& seeded = processorRef.editFor(clip.filePath);
        const auto& fileGroove = processorRef.grooveFor(clip.filePath);
        if (seeded.noteGroove.empty() && !clipFxIsDefault(fileGroove))
            for (const auto& n : clip.notes)
                seeded.noteGroove[n.id] = noteGrooveFrom(fileGroove);
        if (seeded.notePitch.empty() && pitchSettingsActive(capturePitchSettings(seeded)))
            for (const auto& n : clip.notes)
                seeded.notePitch[n.id] = capturePitchSettings(seeded);
    }

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
        {
            applyPitchLock(processorRef.lockedEdit, e);
            // Per-note stamps can still say "native octave" (-1) and then follow
            // each file. The locked section is one value until it is unlocked.
            const auto stamped = capturePitchSettings(e);
            for (auto& entry : e.notePitch)
                entry.second = stamped;
        }
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
        // Same rule as pitch: a locked knob is one value, not each file's stamp.
        auto& pitched = processorRef.editFor(clip.filePath);
        const auto stamped = noteGrooveFrom(g);
        for (auto& entry : pitched.noteGroove)
            entry.second = stamped;
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
    if (!rollEditor.hasSelection())
        return nullptr;
    return &rollEditor.selectedNoteIds();
}

NoteProcessScope MidiToolkitEditor::noteScope() const
{
    NoteProcessScope scope;
    scope.selectedIds = formSelectionIds();
    return scope;
}

void MidiToolkitEditor::showScopeSettings()
{
    if (showingScopeSettings)
        return;
    const auto* clip = hasRecording ? &recordedClip : selectedClip();
    if (clip == nullptr)
        return;
    const auto ids = rollEditor.noteIdsInScope();
    if (ids.empty())
        return;

    const ClipEdit edit = hasRecording ? effectsInspector.getEdit() : selectedEdit();
    juce::ScopedValueSetter<bool> guard(showingScopeSettings, true);

    if (!edit.noteGroove.empty())
    {
        NoteGroove shown{};
        bool first = true;
        bool mixed = false;
        for (int id : ids)
        {
            NoteGroove g{};
            if (const auto it = edit.noteGroove.find(id); it != edit.noteGroove.end())
                g = it->second;
            if (first)
            {
                shown = g;
                first = false;
            }
            else if (!(shown == g))
            {
                mixed = true;
                break;
            }
        }
        GrooveParams view = effectsInspector.getGroove();
        applyNoteGroove(view, mixed ? NoteGroove{} : shown);
        effectsInspector.setGroove(view, juce::dontSendNotification);
    }

    if (!edit.notePitch.empty() && (processorRef.sectionLocks & toolkitLock::Pitch) == 0)
    {
        NotePitchSettings shown{};
        bool first = true;
        bool mixed = false;
        for (int id : ids)
        {
            NotePitchSettings p{};
            if (const auto it = edit.notePitch.find(id); it != edit.notePitch.end())
                p = it->second;
            if (first)
            {
                shown = p;
                first = false;
            }
            else if (!(shown == p))
            {
                mixed = true;
                break;
            }
        }
        ClipEdit view = effectsInspector.getEdit();
        view.noteGroove = edit.noteGroove;
        view.notePitch = edit.notePitch;
        applyPitchSettings(view, mixed ? NotePitchSettings{} : shown);
        effectsInspector.setEdit(view, juce::dontSendNotification);
    }
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
    const auto scope = noteScope();
    const auto resolved = resolveClip(*clip, edit, nullptr, scope);
    auto notes = applyClipGroove(resolved.notes, edit, groove, clip->complexity,
                                 clip->kind, clip->root, edit.mode, scope);
    // Complexity morph can invent ±1/±2 ornaments — keep Fit to Scale honest.
    refitClipGroove(notes, edit, scope);
    const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());

    out.name = clip->name;
    out.bpm = clip->bpm;
    out.lengthBeats = (double) (resolved.bars * kStepsPerBar) / 4.0 * stretch;
    for (const auto& n : notes)
    {
        NoteEvent ev;
        ev.noteNumber = juce::jlimit(0, 127, n.pitch);
        GrooveParams heard = groove;
        if (!edit.noteGroove.empty())
        {
            if (const auto it = edit.noteGroove.find(n.id); it != edit.noteGroove.end())
                applyNoteGroove(heard, it->second);
            else
                applyNoteGroove(heard, NoteGroove{});
        }
        ev.velocity = juce::jlimit(1, 127, (int) std::lround(effectiveVelocity(n, heard) * 127.0));
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

bool writeFilesToZip(const juce::File& dest, const std::vector<juce::File>& files)
{
    if (files.empty())
        return false;

    auto parent = dest.getParentDirectory();
    if (!parent.isDirectory())
        parent.createDirectory();

    // Write beside the destination, then swap, so a cancelled or failed export
    // never leaves a half-written zip where the user asked to save.
    const auto temp = dest.getSiblingFile(dest.getFileNameWithoutExtension() + ".tmp.zip");
    if (temp.existsAsFile())
        temp.deleteFile();

    juce::ZipFile::Builder builder;
    std::set<juce::String> used;
    int added = 0;
    for (const auto& file : files)
    {
        if (!file.existsAsFile())
            continue;
        auto name = file.getFileName();
        if (name.isEmpty())
            continue;
        if (used.count(name) != 0)
        {
            const auto stem = file.getFileNameWithoutExtension();
            const auto ext = file.getFileExtension();
            int n = 2;
            do
                name = stem + " " + juce::String(n++) + ext;
            while (used.count(name) != 0);
        }
        used.insert(name);
        builder.addFile(file, 6, name);
        ++added;
    }
    if (added == 0)
        return false;

    bool wrote = false;
    {
        std::unique_ptr<juce::FileOutputStream> out(temp.createOutputStream());
        if (out == nullptr)
            return false;
        wrote = builder.writeToStream(*out, nullptr);
        out.reset();
    }
    if (!wrote)
    {
        temp.deleteFile();
        return false;
    }
    if (dest.existsAsFile())
        dest.deleteFile();
    if (!temp.moveFileTo(dest))
    {
        temp.deleteFile();
        return false;
    }
    return true;
}

void MidiToolkitEditor::exportListedFilesAsZip()
{
    exportFilesAsZip(fileList.listedMidiFiles(), "Export as Zip");
}

void MidiToolkitEditor::exportFilesAsZip(std::vector<juce::File> files, const juce::String& dialogTitle)
{
    if (files.empty())
        return;

    juce::String leaf = "MIDI Files";
    if (rootDir.isDirectory() && rootDir.getFileName().isNotEmpty())
        leaf = rootDir.getFileName();
    if (files.size() == 1)
        leaf = files.front().getFileNameWithoutExtension();
    leaf = juce::File::createLegalFileName(leaf);
    if (leaf.isEmpty())
        leaf = "MIDI Files";

    auto startDir = rootDir.isDirectory() ? rootDir.getParentDirectory()
                                          : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    if (!startDir.isDirectory())
        startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    auto chooser = std::make_shared<juce::FileChooser>(
        dialogTitle, startDir.getChildFile(leaf + ".zip"), "*.zip");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
        [chooser, files](const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest.getFullPathName().isEmpty())
                return;
            if (!dest.hasFileExtension(".zip"))
                dest = dest.withFileExtension(".zip");
            juce::Thread::launch([dest, files]
            {
                writeFilesToZip(dest, files);
            });
        });
}

void MidiToolkitEditor::exportLibraryFolder()
{
    const auto selected = fileList.selectedMidiFiles();
    if (selected.empty() || !rootDir.isDirectory())
        return;

    struct ExportPanel : juce::Component
    {
        juce::Label title { {}, "Export selection" };
        juce::Label destLabel { {}, "Destination" };
        juce::Label destPath;
        IconBtn chooseBtn { icons::folder, "Choose folder" };
        fx::FlatSwitch zipSwitch;
        IconBtn exportBtn { icons::download, "Export" };
        IconBtn cancelBtn { icons::x, "Cancel" };
        juce::File dest;
        std::function<void(juce::File, bool)> onExport;
        std::function<void()> onCancel;

        ExportPanel()
        {
            title.setFont(uiFontFixed(14.0f, true));
            title.setColour(juce::Label::textColourId, ds::ink());
            destLabel.setFont(uiFontFixed(12.0f, false));
            destLabel.setColour(juce::Label::textColourId, ds::inkMuted());
            destPath.setFont(uiFontFixed(12.0f, false));
            destPath.setColour(juce::Label::textColourId, ds::ink());
            destPath.setJustificationType(juce::Justification::centredLeft);
            chooseBtn.chipChrome = true;
            chooseBtn.caption = "Choose…";
            chooseBtn.onClick = [this]
            {
                auto chooser = std::make_shared<juce::FileChooser>(
                    "Export destination",
                    dest.isDirectory() ? dest
                                       : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                    "");
                chooser->launchAsync(juce::FileBrowserComponent::openMode
                                         | juce::FileBrowserComponent::canSelectDirectories,
                    [this, chooser](const juce::FileChooser& fc)
                    {
                        const auto r = fc.getResult();
                        if (!r.isDirectory()) return;
                        dest = r;
                        destPath.setText(dest.getFullPathName(), juce::dontSendNotification);
                    });
            };
            zipSwitch.setToggleState(false, juce::dontSendNotification);
            exportBtn.chipChrome = true;
            exportBtn.caption = "Export";
            exportBtn.onClick = [this]
            {
                if (dest.isDirectory() && onExport)
                    onExport(dest, zipSwitch.getToggleState());
            };
            cancelBtn.chipChrome = true;
            cancelBtn.onClick = [this] { if (onCancel) onCancel(); };
            addAndMakeVisible(title);
            addAndMakeVisible(destLabel);
            addAndMakeVisible(destPath);
            addAndMakeVisible(chooseBtn);
            addAndMakeVisible(zipSwitch);
            addAndMakeVisible(exportBtn);
            addAndMakeVisible(cancelBtn);
            setSize(360, 168);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(ds::surface100());
            g.setColour(ds::edge());
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), ds::radius::md, 1.0f);
            g.setColour(ds::ink());
            g.setFont(uiFontFixed(12.0f, false));
            g.drawText("Save as zip",
                       juce::Rectangle<int>(16, 96, 120, 24),
                       juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced(16);
            title.setBounds(r.removeFromTop(22));
            r.removeFromTop(10);
            destLabel.setBounds(r.removeFromTop(18));
            auto destRow = r.removeFromTop(28);
            const int chooseW = chooseBtn.captionWidth();
            chooseBtn.setBounds(destRow.removeFromRight(chooseW));
            destRow.removeFromRight(8);
            destPath.setBounds(destRow);
            r.removeFromTop(14);
            zipSwitch.setBounds(140, 96, zipSwitch.idealWidth(), 18);
            auto actions = r.removeFromBottom(28);
            const int exportW = exportBtn.captionWidth();
            exportBtn.setBounds(actions.removeFromRight(exportW));
            actions.removeFromRight(8);
            cancelBtn.setBounds(actions.removeFromRight(28));
        }
    };

    auto* panel = new ExportPanel();
    panel->dest = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    panel->destPath.setText(panel->dest.getFullPathName(), juce::dontSendNotification);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(panel);
    opts.dialogTitle = "Export";
    opts.dialogBackgroundColour = ds::surface100();
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    auto* window = opts.launchAsync();
    if (window == nullptr)
        return;

    panel->onCancel = [window] { window->exitModalState(0); };
    const auto folderName = rootDir.getFileName();
    panel->onExport = [window, selected, folderName](juce::File dest, bool asZip)
    {
        window->exitModalState(1);
        juce::Thread::launch([dest, asZip, selected, folderName]
        {
            const auto leaf = juce::File::createLegalFileName(
                folderName.isEmpty() ? "Selection" : folderName);
            if (asZip)
            {
                writeFilesToZip(dest.getChildFile(leaf + ".zip"), selected);
                return;
            }
            const auto outDir = dest.getChildFile(leaf);
            outDir.createDirectory();
            std::set<juce::String> used;
            for (const auto& f : selected)
            {
                if (!f.existsAsFile() || f.getParentDirectory() == outDir)
                    continue;
                auto name = f.getFileName();
                if (name.isEmpty())
                    continue;
                if (used.count(name) != 0)
                {
                    const auto stem = f.getFileNameWithoutExtension();
                    const auto ext = f.getFileExtension();
                    int n = 2;
                    do
                        name = stem + " " + juce::String(n++) + ext;
                    while (used.count(name) != 0);
                }
                used.insert(name);
                const auto target = outDir.getChildFile(name);
                if (target.existsAsFile())
                    target.deleteFile();
                f.copyFileTo(target);
            }
        });
    };
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
        const auto scope = noteScope();
        const auto resolved = resolveClip(recordedClip, edit, nullptr, scope);
        const int rootPc = edit.root >= 0 ? edit.root : 0;
        auto grooved = applyClipGroove(resolved.notes, edit, groove, recordedClip.complexity,
                                       recordedClip.kind, recordedClip.root, edit.mode, scope);
        refitClipGroove(grooved, edit, scope);
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
    const auto scope = noteScope();
    const auto resolved = resolveClip(*clip, edit, nullptr, scope);
    const int rootPc = edit.root >= 0 ? edit.root : (clip->root >= 0 ? clip->root : 0);
    miniRoll.setNoteColour(ds::note()); // Theme-aware notes (keys stay black/white)
    auto grooved = applyClipGroove(resolved.notes, edit, groove, clip->complexity,
                                   clip->kind, clip->root, edit.mode, scope);
    refitClipGroove(grooved, edit, scope);
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
    btnVel.setVisible(false);
}

void MidiToolkitEditor::PreviewHeader::refresh()
{
    // Title is always just "Preview" — no clip meta.
    if (!name.isEmpty() || !meta.isEmpty())
    {
        name.clear();
        meta.clear();
        repaint();
    }
}

void MidiToolkitEditor::PreviewHeader::resized()
{
    btnVel.setVisible(false);
    btnVel.setBounds({});
}

void MidiToolkitEditor::PreviewHeader::mouseDown(const juce::MouseEvent&)
{
    owner.processorRef.previewOpen = !owner.processorRef.previewOpen;
    owner.applyLayoutState();
    owner.syncPassthroughWindowHeight();
}

void MidiToolkitEditor::PreviewHeader::paint(juce::Graphics& g)
{
    // Same ground as the measure-count ruler on the preview roll.
    g.fillAll(ds::surface100());
    g.setColour(ds::line());
    g.fillRect(0, 0, getWidth(), 1);

    const bool open = owner.processorRef.previewOpen;
    auto header = getLocalBounds().reduced(8, 0);
    auto caret = header.removeFromLeft(12).toFloat().withSizeKeepingCentre(9.0f, 9.0f);
    fx::drawStrokedChevron(g, caret, ds::inkMuted(), open);
    header.removeFromLeft(4);

    g.setColour(ds::ink());
    g.setFont(uiFontFixed(11.0f, false));
    g.drawText("Preview", header, juce::Justification::centredLeft, true);
}

// ── layout ───────────────────────────────────────────────────────────────────

void MidiToolkitEditor::toggleEditorFold()
{
    const bool opening = !processorRef.editorOpen;
    if (opening)
    {
        previewOpenBeforeRoll = processorRef.previewOpen;
        rollHidPreview = true;
    }
    processorRef.editorOpen = !processorRef.editorOpen;
    // Piano roll needs the left column; leave effects-only layout.
    // The preview is removed while the roll is open and restored afterwards.
    if (processorRef.editorOpen)
    {
        processorRef.passthrough = false;
        processorRef.setPassthroughEnabled(false);
        processorRef.previewOpen = false;
    }
    else if (rollHidPreview)
    {
        processorRef.previewOpen = previewOpenBeforeRoll;
        rollHidPreview = false;
    }
    transport.setPassthrough(processorRef.passthrough);
    persistBrowserSession();
    applyLayoutState();
    syncPassthroughWindowHeight();
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
    syncPassthroughWindowHeight();
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
    // Allow growing past content — editor paint fills the extra area (no black void).
    return display != nullptr ? juce::jmax(360, display->userArea.getHeight() - 48) : 1200;
}

int MidiToolkitEditor::defaultRollWindowHeight() const
{
    // Key grid of a default roll, plus the roll toolbar, ruler, and velocity.
    const int keys = metrics::scaled(520);
    return metrics::transportH()
         + metrics::paneHeaderH()
         + 20 // piano-roll ruler
         + keys
         + rollEditor.velocityChromeHeight();
}

int MidiToolkitEditor::generatorWindowHeight() const
{
    int h = metrics::transportH() + metrics::paneHeaderH();
    h += effectsInspector.generatorColumnHeight();
    h += PreviewHeader::kHeaderH;
    if (processorRef.previewOpen)
        h += juce::jmax(metrics::miniRollH(), metrics::scaled(80));
    return h;
}

int MidiToolkitEditor::toolkitWindowHeight() const
{
    return metrics::transportH() + effectsInspector.contentHeight();
}

int MidiToolkitEditor::columnWidth() const
{
    if (recording)
        return metrics::editorPaneWidth()
             + (processorRef.effectsOpen ? metrics::effectsPaneWidth() : 0);
    if (processorRef.passthrough && processorRef.effectsOpen)
        return metrics::effectsPaneWidth();
    const int browser = juce::jmax(browserContentWidth(), metrics::fileTableWidth());
    return visibleWorkspaceWidth(sidebar.isCollapsed(),
                                 processorRef.editorOpen,
                                 processorRef.effectsOpen,
                                 browser,
                                 leftPaneTab == 1);
}

void MidiToolkitEditor::applyWindowSize(bool snapBrowserToDefault)
{
    if (applyingWindowSize || getWidth() < 0)
        return;
    juce::ScopedValueSetter<bool> guard(applyingWindowSize, true);

    const int w = juce::jmax(metrics::sidebarRailWidth(), columnWidth());
    const bool pass = processorRef.passthrough;
    const bool edOpen = processorRef.editorOpen || recording;
    const bool fxOpen = processorRef.effectsOpen;
    const bool generator = leftPaneTab == 1 && !pass && !recording;

    int minH = 0;
    int maxH = 0;
    if (edOpen)
    {
        // Piano roll (and recording) lock to the roll plus the velocity row or lane.
        minH = maxH = defaultRollWindowHeight();
    }
    else if (pass || (fxOpen && generator))
    {
        // Passthrough toolkit, or generator + toolkit: hug content height.
        int h = toolkitWindowHeight();
        if (generator)
            h = juce::jmax(h, generatorWindowHeight());
        minH = maxH = h;
    }
    else if (generator)
    {
        minH = maxH = generatorWindowHeight();
    }
    else
    {
        // Browser tab (toolkit open or closed): keep browser height. Opening the
        // toolkit only adds column width — it must not shrink the window.
        maxH = defaultRollWindowHeight();
        const int shrink = metrics::scaled(520) / 2;
        minH = juce::jmax(metrics::transportH() + metrics::paneHeaderH() + PreviewHeader::kHeaderH,
                          maxH - shrink);
    }

    int h = (minH == maxH || snapBrowserToDefault) ? maxH
          : juce::jlimit(minH, maxH, getHeight() > 0 ? getHeight() : maxH);
    setResizeLimits(w, minH, w, maxH);
    if (std::abs(getWidth() - w) > 1 || std::abs(getHeight() - h) > 1)
        setSize(w, h);
}

int MidiToolkitEditor::filledContentHeight() const
{
    const int transport = metrics::transportH();
    const bool edOpen = processorRef.editorOpen;
    const bool fxOpen = processorRef.effectsOpen;
    const bool previewOpen = !edOpen && processorRef.previewOpen;
    const bool pass = processorRef.passthrough;

    // Piano roll open: fitted roll height + one octave of headroom.
    if (edOpen)
    {
        const float row = juce::jmax(8.0f, rollEditor.currentRowHeight());
        const int rollFit = metrics::scaled(520);
        const int octave = juce::roundToInt(row * 12.0f);
        int h = transport + rollFit + octave;
        if (fxOpen)
            h = juce::jmax(h, transport + effectsInspector.contentHeight());
        return h;
    }

    if (pass && fxOpen && !recording)
        return transport + effectsInspector.contentHeight();

    if (recording)
        return juce::jmax(transport + metrics::scaled(520),
                          fxOpen ? transport + effectsInspector.contentHeight() : 0);

    if (leftPaneTab == 1)
    {
        int h = transport + metrics::paneHeaderH();
        h += effectsInspector.generatorColumnHeight();
        h += PreviewHeader::kHeaderH;
        if (previewOpen)
            h += juce::jmax(metrics::miniRollH(), metrics::scaled(80));
        if (fxOpen)
            h = juce::jmax(h, transport + effectsInspector.contentHeight());
        return h;
    }

    // Browser: stack of open containers (toolbar, list, preview fold, toolkit).
    int h = transport + metrics::paneHeaderH() + browserBar.idealHeight()
          + metrics::listHeaderH() + metrics::listRowH() * 6;
    h += PreviewHeader::kHeaderH;
    if (previewOpen)
        h += juce::jmax(metrics::miniRollH(), metrics::scaled(80));
    if (fxOpen)
        h = juce::jmax(h, transport + effectsInspector.contentHeight());
    return h;
}

int MidiToolkitEditor::passthroughWindowWidth() const
{
    // Effects-only: snap to the Effects column. Otherwise fit every open column.
    if (processorRef.passthrough && processorRef.effectsOpen && !recording)
        return metrics::effectsPaneWidth();

    const int rollW = processorRef.editorOpen ? metrics::editorPaneWidth() : 0;
    if (leftPaneTab == 1)
    {
        return metrics::effectsPaneWidth()
             + rollW
             + (processorRef.effectsOpen ? metrics::effectsPaneWidth() : 0);
    }

    const int side = sidebar.isCollapsed() ? metrics::sidebarRailWidth()
                                           : metrics::sidebarExpandedWidth();
    const int browser = juce::jmax(browserContentWidth(), metrics::scaled(260));
    return side + browser
         + rollW
         + (processorRef.effectsOpen ? metrics::effectsPaneWidth() : 0);
}

MidiToolkitEditor::LeftPaneTabs::LeftPaneTabs()
{
    auto prep = [](IconBtn& b)
    {
        b.ghost = false;
        b.iconScale = 1.0f;
        b.chipChrome = true;
        b.setWantsKeyboardFocus(true);
    };

    tabToggle.setComponentID("btnPaneTabToggle");
    tabToggle.setTooltip("Switch between Browser and Generator");
    tabToggle.onChange = [this](int i) { if (onTab) onTab(i); };
    addAndMakeVisible(tabToggle);

    prep(btnSearch);
    btnSearch.chipRadiusMd = true;
    btnSearch.setComponentID("btnBrowserSearch");
    btnSearch.setTooltip("Search");
    btnSearch.onClick = [this] { if (onToggleSearch) onToggleSearch(); };
    addAndMakeVisible(btnSearch);

    prep(btnFilters);
    btnFilters.chipRadiusMd = true;
    btnFilters.setComponentID("btnBrowserFilters");
    btnFilters.setTooltip("Filters");
    btnFilters.onClick = [this] { if (onToggleFilters) onToggleFilters(); };
    addAndMakeVisible(btnFilters);

    prep(btnGrid);
    btnGrid.chipRadiusMd = true;
    btnGrid.setComponentID("btnGridView");
    btnGrid.caption = "Grid";
    btnGrid.setTooltip("Switch to Grid View");
    btnGrid.onClick = [this] { if (onToggleGrid) onToggleGrid(); };
    addAndMakeVisible(btnGrid);

    setTab(0);
}

void MidiToolkitEditor::LeftPaneTabs::setTab(int t)
{
    tab = juce::jlimit(0, 1, t);
    tabToggle.setIndex(tab);
    const bool browser = tab == 0;
    btnGrid.setVisible(browser);
    btnSearch.setVisible(false);
    btnFilters.setVisible(browser);
    resized();
    repaint();
}

void MidiToolkitEditor::LeftPaneTabs::setGrid(bool on)
{
    btnGrid.active = on;
    btnGrid.repaint();
}

void MidiToolkitEditor::LeftPaneTabs::setSearchOpen(bool on)
{
    btnSearch.active = on;
    btnSearch.repaint();
}

void MidiToolkitEditor::LeftPaneTabs::setFiltersActive(bool open, int activeCount, const juce::String& summary)
{
    const bool applied = activeCount > 0;
    btnFilters.active = open || applied;
    // Applied filters → solid filled chip (no summary strip under the path).
    btnFilters.solid = applied;
    btnFilters.accentSolid = applied;
    if (!open && applied && summary.isNotEmpty())
        btnFilters.setTooltip(summary);
    else if (applied)
        btnFilters.setTooltip("Filters (" + juce::String(activeCount) + " active)");
    else
        btnFilters.setTooltip("Filters");
    btnFilters.repaint();
}

void MidiToolkitEditor::LeftPaneTabs::resized()
{
    auto row = getLocalBounds().reduced(metrics::scaled(8), 0);
    const int gap = metrics::scaled(4);
    // Same box as the transport chips (play, Drag Me, settings).
    const int toggleH = juce::jmin(row.getHeight(), metrics::chromeIconButton());
    const int toggleW = tabToggle.idealWidth();
    tabToggle.setBounds(row.removeFromLeft(toggleW)
                            .withSizeKeepingCentre(toggleW, toggleH));

    auto placeRight = [&](IconBtn& b, int w)
    {
        b.setBounds(row.removeFromRight(w).withSizeKeepingCentre(w, toggleH));
        row.removeFromRight(gap);
    };

    // Right cluster order (right → left): Filter, Search, Grid.
    if (btnFilters.isVisible())
        placeRight(btnFilters, toggleH);
    else
        btnFilters.setBounds({});

    if (btnSearch.isVisible())
        placeRight(btnSearch, toggleH);
    else
        btnSearch.setBounds({});

    if (btnGrid.isVisible())
        placeRight(btnGrid, btnGrid.captionWidth());
    else
        btnGrid.setBounds({});
}

void MidiToolkitEditor::LeftPaneTabs::paint(juce::Graphics& g)
{
    g.fillAll(ds::bg());
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
    applyWindowSize(false);
}

void MidiToolkitEditor::updateWindowLimits()
{
    applyWindowSize(false);
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

    const bool browserOnly = !pass && !recording && !edOpen && !fxOpen && leftPaneTab == 0;
    const bool wasBrowserOnly = layoutTracked && !trackedPassthrough && !trackedRecording
                             && !trackedEditor && !trackedEffects && trackedLeftPaneTab == 0;

    trackedBrowserContentW = browserContentWidth();
    trackedEditor = edOpen;
    trackedEffects = fxOpen;
    trackedSidebar = side;
    trackedPassthrough = pass;
    trackedRecording = recording;
    trackedLeftPaneTab = leftPaneTab;
    layoutTracked = true;
    layoutTargetW = 0;
    // Width is the last open column. Height locks to that layout, except the
    // browser-only view, which opens at the default roll and can shorten by half.
    applyWindowSize(browserOnly && !wasBrowserOnly);
    if (std::abs(getWidth() - columnWidth()) <= 1)
        resized();
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
    kindGrid.setKindMask(sidebar.listingFilter().kindMask);
    previewHeader.refresh();
    const int files = listedFileCount;
    const int barH = browserBar.idealHeight();
    juce::String folderTitle = fileList.getFolderName();
    juce::File folderForTitle = rootDir;
    if (!folderForTitle.isDirectory() && processorRef.lastBrowserDir.isNotEmpty())
        folderForTitle = juce::File(processorRef.lastBrowserDir);
    if (folderForTitle.isDirectory())
    {
        const auto parent = folderForTitle.getParentDirectory().getFileName();
        const auto leaf = folderForTitle.getFileName();
        if (parent.isNotEmpty() && leaf.isNotEmpty())
            folderTitle = "/" + parent + "/" + leaf + "/";
        else if (leaf.isNotEmpty())
            folderTitle = "/" + leaf + "/";
    }
    browserBar.setTitle(folderTitle);
    browserBar.setFolderPath(folderForTitle);
    const bool libraryFolder = folderForTitle.isDirectory()
        && processorRef.savedBrowserDirs.contains(folderForTitle.getFullPathName());
    browserBar.setLibraryFolder(libraryFolder);
    browserBar.setStarred(folderForTitle.isDirectory()
                          && processorRef.isStarredFolder(folderForTitle.getFullPathName()));
    const int total = juce::jmax(files, (int) clips.size());
    browserBar.setCount(files, total);
    if (leftPaneTab != 0 || processorRef.passthrough || recording)
        browserSearchOpen = false;
    leftPaneTabs.setSearchOpen(browserSearchOpen);
    leftPaneTabs.setFiltersActive(sidebar.isFilterOpen(), sidebar.activeFilterCount(),
                                  sidebar.describeActiveFilters());
    browserBar.setFiltersActive(sidebar.isFilterOpen(), sidebar.activeFilterCount(),
                                sidebar.describeActiveFilters());
    // The open filter owns its footer. Recounting here ran on every 30 Hz tick.
    if (!sidebar.isFilterOpen())
        filterPopover.setCountText(juce::String(files) + " of " + juce::String(total)
                                   + (total == 1 ? " file" : " files"));
    // Filter summary strip changes BrowserToolbar height — reflow only when it does.
    if (browserBar.isVisible() && browserBar.idealHeight() != barH)
        layoutContent();
}

void MidiToolkitEditor::layoutContent()
{
    auto r = content.getLocalBounds();
    const bool edOpen = processorRef.editorOpen;
    const bool fxOpen = processorRef.effectsOpen;
    // Mini preview lives under the file list and can open while the piano roll is up.
    const bool previewOpen = processorRef.previewOpen;
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
        sidebar.searchComponent().setVisible(false);
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
        sidebar.searchComponent().setVisible(false);
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
    // Fold strip always present when the full roll is closed (like Velocity).
    previewHeader.setVisible(!edOpen);
    rollEditor.setVisible(edOpen);
    effectsInspector.setVisible(showFx);
    leftPaneTabs.setVisible(true);
    generatorViewport.setVisible(!showFiles);

    auto row = r;
    const int colW = metrics::effectsPaneWidth();
    int sideW = 0;
    int leftColW = 0;
    // Columns keep their own width. Extra host space is not given to the piano
    // roll, and turning the toolkit off does not leave a blank column.
    const int fxW = showFx ? colW : 0;
    int editorW = edOpen ? metrics::editorPaneWidth() : 0;
    int browserW = 0;
    if (showFiles)
    {
        sideW = sidebar.idealWidth();
        browserW = juce::jmax(metrics::fileTableWidth(), browserContentWidth());
    }
    else
    {
        leftColW = colW;
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

        // Preview is removed entirely while the piano roll is open.
        if (!edOpen)
        {
            const int headerH = PreviewHeader::kHeaderH;
            const int desiredRollH = previewOpen ? juce::jmax(metrics::miniRollH(), metrics::scaled(80)) : 0;
            const int rollH = juce::jmin(desiredRollH, juce::jmax(0, left.getHeight() - headerH));
            if (rollH > 0)
                miniRoll.setBounds(left.removeFromBottom(rollH));
            else
                miniRoll.setBounds({});
            miniRoll.setVisible(rollH > 0);
            miniRoll.setShowVelocity(false);
            previewHeader.setBounds(left.removeFromBottom(headerH));
            previewHeader.setVisible(true);
            previewHeader.resized();
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
        syncPassthroughWindowHeight();
    }
    else
    {
        sidebar.setBounds(left.removeFromLeft(sideW));
        browserCol = left;
        auto& searchComp = sidebar.searchComponent();
        if (browserSearchOpen)
        {
            const int searchH = metrics::scaled(28);
            auto searchRow = browserCol.removeFromTop(searchH + metrics::scaled(8))
                                 .reduced(metrics::scaled(12), 0);
            // Vertically centre the field in the row (same pad above and below).
            searchComp.setBounds(searchRow.withSizeKeepingCentre(searchRow.getWidth(), searchH));
            searchComp.setVisible(true);
            searchComp.toFront(false);
        }
        else
        {
            searchComp.setBounds({});
            searchComp.setVisible(false);
        }
        browserBar.setBounds(browserCol.removeFromTop(juce::jmin(browserBar.idealHeight(), browserCol.getHeight())));
    }
    if (edOpen)
    {
        rollEditor.setBounds(paneX, paneY, editorW, paneH);
        browserRollSplitter.setVisible(false);
        paneX += editorW;
    }
    else
    {
        browserRollSplitter.setVisible(false);
    }
    if (showFx)
        effectsInspector.setBounds(paneX, paneY, fxW, paneH);

    // Mini preview under the file list. Hidden completely while the piano roll is open.
    if (showFiles)
    {
        if (!edOpen)
        {
            const int colH = browserCol.getHeight();
            const int headerH = PreviewHeader::kHeaderH;
            const int desiredRollH = previewOpen ? juce::jmax(metrics::miniRollH(), metrics::scaled(80)) : 0;
            const int rollH = juce::jmin(desiredRollH, juce::jmax(0, colH - headerH));
            if (rollH > 0)
                miniRoll.setBounds(browserCol.removeFromBottom(rollH));
            else
                miniRoll.setBounds({});
            miniRoll.setVisible(rollH > 0);
            miniRoll.setShowVelocity(false);
            previewHeader.setBounds(browserCol.removeFromBottom(headerH));
            previewHeader.setVisible(true);
            previewHeader.resized();
        }
        else
        {
            miniRoll.setBounds({});
            miniRoll.setVisible(false);
            previewHeader.setBounds({});
            previewHeader.setVisible(false);
        }

        fileList.setBounds(browserCol);
        kindGrid.setBounds(browserCol);
        fileList.setVisible(!gridView);
        kindGrid.setVisible(gridView);
        browserColW = fileList.getWidth();
    }
    else
    {
        sidebar.searchComponent().setBounds({});
        sidebar.searchComponent().setVisible(false);
    }

    if (showFiles && sidebar.isFilterOpen())
    {
        const int w = metrics::effectsPaneWidth();
        const int room = juce::jmax(120, content.getHeight() - metrics::scaled(16));
        const int chrome = metrics::scaled(40) + metrics::scaled(40);
        filterPopover.setMaxBodyHeight(juce::jmax(80, room - chrome));
        const int h = juce::jmin(filterPopover.idealHeight(), room);
        const int x = juce::jlimit(metrics::scaled(4),
                                   juce::jmax(metrics::scaled(4), content.getWidth() - w - metrics::scaled(4)),
                                   (content.getWidth() - w) / 2);
        const int y = juce::jlimit(metrics::scaled(4),
                                   juce::jmax(metrics::scaled(4), content.getHeight() - h - metrics::scaled(4)),
                                   (content.getHeight() - h) / 2);
        filterDismissShield.setBounds(content.getLocalBounds());
        filterDismissShield.setVisible(true);
        filterDismissShield.toFront(false);
        filterPopover.setBounds(x, y, w, h);
        filterPopover.setVisible(true);
        filterPopover.toFront(false);
    }
    else
    {
        filterDismissShield.setVisible(false);
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

    if (previewHeader.isVisible())
    {
        miniRoll.toFront(false);
        previewHeader.toFront(false);
    }
    // A relayout (chip edits call this) must not let the preview cover the filter.
    if (filterPopover.isVisible())
    {
        filterDismissShield.toFront(false);
        filterPopover.toFront(false);
    }
    // Column rules are painted by the file list. Keep Settings above them.
    if (auto* settings = content.findChildWithID("settingsOverlay"))
        settings->toFront(false);
}

void MidiToolkitEditor::paint(juce::Graphics& g)
{
    // Theme surface fills the whole editor — including any area past laid-out content
    // when the window is dragged taller than filledContentHeight().
    g.fillAll(ds::surface000());
}

// ── tweaks ───────────────────────────────────────────────────────────────────

namespace {

/** In-app settings — single scrolling Design column + folded Info. */
class SettingsPanel : public juce::Component
{
public:
    std::function<void()> onChanged;
    std::function<void()> onClose;
    std::function<void()> onSaveDefault;

    // Remapped so 100% == former 115% size (see textIconScale).
    static constexpr int kTextPctChoices[] = { 60, 75, 85, 100, 115 };
    static int textPctForUiSize(int sizeIdx)
    {
        // Small / Medium / Large → matching text-size step.
        static constexpr int map[] = { 75, 100, 115 };
        return map[juce::jlimit(0, 2, sizeIdx)];
    }
    static constexpr int kPanelW = 290;
    static constexpr int kHeaderH = 44;
    static constexpr int kRowH = ds::region::sidebarRowH;
    static constexpr int kRowGap = 0;
    static constexpr int kPadX = 16;

    static bool narrowWidth(int) { return true; } // always single column

    static int contentHeight(bool aboutTab)
    {
        if (aboutTab)
            return kHeaderH + 8 + 22 + 18 + 12; // version + stamp
        // Appearance … Note labels, then Save default view.
        const int rows = 12;
        return kHeaderH + 8 + rows * (kRowH + kRowGap) + 12;
    }

    /** Colour swatch that opens a ColourSelector callout. */
    class ColourSwatch : public juce::Component
    {
    public:
        std::function<void(juce::Colour)> onChange;
        juce::Colour colour { juce::Colours::grey };

        void setColour(juce::Colour c)
        {
            colour = c;
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(colour);
            g.fillRoundedRectangle(r, ds::radius::sm);
            g.setColour(ds::edge());
            g.drawRoundedRectangle(r.reduced(0.5f), ds::radius::sm, 1.0f);
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            auto* selector = new juce::ColourSelector(
                juce::ColourSelector::showColourAtTop
                    | juce::ColourSelector::showSliders
                    | juce::ColourSelector::showColourspace);
            selector->setName("colour");
            selector->setCurrentColour(colour);
            selector->setSize(280, 320);
            selector->addChangeListener(&listener);
            listener.owner = this;
            listener.selector = selector;
            juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(selector),
                                                   getScreenBounds(), nullptr);
        }

    private:
        struct Listener : juce::ChangeListener
        {
            ColourSwatch* owner = nullptr;
            juce::ColourSelector* selector = nullptr;
            void changeListenerCallback(juce::ChangeBroadcaster*) override
            {
                if (owner == nullptr || selector == nullptr) return;
                owner->setColour(selector->getCurrentColour());
                if (owner->onChange) owner->onChange(owner->colour);
            }
        } listener;
    };

    SettingsPanel()
    {
        appearancePopup.setItems({ "Auto", "Light", "Dark" },
                                 juce::jlimit(0, 2, tweaks().appearance.load()));
        refreshThemePopup();
        sizePopup.setItems({ "Small", "Medium", "Large" }, tweaks().size.load());
        fontPopup.setItems({ "Inter", "JetBrains Mono", "Archivo", "IBM Plex Mono",
                             "System", "Helvetica", "Avenir" },
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
        tooltipsSwitch.primaryWhenOn = true;
        alternateRowsSwitch.primaryWhenOn = true;
        noteLabelsSwitch.primaryWhenOn = true;
        tooltipsSwitch.setToggleState(tweaks().showTooltips.load() != 0, juce::dontSendNotification);
        alternateRowsSwitch.setToggleState(tweaks().alternateRows.load() != 0, juce::dontSendNotification);
        noteLabelsSwitch.setToggleState(tweaks().noteLabels.load() != 0, juce::dontSendNotification);

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
            refreshColourSwatches();
        });
        wire(themePopup, [this](int i)
        {
            const int idx = juce::jlimit(0, kNumThemesPerFamily - 1, i);
            if (settingsThemeFamilyIsDark())
                tweaks().darkTheme.store(idx);
            else
                tweaks().lightTheme.store(idx);
            // Changing theme clears custom overrides and reseeds pickers.
            tweaks().customPrimary.store(0);
            tweaks().customSecondary.store(0);
            refreshColourSwatches();
        });
        wire(sizePopup, [this](int i)
        {
            tweaks().size.store(juce::jlimit(0, 2, i));
            // Scaling the app size also updates the Text size dropdown.
            const int pct = textPctForUiSize(i);
            tweaks().textScalePct.store(pct);
            int pctIdx = 3;
            int best = 1000;
            for (int j = 0; j < (int) (sizeof(kTextPctChoices) / sizeof(kTextPctChoices[0])); ++j)
            {
                const int d = std::abs(kTextPctChoices[j] - pct);
                if (d < best) { best = d; pctIdx = j; }
            }
            textPopup.setItems({ "60%", "75%", "85%", "100%", "115%" }, pctIdx);
        });
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
        noteLabelsSwitch.onClick = [this]
        {
            tweaks().noteLabels.store(noteLabelsSwitch.getToggleState() ? 1 : 0);
            if (onChanged) onChanged();
        };
        addAndMakeVisible(noteLabelsSwitch);

        contrastSlider.setValue(juce::jlimit(50, 400, tweaks().contrastPct.load()), juce::dontSendNotification);
        contrastSlider.valueText = juce::String(contrastSlider.getValue()) + "%";
        contrastSlider.onChange = [this](int v)
        {
            tweaks().contrastPct.store(juce::jlimit(50, 400, v));
            contrastSlider.valueText = juce::String(v) + "%";
            if (onChanged) onChanged();
        };
        addAndMakeVisible(contrastSlider);

        primarySwatch.onChange = [this](juce::Colour c)
        {
            tweaks().customPrimary.store(c.getARGB());
            if (onChanged) onChanged();
        };
        secondarySwatch.onChange = [this](juce::Colour c)
        {
            tweaks().customSecondary.store(c.getARGB());
            if (onChanged) onChanged();
        };
        addAndMakeVisible(primarySwatch);
        addAndMakeVisible(secondarySwatch);
        refreshColourSwatches();

        closeBtn.ghost = true;
        closeBtn.iconScale = 0.95f;
        closeBtn.setTooltip("Close");
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible(closeBtn);

        saveDefaultBtn.caption = "Save default view";
        saveDefaultBtn.solid = true;
        saveDefaultBtn.setTooltip("Remember this layout, size, and columns as the default view");
        saveDefaultBtn.onClick = [this] { if (onSaveDefault) onSaveDefault(); };
        addAndMakeVisible(saveDefaultBtn);

        setSize(kPanelW, contentHeight(aboutTab));
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

    void refreshColourSwatches()
    {
        const auto& spec = themeAt(settingsThemeFamilyIndex());
        juce::Colour prim = (tweaks().customPrimary.load() != 0)
                                ? juce::Colour(tweaks().customPrimary.load())
                                : (spec.primary != 0 ? juce::Colour(spec.primary)
                                                     : ds::primary());
        juce::Colour sec = (tweaks().customSecondary.load() != 0)
                               ? juce::Colour(tweaks().customSecondary.load())
                               : (spec.accent != 0 ? juce::Colour(spec.accent) : ds::acc());
        primarySwatch.setColour(prim);
        secondarySwatch.setColour(sec);
    }

    void setAboutTab(bool on)
    {
        if (aboutTab == on) return;
        aboutTab = on;
        setSize(getWidth(), contentHeight(aboutTab));
        resized();
        repaint();
        if (auto* parent = getParentComponent())
            parent->resized();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(ds::surface100());
        g.setColour(ds::surface100());
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(ds::edge());
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

        // Header tabs: SETTINGS | ABOUT — keep the existing 11pt semibold style.
        auto header = getLocalBounds().removeFromTop(kHeaderH).reduced(kPadX, 0);
        header.removeFromRight(28);
        const auto tabFont = uiFont(11.0f, true);
        g.setFont(tabFont);
        const juce::String settingsWord = "SETTINGS";
        const juce::String aboutWord = "ABOUT";
        const int settingsW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(tabFont, settingsWord)) + 2;
        const int aboutW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(tabFont, aboutWord)) + 2;
        settingsTabBounds = header.removeFromLeft(settingsW);
        header.removeFromLeft(14);
        aboutTabBounds = header.removeFromLeft(aboutW);

        auto paintTab = [&](juce::Rectangle<int> r, const juce::String& word, bool active)
        {
            g.setColour(active ? ds::acc() : ds::inkMuted());
            g.setFont(tabFont);
            g.drawText(word, r, juce::Justification::centredLeft, false);
            if (!active)
                return;
            // Sit the rule just under the glyphs, not on the header floor.
            const int textH = juce::roundToInt(std::ceil(tabFont.getHeight()));
            const int textTop = r.getCentreY() - textH / 2;
            const int underlineY = textTop + juce::roundToInt(std::ceil(tabFont.getAscent())) + 1;
            g.setColour(ds::acc());
            g.fillRect(r.getX(), underlineY, r.getWidth(), 1);
        };
        paintTab(settingsTabBounds, settingsWord, !aboutTab);
        paintTab(aboutTabBounds, aboutWord, aboutTab);

        g.setColour(ds::line());
        g.fillRect(kPadX, kHeaderH, getWidth() - kPadX * 2, 1);

        auto body = getLocalBounds().withTrimmedTop(kHeaderH + 8).reduced(kPadX, 6);
        if (aboutTab)
        {
            g.setFont(uiFontFixed(14.0f, true));
            g.setColour(ds::ink());
            g.drawText(build_info::version(), body.removeFromTop(22),
                       juce::Justification::centredLeft, true);
            g.setFont(uiFontFixed(11.0f, false));
            g.setColour(ds::inkMuted());
            g.drawText(juce::String(build_info::buildTime()) + "  ·  " + build_info::gitHash(),
                       body.removeFromTop(18), juce::Justification::centredLeft, true);
            return;
        }

        // Match filter-window row labels (sidebarLabelStyle / fixed 12pt).
        const char* labels[] = {
            "Appearance", "Theme", "Primary colour", "Secondary colour", "Contrast",
            "UI size", "Text size", "Font", "Show tooltips", "Alternate File Rows", "Note labels"
        };
        for (auto* label : labels)
        {
            auto row = body.removeFromTop(kRowH);
            g.setColour(ds::ink());
            g.setFont(uiFontFixed(12.0f, false));
            g.drawText(label, row, juce::Justification::centredLeft, false);
            body.removeFromTop(kRowGap);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (settingsTabBounds.expanded(4, 6).contains(e.getPosition()))
            setAboutTab(false);
        else if (aboutTabBounds.expanded(4, 6).contains(e.getPosition()))
            setAboutTab(true);
    }

    void resized() override
    {
        const int iconBtn = metrics::chromeIconButton();
        closeBtn.setBounds(juce::Rectangle<int>(getWidth() - kPadX - iconBtn, 0, iconBtn, kHeaderH)
                               .withSizeKeepingCentre(iconBtn, iconBtn));

        auto body = getLocalBounds().withTrimmedTop(kHeaderH + 8).reduced(kPadX, 6);
        const int ctrlH = metrics::scaled(22);
        const bool showSettings = !aboutTab;

        appearancePopup.setVisible(showSettings);
        themePopup.setVisible(showSettings);
        sizePopup.setVisible(showSettings);
        textPopup.setVisible(showSettings);
        fontPopup.setVisible(showSettings);
        tooltipsSwitch.setVisible(showSettings);
        alternateRowsSwitch.setVisible(showSettings);
        noteLabelsSwitch.setVisible(showSettings);
        contrastSlider.setVisible(showSettings);
        primarySwatch.setVisible(showSettings);
        secondarySwatch.setVisible(showSettings);

        saveDefaultBtn.setVisible(showSettings);
        if (aboutTab)
        {
            saveDefaultBtn.setBounds({});
            return;
        }

        auto placePopup = [&](fx::FlatPopup& p)
        {
            auto row = body.removeFromTop(kRowH);
            const int w = juce::jmin(juce::jmax(p.idealWidth(), 110), row.getWidth() / 2);
            p.setBounds(row.removeFromRight(w).withSizeKeepingCentre(w, ctrlH));
            body.removeFromTop(kRowGap);
        };
        auto placeSwitch = [&](fx::FlatSwitch& s)
        {
            auto row = body.removeFromTop(kRowH);
            const int w = s.idealWidth();
            s.setBounds(row.removeFromRight(w).withSizeKeepingCentre(w, 16));
            body.removeFromTop(kRowGap);
        };
        auto placeSwatch = [&](ColourSwatch& sw)
        {
            auto row = body.removeFromTop(kRowH);
            sw.setBounds(row.removeFromRight(28).withSizeKeepingCentre(28, 22));
            body.removeFromTop(kRowGap);
        };

        placePopup(appearancePopup);
        placePopup(themePopup);
        placeSwatch(primarySwatch);
        placeSwatch(secondarySwatch);
        {
            auto row = body.removeFromTop(kRowH);
            const int w = juce::jmin(160, row.getWidth() / 2);
            contrastSlider.setBounds(row.removeFromRight(w).withSizeKeepingCentre(w, ctrlH + 4));
            body.removeFromTop(kRowGap);
        }
        placePopup(sizePopup);
        placePopup(textPopup);
        placePopup(fontPopup);
        placeSwitch(tooltipsSwitch);
        placeSwitch(alternateRowsSwitch);
        placeSwitch(noteLabelsSwitch);
        {
            body.removeFromTop(metrics::scaled(8));
            auto row = body.removeFromTop(kRowH);
            const int chipH = metrics::chromeIconButton();
            const int saveW = juce::jmin(saveDefaultBtn.captionWidth(), row.getWidth());
            saveDefaultBtn.setBounds(row.removeFromRight(saveW).withSizeKeepingCentre(saveW, chipH));
            body.removeFromTop(metrics::scaled(8));
        }
    }

private:
    bool aboutTab = false;
    juce::Rectangle<int> settingsTabBounds, aboutTabBounds;
    fx::FlatPopup appearancePopup, themePopup, sizePopup, textPopup, fontPopup;
    fx::FlatSwitch tooltipsSwitch;
    fx::FlatSwitch alternateRowsSwitch;
    fx::FlatSwitch noteLabelsSwitch;
    fx::FlatSliderRow contrastSlider { "", 50, 400, 100, false };
    ColourSwatch primarySwatch, secondarySwatch;
    IconBtn closeBtn { icons::x, "Close" };
    IconBtn saveDefaultBtn { icons::save, "Save as default" };
};

} // namespace

void MidiToolkitEditor::showTweaksMenu()
{
    if (auto* existing = content.findChildWithID("settingsOverlay"))
    {
        content.removeChildComponent(existing);
        delete existing;
        // Restore the height from before Settings forced a taller window.
        if (heightBeforeSettings > 0)
        {
            setSize(getWidth(), heightBeforeSettings);
            heightBeforeSettings = 0;
            updateWindowLimits();
        }
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
            view.setOpaque(true);
            addAndMakeVisible(view);
        }

        void resized() override
        {
            const int margin = 12;
            const int minH = 200;
            const int sidePad = metrics::scaled(20);
            const int w = juce::jmin(SettingsPanel::kPanelW,
                                     juce::jmax(metrics::scaled(180), getWidth() - sidePad * 2));
            const int availH = juce::jmax(minH, getHeight() - margin * 2);
            const int contentH = panel.getHeight() > 0 ? panel.getHeight()
                                                       : SettingsPanel::contentHeight(false);
            const int h = juce::jmin(contentH, availH);
            view.setBounds(getLocalBounds().withSizeKeepingCentre(w, h));
            view.setScrollBarsShown(contentH > h, false);
            panel.setSize(w, contentH);
        }

        void paint(juce::Graphics& g) override
        {
            g.setColour(juce::Colours::black.withAlpha(0.46f));
            g.fillAll();
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (!view.getBounds().contains(e.getPosition()) && onDismiss)
                onDismiss();
        }
    };

    // Grow the window so Settings fits, then restore that height on close.
    heightBeforeSettings = getHeight();
    {
        const int needed = SettingsPanel::contentHeight(false) + metrics::transportH() + 24;
        auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* display = displays.getDisplayForRect(getScreenBounds());
        if (display == nullptr)
            display = displays.getPrimaryDisplay();
        const int screenCap = display != nullptr ? juce::jmax(needed, display->userArea.getHeight() - 48)
                                                 : juce::jmax(needed, 1200);
        if (getHeight() < needed)
            setSize(getWidth(), juce::jmin(needed, screenCap));
    }

    auto* overlay = new Overlay();
    overlay->setComponentID("settingsOverlay");
        overlay->panel.onChanged = [this]
        {
            persistBrowserSession();
            lnf.refreshColours();
            sendLookAndFeelChange();
            rollEditor.noteColour = ds::note();
            miniRoll.setNoteColour(ds::note());
            applyLayoutState();
            resized();
            fileList.repaintRows();
            repaint();
        };
    overlay->onDismiss = [this, overlay]
    {
        content.removeChildComponent(overlay);
        delete overlay;
        if (heightBeforeSettings > 0)
        {
            setSize(getWidth(), heightBeforeSettings);
            heightBeforeSettings = 0;
            updateWindowLimits();
        }
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
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
    {
        if (dynamic_cast<juce::TextEditor*>(focused) != nullptr && isParentOf(focused))
        {
            if (focused->keyPressed(key))
                return true;
        }
        if (auto* popup = dynamic_cast<fx::FlatPopup*>(focused))
            if (isParentOf(popup) && popup->keyPressed(key))
                return true;
    }
    if (auto* popup = fx::FlatPopup::armed())
    {
        const bool step = key == juce::KeyPress::upKey || key == juce::KeyPress::downKey;
        if (step && popup->isShowing() && isParentOf(popup) && popup->keyPressed(key))
            return true;
    }

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
            if (rollEditor.isVisible() && rollEditor.hasSelection())
            {
                if (rollEditor.keyPressed(key))
                    return true;
            }
            // Consume arrows in the editor so they don't cycle focus elsewhere.
            return true;
        }
        if (keyNavTarget == KeyNavTarget::Effects)
            return true;   // effects are mouse-driven; don't steal browser rows
        if (keyNavTarget == KeyNavTarget::Sidebar)
            return sidebar.keyPressed(key);

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
    fx::FlatPopup::clearArmed();
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
    else if (target == KeyNavTarget::Sidebar)
        sidebar.grabKeyboardFocus();
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
    if (filterApplyActive)
        pumpFilterApply();
    else if (filterCountActive)
        pumpFilterCount();

    syncTooltipHighlight();
    refreshBrowserBar();

    if (sessionDirty
        && juce::Time::getMillisecondCounterHiRes() - sessionDirtyAtMs > 400.0)
        flushBrowserSession();

    // Host browser-index automation that arrived before the list (or recording) was ready.
    if (const int pending = processorRef.pendingBrowserIndex.load(); pending >= 0)
    {
        if (!recording && !sidebarScanning && selectFilePosition(pending))
            processorRef.pendingBrowserIndex.store(-1);
    }
    else
        publishBrowserIndexToHost();

    if (layoutTargetW > 0)
        layoutTargetW = 0;

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
        // Half/Double retimes notes via stretch; playhead advances at host/free rate.
        const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        const double lenBeats = juce::jmax(0.25, (double) (resolved.bars * kStepsPerBar) / 4.0 * stretch);
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
            const double hostBeat = processorRef.hostBeatPos.load();
            beat = loopStart + std::fmod(std::fmod(hostBeat, loopLen) + loopLen, loopLen);
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
