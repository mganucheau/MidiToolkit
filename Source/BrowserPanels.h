#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "EditModel.h"
#include "UiAtoms.h"
#include "EffectsInspectorWidgets.h"

namespace pflow {

struct BrowserSearch
{
    juce::String query;
    double bpmMin = 0.0; // 0 = no lower bound
    double bpmMax = 0.0; // 0 = no upper bound
    int keyRoot = -1;    // legacy single key; -1 = any (used when keyMask == 0)
    /** Bit i set = pitch-class i selected. 0 = any key. */
    uint16_t keyMask = 0;
    int barsMin = 0;     // 0 = any; 64 = 64+ (≥64)
    int barsMax = 0;     // 0 = any; 64 = 64+ (unlimited upper)
    int complexityMin = 0; // 0 = no lower bound
    int complexityMax = 0; // 0 = no upper bound
    /** Bit i set = ClipKind i selected. 0 = any kind. */
    uint8_t kindMask = 0;
    /** Bit i matches kFilterTimeSigs[i]. 0 = any time signature. */
    uint16_t timeSigMask = 0;
    bool subdirs = true;
    /** Collapse content-identical hits from multiple folders into one row. */
    bool removeDuplicates = true;
};

/** A folder kept in the Library list, with the filters and scan scope used when it was saved. */
struct SavedLibraryFolder
{
    juce::String path;
    /** True when this library was saved while scanning subdirectories (flat file list). */
    bool scanSubdirs = false;
    BrowserSearch filter;
};

struct TimeSigOption
{
    int num;
    int den;
    const char* label;
};

inline constexpr TimeSigOption kFilterTimeSigs[] = {
    { 4, 4, "4/4" }, { 3, 4, "3/4" }, { 2, 4, "2/4" }, { 6, 8, "6/8" },
    { 5, 4, "5/4" }, { 7, 8, "7/8" }, { 12, 8, "12/8" }, { 9, 8, "9/8" },
};
inline constexpr int kNumFilterTimeSigs = (int) (sizeof(kFilterTimeSigs) / sizeof(kFilterTimeSigs[0]));

inline int timeSigFilterBit(int num, int den)
{
    for (int i = 0; i < kNumFilterTimeSigs; ++i)
        if (kFilterTimeSigs[i].num == num && kFilterTimeSigs[i].den == den)
            return i;
    return -1;
}

struct SavedSearchEntry
{
    juce::String name;
    BrowserSearch search;
    /** Folder the search was run against — used to restore the path cache. */
    juce::String rootPath;
    /** Matching file paths captured when the search was saved / last run.
        Lets saved searches reopen instantly across sessions without rescanning. */
    juce::StringArray resultPaths;
};

// Sidebar: joined search → LIBRARY (+ opens a folder) → SAVED SEARCHES.

class FavoritesSidebar : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    FavoritesSidebar();

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;

    void setSavedDirs(const juce::StringArray& paths, const juce::String& activePath);
    void setSavedSearches(const std::vector<SavedSearchEntry>& searches, int activeSearchIdx);
    void setBrowseMode(int mode); // 0 folder, 1 starred, 2 search
    void setStarredFilter(bool on);
    bool isStarredFilter() const { return starredFilter; }
    bool isCollapsed() const { return collapsed; }
    void setCollapsed(bool shouldCollapse);
    int idealWidth() const;
    int getExpandedWidth() const { return expandedWidth; }
    void setExpandedWidth(int w);
    int idealMinHeight() const;

    /** Combined criteria (query + filter) for saved searches / Enter. */
    BrowserSearch getCriteria() const;
    /** Filter fields only (key/bpm/bars/complexity/dedupe) — no query. */
    BrowserSearch getFilter() const;
    void setCriteria(const BrowserSearch&);

    std::function<void()> onOpenFolder;
    std::function<void(const juce::String&)> onPickDir;
    std::function<void(const juce::String&)> onRemoveDir;
    /** Keep / un-keep current folder in Library. */
    std::function<void()> onAddCurrent;
    /** Rescan the current folder without changing subdirectory scope. */
    std::function<void()> onRefreshFolder;
    /** Scan this folder and its subdirectories (flat list, no folder rows). */
    std::function<void()> onScanSubdirs;
    void setIncludeSubdirs(bool on);
    bool getIncludeSubdirs() const { return includeSubdirs; }
    void setCurrentClipCount(int n);
    void setStarredCount(int n);
    void setScanning(bool on);
    /** True after a Scan (or a valid folder index) so the action reads Update. */
    void setScanFinished(bool finished);
    /** Optional "42/200" progress while a folder scan runs. */
    void setScanProgress(int done, int total);

    /** Refresh Filter histogram bars from loaded browser clips. */
    void setFilterHistograms(const std::vector<StepClip>& clips);
    std::function<void()> onShowStarred;
    std::function<void()> onShowSearch;
    /** Text search (Enter / saved search) — scans disk by query. */
    std::function<void(const BrowserSearch&)> onRunSearch;
    /** Filter controls changed — refilter already-loaded clips (no rescan). */
    std::function<void()> onFilterChanged;
    std::function<void(int)> onPickSavedSearch;
    std::function<void(int)> onRemoveSavedSearch;
    std::function<void()> onSaveCurrentSearch;
    std::function<void()> onCopyStarredToFolder;
    std::function<void()> onCollapsedChanged;
    std::function<void()> onOpenSettings;
    std::function<void()> onWidthChanged;
    std::function<void()> onContentHeightChanged;
    /** Fired when × clears search — leave search results. */
    std::function<void()> onExitSearch;

    /** Open/focus the SEARCH field (legacy name kept for editor wiring). */
    void setSearchFormOpen(bool open);
    bool isSearchFormOpen() const { return searchOpen; }
    void deactivateSearch(bool clearCriteria = false);

private:
    enum class RowKind { Open, Starred, SavedDir, SavedSearch };
    enum class SectionId { Library, Search, Saved };

    struct RowHit
    {
        bool valid = false;
        RowKind kind = RowKind::Open;
        int index = -1;
        bool removeZone = false;
    };
    enum class ChromeHit
    {
        None, Settings, OpenFolder, Star, Refresh, ScanSubdirs, Filters
    };

    struct Row { RowKind kind; int index = -1; };

    void rebuildRows();
    void layoutChildren();
    void notifyHeightChanged();
    RowHit rowHitAt(juce::Point<int> pos) const;
    ChromeHit chromeHitAt(juce::Point<int> pos) const;
    juce::Rectangle<int> rowBounds(int rowIdx) const;
    void paintRow(juce::Graphics&, int rowIdx, const juce::Rectangle<int>& r,
                  bool hovered, bool removeZone, bool keyboardFocus);
    void paintSectionLabel(juce::Graphics&, const juce::String& title,
                           juce::Rectangle<int> bounds);
    void paintOpenFolderButton(juce::Graphics&, juce::Rectangle<int> r, bool hover, bool focus);
    void paintFolderCard(juce::Graphics&);
    void paintActionButton(juce::Graphics&, juce::Rectangle<int> r, bool hover, bool active,
                           bool disabled);
    int contentTopY() const;
    int contentBottomY() const;
    int footerPad() const;
    int footerHeight() const;
    bool isCurrentFolderFavorited() const;
    int activeFilterGroupCount() const;
    void toggleFilterPanel();
    int folderCardHeight() const;

    /** Keyboard / tooltip navigation targets in visual order. */
    enum class NavKind
    {
        Row, OpenFolder, Star, Refresh, ScanSubdirs, Filters, Settings
    };
    struct NavItem
    {
        NavKind kind = NavKind::Row;
        int rowIdx = -1;
    };
    void rebuildNavItems();
    void activateNavItem(const NavItem&);
    void moveNav(int delta);
    void updateTooltipForPos(juce::Point<int> pos);
    juce::Rectangle<int> navItemBounds(const NavItem&) const;

    enum class LayoutKind { OpenBtn, FolderCard, Section, Row, Query, Filter, End };
    struct LayoutItem
    {
        LayoutKind kind = LayoutKind::End;
        int id = -1; // row index or SectionId
        juce::Rectangle<int> bounds;
    };
    std::vector<LayoutItem> collectLayout() const;
    void scheduleFilterApply();
    void timerCallback() override;

    IconBtn btnToggle { icons::sidebar, "Show or hide sidebar" };
    IconBtn btnSettings { icons::gear, "Settings" };
    juce::StringArray dirs;
    juce::String active; // current folder path when a root is loaded
    std::vector<SavedSearchEntry> savedSearches;
    int activeSearchIdx = -1;
    int browseMode = 0;
    bool starredFilter = false;
    bool collapsed = false;
    int expandedWidth = 0;
    int hoverRow = -1;
    bool hoverRemove = false;
    bool hoverSettings = false;
    ChromeHit hoverChrome = ChromeHit::None;
    std::vector<NavItem> navItems;
    int navIndex = -1;
    bool resizing = false;
    int resizeStartWidth = 0;
    int resizeStartX = 0;

    bool searchOpen = true;
    bool includeSubdirs = false;
    bool scanning = false;
    bool scanFinished = false;
    int scanProgressDone = 0;
    int scanProgressTotal = 0;
    bool filterOpen = false;
    bool filterApplyPending = false;
    float scanAngle = 0.0f;
    int currentClipCount = 0;
    int starredCount = 0;

    juce::Rectangle<int> settingsRowBounds;
    juce::Rectangle<int> openFolderBounds, folderCardBounds;
    juce::Rectangle<int> starBounds, refreshBtnBounds, scanBtnBounds, filtersBtnBounds;
    juce::Rectangle<int> libraryHeaderBounds, libraryPlusBounds, searchHeaderBounds, savedHeaderBounds;

    std::vector<Row> rows;

    // ── Filter panel (key, kind, histogram ranges)
    class FilterPanel : public juce::Component
    {
    public:
        FilterPanel();
        void resized() override;
        void paint(juce::Graphics&) override;
        void lookAndFeelChanged() override;
        void applyTo(BrowserSearch&) const;
        void loadFrom(const BrowserSearch&);
        void resetFilters();
        void setHistograms(const std::vector<StepClip>& clips);
        int idealHeight() const;
        int activeGroupCount() const;
        std::function<void()> onCriteriaChanged;
    private:
        int rowH() const;
        int rangeH() const;
        void mouseDown(const juce::MouseEvent&) override;
        bool hideDuplicates = true;
        juce::Rectangle<int> hideDupesBounds, resetBounds;
        fx::ChipGrid keyGrid;
        fx::ChipGrid kindGrid;
        fx::ChipGrid timeSigGrid;
        fx::HistRangeSlider bpmRange { "TEMPO", 40, 240 };
        fx::HistRangeSlider barsRange { "BARS", 1, 64 };
        fx::HistRangeSlider complexityRange { "COMPLEXITY", 0, 100 };
    };

    // ── Search query (field + Save chip when typed)
    class SearchQueryPanel : public juce::Component
    {
    public:
        SearchQueryPanel();
        void resized() override;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void lookAndFeelChanged() override;
        juce::String getQuery() const;
        void setQuery(const juce::String&);
        void clearQuery();
        void focusQuery();
        void blurQuery();
        bool isQueryFocused() const;
        int idealHeight() const { return metrics::scaled(28); }
        void setFilterActive(bool on);
        std::function<void()> onActivate;
        std::function<void()> onToggleFilter;
        std::function<void()> onClear;
        std::function<void()> onDeactivate;
        std::function<void()> onSave;
        std::function<void()> onReturn; // Enter → run search
    private:
        void styleEditors();
        void updateTrailingVisible();

        struct QueryEditor : juce::TextEditor
        {
            std::function<void()> onFocused;
            std::function<void()> onEscape;
            std::function<void()> onFocusChanged;
            void focusGained(FocusChangeType cause) override
            {
                juce::TextEditor::focusGained(cause);
                if (onFocused) onFocused();
                if (onFocusChanged) onFocusChanged();
            }
            void focusLost(FocusChangeType cause) override
            {
                juce::TextEditor::focusLost(cause);
                if (onFocusChanged) onFocusChanged();
            }
            bool keyPressed(const juce::KeyPress& key) override
            {
                if (key == juce::KeyPress::escapeKey)
                {
                    if (onEscape) onEscape();
                    return true;
                }
                return juce::TextEditor::keyPressed(key);
            }
        };

        QueryEditor queryField;
        IconBtn clearBtn { icons::x, "Clear search" };
        juce::Rectangle<int> queryWell, filterCapBounds, saveChipBounds;
        bool saveVisible = false;
        bool filterActive = false;
    };

    FilterPanel filterPanel;
    SearchQueryPanel searchQuery;
    std::vector<StepClip> histogramClips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FavoritesSidebar)
};

// Sortable file table with optional metadata columns.

struct BrowserColumnVisibility
{
    bool key = true;
    bool tempo = true;
    bool bars = true;
    bool kind = true;
    bool complexity = false;
    bool difNotes = false;
    bool timeSig = false;
    bool notes = false;
};

struct FileListEntry
{
    juce::File file;
    juce::String name;
    ClipKind kind = ClipKind::Lead;
    juce::String rootName;
    double bpm = 0.0;
    int bars = 0;
    int timeSigNum = 4;
    int timeSigDen = 4;
    int noteCount = 0;
    int difNotes = 0;
    int complexity = 0;
    bool edited = false;
    bool starred = false;
    bool isDirectory = false;
    /** When remove-duplicates collapsed several paths: Location 1..N for Finder. */
    juce::StringArray locations;
};

class FileListPanel : public juce::Component,
                      private juce::Timer,
                      private juce::ScrollBar::Listener
{
public:
    enum class SortColumn
    {
        Name, Key, Tempo, Bars, Kind, Complexity, DifNotes, TimeSig, Notes
    };

    FileListPanel();

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

    void setFolderName(const juce::String& name);
    /** Shown when the list has no rows (e.g. folder open but not yet scanned). */
    void setEmptyHint(const juce::String& hint);
    void setEntries(std::vector<FileListEntry> entries);
    void updateEntry(int index, const FileListEntry& entry);
    void setSelectedIndex(int index, juce::NotificationType notify = juce::sendNotification);
    int getSelectedIndex() const { return selected; }
    int getNumEntries() const { return (int) entries.size(); }
    void setPlaying(bool isPlaying);
    void setSearching(bool searching);
    /** Shown on the scanning overlay. `total <= 0` hides the bar. */
    void setScanProgress(int done, int total);

    void selectAdjacent(int direction);
    void grabBrowseFocus();

    void setColumnVisibility(const BrowserColumnVisibility& v);
    BrowserColumnVisibility getColumnVisibility() const { return columnsVisible; }
    /** Logical (unscaled) Name column width. */
    void setNameColumnWidth(int logicalW);
    int getNameColumnWidth() const { return nameColumnW; }
    /** Size the Name column so every current filename fits (no ellipsis). */
    void fitNameColumnToContents();
    /** Pixel width needed to show every visible column without horizontal scroll. */
    int idealContentWidth() const { return totalContentWidth(); }
    std::function<void(const BrowserColumnVisibility&)> onColumnVisibilityChanged;
    std::function<void(int logicalWidth)> onNameColumnWidthChanged;

    std::function<void(int)> onSelect;       // entry index
    std::function<void(int)> onPlayRow;      // entry index
    std::function<void(int)> onToggleStar;   // entry index
    std::function<void()> onEnterParent;
    std::function<void(int)> onEnterFolder;  // entry index
    /** Double-click a file (not a folder). */
    std::function<void(int)> onOpenInPianoRoll;
    std::function<void(const juce::File&)> onDragFile;
    std::function<void()> onEmptyOpenFolder;
    /** Fired when the user clicks the file list (sticky browser key-nav). */
    std::function<void()> onActivated;
    std::function<void(const juce::File&)> onRevealFile;
    std::function<void(const juce::File&)> onCopyFileToFolder;

    void setSort(SortColumn column, bool ascending);

    const FileListEntry* entryAt(int index) const
    {
        return juce::isPositiveAndBelow(index, (int) entries.size())
                   ? &entries[(size_t) index] : nullptr;
    }

private:
    class ListContent : public juce::Component
    {
    public:
        explicit ListContent(FileListPanel& p) : owner(p) {}
        void paint(juce::Graphics&) override;
        void mouseMove(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void clearDragState();
        FileListPanel& owner;
        int hoverRow = -1;
        bool hoverStar = false;
        juce::String dragSourcePath;
    };

    struct ColumnRects
    {
        juce::Rectangle<int> name, key, tempo, bars, kind, complexity, difNotes, timeSig, notes;
    };

    static constexpr int kNameWDefault = 140;
    static constexpr int kNameWMin = 120;
    static constexpr int kNameWMax = 2400;
    static constexpr int kKeyW = 44;
    static constexpr int kTempoW = 52;
    static constexpr int kBarsW = 44;
    static constexpr int kKindW = 56;
    static constexpr int kComplexityW = 52;
    static constexpr int kDifNotesW = 56;
    static constexpr int kTimeSigW = 52;
    static constexpr int kNotesW = 48;
    static constexpr int kSortArrowW = 12;
    static constexpr int kIconW = 23;
    static constexpr int kResizeHitSlop = 8;

    int nameResizeHandleX() const;
    int nameColumnRightContentX() const;
    bool hitNameResizeHandle(juce::Point<int> pos) const;
    bool hitNameResizeHandleContent(juce::Point<int> pos) const;
    void beginNameColumnResize(int screenX);
    void dragNameColumnResize(int screenX);
    void endNameColumnResize();
    void paintNameColumnDivider(juce::Graphics& g, int x, int y, int h) const;

    void timerCallback() override;
    void ensureRowVisible(int index);
    void paintRow(juce::Graphics&, int index, juce::Rectangle<int> r,
                  bool hovered, bool hoverStar);
    void paintColumnHeader(juce::Graphics&);
    void updateContentSize();
    void rebuildSortOrder();
    void setScrollRow(int row);
    void syncRowScroll();
    int visibleRowCount() const;
    int displayIndexAtContentY(int y) const;
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
    int displayToEntry(int displayIdx) const;
    int entryToDisplay(int entryIdx) const;
    juce::Rectangle<int> headerColumnBounds(SortColumn col) const;
    ColumnRects splitRowColumns(juce::Rectangle<int> row) const;
    int totalContentWidth() const;
    bool isColumnVisible(SortColumn col) const;
    void showColumnVisibilityMenu();

    juce::String folderName { "Select a folder" };
    juce::String emptyHint { "No files" };
    std::vector<FileListEntry> entries;
    std::vector<int> sortOrder;
    /** entry index -> display row. Rebuilt with sortOrder. */
    std::vector<int> displayOfEntry;
    int scrollRow = 0;
    SortColumn sortColumn = SortColumn::Name;
    bool sortAscending = true;
    BrowserColumnVisibility columnsVisible;
    int nameColumnW = kNameWDefault;
    bool resizingNameColumn = false;
    int nameResizeStartScreenX = 0;
    int nameResizeStartW = kNameWDefault;
    int selected = -1;
    bool playing = false;
    bool searching = false;
    int scanDone = 0;
    int scanTotal = 0;
    double searchAnimT = 0.0;
    int lastViewX = 0;

    juce::Viewport viewport;
    juce::ScrollBar rowScroll { true };
    ListContent content { *this };
    IconBtn scrollTopBtn { icons::caretUp, "Scroll to top" };
    void updateScrollTopButton();

    friend class ListContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileListPanel)
};

} // namespace pflow
