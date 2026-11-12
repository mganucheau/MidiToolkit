#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "TransportBar.h"
#include "BrowserPanels.h"
#include "PianoRollEditor.h"
#include "EffectsInspector.h"
#include "Theme.h"
#include "ClipNoteCache.h"

namespace pflow {

// Cupertino shell: toolbar / [sidebar | file table + preview | editor? | effects?]

class MidiToolkitEditor : public juce::AudioProcessorEditor,
                          public juce::Timer,
                          public juce::DragAndDropContainer,
                          private juce::DarkModeSettingListener
{
public:
    explicit MidiToolkitEditor(MidiToolkitProcessor&);
    ~MidiToolkitEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress&) override;
    void darkModeSettingChanged() override;

    void setRootDirectory(const juce::File& dir, bool keepSelection = false,
                          bool scanSubdirs = false);
    /** Rows currently shown in the file browser (folders + MIDI files). */
    int browserEntryCount() const { return fileList.getNumEntries(); }
    /** Parse one MIDI file for preview without a full folder Scan. */
    void ensureAndSelectFile(const juce::File& file);
    void toggleEditorFold();

    // Test / snapshot hooks.
    void toggleEffectsForTesting() { toggleEffectsFold(); }
    void setGridViewForTesting(bool on) { if (gridView != on && browserBar.onToggleGrid) browserBar.onToggleGrid(); }
    void setFilterOpenForTesting(bool on) { sidebar.setFilterOpen(on); }
    void setSidebarCollapsedForTesting(bool on) { sidebar.setCollapsed(on); }
    void setPassthroughForTesting(bool on) { if (processorRef.passthrough != on) togglePassthrough(); }

private:
    void applyNativeWindowChrome();
    /** Async folder scan — never parses MIDI on the message thread. */
    void rescanFolder(bool keepSelection, const juce::String& preferredPath = {});
    /** Load every MIDI file under the current folder tree (flat list). */
    void scanAllFolders();
    void chooseFolder();
    void rebuildEntries();
    /** Load notes for an indexed clip (metadata-only) before preview/play. */
    bool ensureClipNotesLoaded(int clipIndex);
    /** Prefer a valid on-disk folder index; fall back to name listing.
        selectPath restores the previous file. rescanIfMissing parses when the index is stale. */
    void refreshBrowseListingAsync(const juce::String& selectPath = {},
                                   bool rescanIfMissing = false);
    void handleTrimClicked();
    /** Refilter the already-loaded clip list (no disk rescan). */
    void applyBrowserFilter();
    int displayForClip(int clipIdx) const;
    void selectIndex(int index);
    void enterFolderAtDisplay(int displayIdx);
    void enterParentFolder();
    void applyTimeStretchFromMultiplier();
    void refreshEntryMeta(int index);
    void updateMiniPreview();
    MidiClip buildRenderedClip() const;
    void pushPreviewToProcessor();
    void pushLiveFxToProcessor();
    void applyPassthroughUi();
    void setLeftPaneTab(int tab);
    void togglePassthrough();
    void toggleRecord();
    void refreshRecordingRoll(const RecordingSnapshot& snap);
    void finishRecording(const RecordingSnapshot& snap);
    void startDragExport();
    void startDragOriginalFile(const juce::File& file);
    void copyFileToFolder(const juce::File& file);
    void copyStarredToFolder();
    void copyRenderedClipToFolder();
    void applyLayoutState();
    void updateWindowLimits();
    void layoutContent();
    /** Push folder / count / subfolder / saved / filter state into the browser toolbar. */
    void refreshBrowserBar();
    void applySavedDefaultView();
    void saveCurrentViewAsDefault();
    void toggleEffectsFold();
    /** Dim veil over file browser + piano UI while Passthrough is on. */
    class PassthroughVeil : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override
        {
            g.setColour(colours::panel().withMultipliedBrightness(0.72f));
            g.fillRect(getLocalBounds());
            g.setColour(colours::text().withAlpha(0.18f));
            g.fillRect(getLocalBounds());
        }
        void mouseDown(const juce::MouseEvent&) override {}
        void mouseDrag(const juce::MouseEvent&) override {}
        void mouseUp(const juce::MouseEvent&) override {}
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}
    };
    PassthroughVeil passthroughVeil;

    /** Click-catcher behind the filter popover — dismisses without applying. */
    class FilterDismissShield : public juce::Component
    {
    public:
        std::function<void()> onDismiss;
        void paint(juce::Graphics&) override {}
        void mouseDown(const juce::MouseEvent&) override
        {
            if (onDismiss) onDismiss();
        }
    };
    FilterDismissShield filterDismissShield;

    /** Drag handle between the file browser and the piano roll. */
    class PaneSplitter : public juce::Component
    {
    public:
        std::function<void()> onDragStart;
        std::function<void(int dx)> onDrag;

        void paint(juce::Graphics& g) override
        {
            g.setColour(colours::line());
            g.fillRect(getLocalBounds().withSizeKeepingCentre(1, getHeight()));
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            dragOriginX = e.getScreenX();
            if (onDragStart) onDragStart();
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (onDrag) onDrag(e.getScreenX() - dragOriginX);
        }
        juce::MouseCursor getMouseCursor() override
        {
            return juce::MouseCursor::LeftRightResizeCursor;
        }
    private:
        int dragOriginX = 0;
    };
    PaneSplitter browserRollSplitter;
    void showTweaksMenu();
    void refreshSidebar();
    void syncEffectsInspector();
    void setBrowseMode(int mode);
    void loadStarredClips(const juce::String& preferredPath = {});
    void loadStarredFolders();
    /** Mirror browser UI into the processor (survives editor teardown / host save). */
    void persistBrowserSession();
    /** Write the shared session. Debounced so arrow-key browsing does not hit disk. */
    void flushBrowserSession();
    /** Preview/roll update once `clips[selectedIdx]` has resident notes. */
    void finishClipSelection();
    /** Copy locked section snapshots onto the selected clip.
        resetUnlockedGroove drops unlocked groove tweaks so browsing plays the
        file itself except for sections that are locked. */
    void stampLockedSections(bool resetUnlockedGroove);
    void captureLocksFromEdit(const ClipEdit& previous, const ClipEdit& next, const StepClip* clip);
    void loadClipNotesAsync(int clipIndex, bool applyWhenReady);
    void prefetchNearbyClips(int clipIndex);
    /** Rebuild folder/search/filters/selection from processor session state. */
    void restoreBrowserSession();
    void selectPathOrFirst(const juce::String& path);
    /** Cancel in-flight scans. hideList covers the file list with the scan overlay. */
    int beginBackgroundClipLoad(bool hideList = true);
    /** Hydrate clips from cached paths using FolderIndex metadata (no MIDI parse).
        Notes are still loaded lazily on select via ensureClipNotesLoaded. */
    void loadClipsFromPathsAsync(juce::StringArray paths,
                                 std::function<void(std::vector<StepClip>&&)> onDone,
                                 juce::File indexRoot = {});
    void runSearch(const BrowserSearch& criteria);
    /** savedIdx >= 0 keeps that saved-search selection and stores/restores its result snapshot. */
    void runSearchAsync(const BrowserSearch& criteria, int savedIdx = -1,
                        juce::File searchRoot = {}, bool allowCache = true);
    void saveCurrentSearch();
    bool clipMatchesSearch(const StepClip& clip, const juce::File& file,
                           const BrowserSearch& criteria) const;
    void applySearchSnapshot(std::vector<StepClip> found,
                             std::map<juce::String, juce::StringArray> locMap,
                             const BrowserSearch& criteria, int savedIdx,
                             const juce::String& rootPath, bool showSearching);

    enum class KeyNavTarget { Browser, Editor, Effects };
    void claimKeyNav(KeyNavTarget target);

    const StepClip* selectedClip() const;
    ClipEdit selectedEdit() const;
    GrooveParams selectedGroove() const;
    /** Note ids for Form → Selected notes only; nullptr when the option is off / empty. */
    const std::set<int>* formSelectionIds() const;

    MidiToolkitProcessor& processorRef;
    PatternFlowLookAndFeel lnf;

    /** Hides tips when Settings → Show Tooltips is off; styled via LookAndFeel. */
    struct AppTooltipWindow : juce::TooltipWindow
    {
        AppTooltipWindow(juce::Component* parent, int delayMs)
            : juce::TooltipWindow(parent, delayMs)
        {
            setOpaque(false);
        }

        juce::String getTipFor(juce::Component& c) override
        {
            if (tweaks().showTooltips.load() == 0)
                return {};
            return juce::TooltipWindow::getTipFor(c);
        }
    };
    AppTooltipWindow tooltips { this, 450 };
    void syncTooltipHighlight();

    struct ContentHolder : juce::Component
    {
        std::function<void()> onLayout;
        void resized() override { if (onLayout) onLayout(); }
        void paint(juce::Graphics& g) override { g.fillAll(ds::surface000()); }
    };
    ContentHolder content;

    TransportBar transport;

    /** Files | Generator toggles for the left column (fold lives in the sidebar). */
    class LeftPaneTabs : public juce::Component
    {
    public:
        LeftPaneTabs();
        std::function<void(int)> onTab;
        std::function<void()> onTogglePreview;
        std::function<void()> onToggleGrid;
        void setTab(int t);
        void setPreviewOpen(bool on);
        void setGrid(bool on);
        void resized() override;
        void paint(juce::Graphics&) override;
    private:
        int tab = 0;
        SegmentToggle tabToggle { "Browser", "Generator" };
        IconBtn btnGrid { icons::grid, "Grid" };
        IconBtn btnPreview { icons::noteKeys, "Preview" };
    };

    FavoritesSidebar sidebar;
    BrowserToolbar browserBar;
    FilterPopover filterPopover;
    FileListPanel fileList;
    PianoRollMini miniRoll;
    PianoRollEditor rollEditor;
    EffectsInspector effectsInspector;
    LeftPaneTabs leftPaneTabs;
    juce::Viewport generatorViewport;
    juce::Component generatorBody;
    /** 0 = file browser, 1 = generators. */
    int leftPaneTab = 0;

    /** Groove preview fold strip (like Velocity): chevron + title; body opens below. */
    class PreviewHeader : public juce::Component
    {
    public:
        explicit PreviewHeader(MidiToolkitEditor& o);
        void paint(juce::Graphics&) override;
        void resized() override;
        void refresh();
        void mouseDown(const juce::MouseEvent&) override;
        MidiToolkitEditor& owner;

        struct DragBtn : ChipBtn
        {
            using ChipBtn::ChipBtn;
            std::function<void()> onDragStart;
            bool started = false;
            void mouseDrag(const juce::MouseEvent& e) override
            {
                if (!started && e.getDistanceFromDragStart() > 4 && onDragStart)
                {
                    started = true;
                    onDragStart();
                }
            }
            void mouseUp(const juce::MouseEvent& e) override
            {
                started = false;
                ChipBtn::mouseUp(e);
            }
        };
        ChipBtn btnVel { "Vel" };
        ChipBtn btnTrim { "Trim" };
        ChipBtn btnMute { "Mute" };
        DragBtn btnDrag { "Drag", icons::dragOut };
        juce::String name, meta;
        static constexpr int kHeaderH = 22;
    };
    PreviewHeader previewHeader { *this };
    KindGridView kindGrid;
    bool gridView = false;
    bool showPreviewVelocity = false;
    bool previewMuted = false;
    int browserContentWidth() const;

    juce::File rootDir;
    std::vector<StepClip> clips;
    /** Immediate child folders for browse mode — filled async, never on the UI thread. */
    std::vector<juce::File> cachedSubdirs;
    /** Immediate MIDI files (names only) until Scan parses metadata. */
    std::vector<juce::File> cachedMidiFiles;
    /** True after Scan has parsed this folder's MIDI into `clips`. */
    bool folderScanned = false;
    /** True after "Scan all folders" — flat recursive MIDI list, no subfolder rows. */
    bool recursiveBrowse = false;
    struct DisplayRow
    {
        bool isDirectory = false;
        int clipIndex = -1;
        juce::File file;
    };
    std::vector<DisplayRow> displayRows;
    int browseMode = 0;   // 0 folder, 1 starred files, 2 search, 3 starred folders
    bool starredFilter = false;
    BrowserSearch activeSearch;
    int activeSavedSearchIdx = -1;
    /** Bumps on each search so stale async completions are ignored. */
    std::atomic<int> searchGeneration { 0 };
    /** Bumps on each async folder Update so stale completions are ignored. */
    std::atomic<int> folderScanGeneration { 0 };
    /** Bumps when browsing to a new folder so stale subdir lists are ignored. */
    std::atomic<int> browseListGeneration { 0 };
    /** Bumps when the selected clip changes so a stale note parse is ignored. */
    std::atomic<int> noteLoadGeneration { 0 };
    int browseDirection = 1;
    ClipNoteCache noteCache;
    ClipEdit pendingCarriedScale;
    bool havePendingCarriedScale = false;
    bool sessionDirty = false;
    double sessionDirtyAtMs = 0.0;
    bool sidebarScanning = false;
    /** Primary clip path -> all Finder locations (including primary) after dedupe. */
    std::map<juce::String, juce::StringArray> searchDuplicateLocations;

    /** In-session full results so re-picking a saved search is instant (no rescan). */
    struct SearchSnapshot
    {
        BrowserSearch criteria;
        juce::String rootPath;
        std::vector<StepClip> clips;
        std::map<juce::String, juce::StringArray> locations;
    };
    SearchSnapshot lastSearchSnapshot;
    std::map<int, SearchSnapshot> savedSearchSnapshots;
    void rememberSearchSnapshot(int savedIdx, SearchSnapshot snap);
    void forgetSavedSearchSnapshot(int index);
    int selectedIdx = -1;
    KeyNavTarget keyNavTarget = KeyNavTarget::Browser;
    int lastWindowH = 700;
    /** Height after the first layout. The window cannot shrink below this. */
    int openedWindowH = 0;
    int trackedBrowserContentW = -1;
    int chromeApplyAttempts = 0;
    void* spaceToHostMonitor = nullptr;
    int browserColW = metrics::fileTableW;
    /** User-dragged browser width while the piano roll is open. 0 keeps the default split. */
    int browserPaneW = 0;
    int splitterDragOrigin = 0;
    int splitterDragSpan = 0;
    int layoutTargetW = 0;
    int layoutAnimFromW = 0;
    bool layoutTracked = false;
    bool trackedEditor = false;
    bool trackedEffects = false;
    bool trackedSidebar = false;
    bool trackedPassthrough = false;
    int trackedLeftPaneTab = 0;
    bool recording = false;
    bool hasRecording = false;
    bool trackedRecording = false;
    int savedNonRecordW = 0;
    StepClip recordedClip;
    /** Part index from the last Generate; -1 = take/import (do not live-regenerate). */
    int lastGeneratedPart = -1;
    void regenerateGeneratedClip(const GrooveParams& groove);
    int savedNonPassW = 0;
    int savedNonPassH = 0;
    int passthroughWindowHeight() const;
    int passthroughDefaultHeight() const;
    int passthroughMinHeight() const;
    int passthroughMaxHeight() const;
    /** Through mode fits the file browser, the Toolkit, and the roll when it is open. */
    int passthroughWindowWidth() const;
    /** Tallest useful window height for the current layout (no empty void). */
    int filledContentHeight() const;
    bool recordFromThrough = false;
    void syncPassthroughWindowHeight();
    /** User-dragged passthrough height above the default fold. 0 = use default. */
    int userPassthroughH = 0;
    double layoutAnimStartMs = 0.0;
    static constexpr double kLayoutAnimMs = 220.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiToolkitEditor)
};

} // namespace pflow
