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
    /** Bit i set = clipTag bit i required. 0 = any tags. */
    uint16_t tagMask = 0;
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

/** Shared browser sort keys (Kind omitted from Filters — grid already splits by kind). */
enum class BrowserSortColumn
{
    Name, Key, Tempo, Bars, Kind, Complexity, DifNotes, TimeSig, Notes
};

// Sidebar: LIBRARY (+ opens a folder) → joined search → foldable filters.

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
    void setBrowseMode(int mode); // 0 folder, 1 starred files, 2 search, 3 starred folders
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
    /** Star / unstar the open folder into Favorites → Folders. */
    std::function<void()> onToggleStarFolder;
    /** Gold star state for the current-folder row. */
    void setCurrentFolderStarred(bool on);
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
    std::function<void()> onShowStarredFolders;
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

    /** The search field and the filter panel are owned here (they carry the
        search logic) but hosted by the browser toolbar and the filter popover. */
    juce::Component& searchComponent();
    juce::Component& filterComponent();
    int filterIdealHeight() const;
    bool isFilterOpen() const { return filterOpen; }
    void setFilterOpen(bool open);
    int activeFilterCount() const;
    juce::String describeActiveFilters() const;
    void resetFilters();
    bool getHideDuplicates() const;
    void toggleHideDuplicates();
    bool isCurrentFolderSaved() const { return isCurrentFolderFavorited(); }
    std::function<void(bool)> onFilterOpenChanged;
    std::function<void()> onFilterLayoutChanged;
    std::function<void(BrowserSortColumn)> onSortChanged;
    void setSortColumn(BrowserSortColumn col);

    /** Open/focus the SEARCH field (legacy name kept for editor wiring). */
    void setSearchFormOpen(bool open);
    bool isSearchFormOpen() const { return searchOpen; }
    void deactivateSearch(bool clearCriteria = false);

private:
    enum class RowKind { Open, Starred, StarredFolders, SavedDir, SavedSearch };
    enum class SectionId { Library, Search, Saved };  // library dirs · Favorites · Saved Searches

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
    bool isCurrentFolderStarred() const { return currentFolderStarred; }
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

    enum class LayoutKind { OpenBtn, FolderCard, FoldBtn, Section, Row, Query, Filter, End };
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
    bool currentFolderStarred = false;

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
        juce::String describeActiveFilters() const;
        void setIncludeSubdirs(bool on);
        void setScanning(bool on);
        bool getHideDuplicates() const { return hideDuplicates; }
        void setHideDuplicates(bool on);
        void toggleHideDuplicates();
        void setSortColumn(BrowserSortColumn col);
        BrowserSortColumn getSortColumn() const { return sortColumn; }
        std::function<void()> onCriteriaChanged;
        std::function<void(bool)> onSubdirsChanged;
        std::function<void()> onLayoutChanged;
        std::function<void(BrowserSortColumn)> onSortChanged;
    private:
        void mouseDown(const juce::MouseEvent&) override;
        bool hideDuplicates = true;
        bool includeSubdirs = false;
        bool scanning = false;
        juce::Rectangle<int> hideDupesBounds, resetBounds;
        juce::Rectangle<int> subdirRowBounds, subdirSwitchBounds;
        juce::Rectangle<int> sortRowBounds;
        BrowserSortColumn sortColumn = BrowserSortColumn::Name;
        fx::FlatPopup sortPopup;
        fx::ChipGrid keyGrid;
        fx::ChipGrid kindGrid;
        fx::ChipGrid tagGrid;
        fx::ChipGrid timeSigGrid;
        fx::HistRangeSlider bpmRange { "Tempo", 40, 240 };
        fx::HistRangeSlider barsRange { "Bars", 1, 64 };
        fx::HistRangeSlider complexityRange { "Complexity", 0, 100 };
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
        int idealHeight() const { return metrics::scaled(18); }
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
    using SortColumn = BrowserSortColumn;

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

    std::function<void(const juce::String&)> onFolderNameChanged;
    std::function<void()> onEntriesChanged;
    juce::String getFolderName() const { return folderName; }

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
    static constexpr int kTempoW = 40;
    static constexpr int kBarsW = 44;
    static constexpr int kKindW = 52;
    static constexpr int kComplexityW = 52;
    static constexpr int kDifNotesW = 56;
    static constexpr int kTimeSigW = 52;
    static constexpr int kNotesW = 48;
    static constexpr int kSortArrowW = 12;
    static constexpr int kIconW = 22;
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

/** Clip colour used for a kind (the user's labels: clip-1 … clip-8). */
inline juce::Colour kindClipColour(ClipKind k)
{
    switch (k)
    {
        case ClipKind::Drums: return ds::clip(1);
        case ClipKind::Bass:  return ds::clip(6);
        case ClipKind::Keys:  return ds::clip(7);
        case ClipKind::Lead:  return ds::clip(5);
        case ClipKind::Arp:   return ds::clip(3);
    }
    return ds::clip(6);
}

/** Grid browser: files as clip slots in one column per kind (Groove ClipSlot look). */
class KindGridView : public juce::Component,
                     private juce::ScrollBar::Listener
{
public:
    KindGridView();
    static int idealWidth();
    void setSource(const FileListPanel* list);
    /** Bit i = ClipKind i. 0 = show every kind column. */
    void setKindMask(uint8_t mask);
    void refresh();
    void setPlayhead(bool isPlaying, float loopPhase);
    /** Scroll so the current selection stays in view (keyboard / selection changes). */
    void ensureSelectedVisible();

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

    std::function<void(int)> onSelect;                  // entry index
    std::function<void(int)> onOpen;                    // entry index (double-click)
    std::function<void(int)> onToggleStar;              // entry index (star hit)
    std::function<void(const juce::File&)> onDragFile;
    std::function<void(int)> onStep;                    // -1 / +1 with arrow keys (legacy vertical)
    std::function<void()> onActivated;                  // sticky browser key-nav

private:
    struct Column { ClipKind kind; std::vector<int> entries; };
    int entryAt(juce::Point<int> p) const;
    juce::Rectangle<int> starBoundsForCell(juce::Rectangle<int> inner) const;
    int maxScroll() const;
    int cellH() const;
    int numVisibleCols() const { return (int) cols.size(); }
    juce::Rectangle<int> columnBounds(int c) const;
    int columnDividerAt(juce::Point<int> p) const; // column whose right edge is hit, or -1
    int columnIndexForEntry(int entryIdx) const;
    int rowOfEntry(int col, int entryIdx) const;
    ClipKind selectedColumnKind() const;
    int defaultColWidth() const;
    int gridBodyWidth() const;
    void syncScrollBar();
    void selectAt(int col, int row);
    void moveSelection(int dCol, int dRow);
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;

    const FileListPanel* source = nullptr;
    std::vector<Column> cols;
    uint8_t kindMask = 0;
    int scrollY = 0;
    juce::ScrollBar vScroll { true };
    /** Per-kind width override (0 = share remaining space evenly). */
    int colWidthOverride[kNumClipKinds] = {};
    bool playing = false;
    float phase = 0.0f;
    int pressedEntry = -1;
    bool dragStarted = false;
    bool resizingCol = false;
    int resizeColIndex = -1;
    int resizeStartX = 0;
    int resizeStartW = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KindGridView)
};

// Browser column search row: Subfolders · Grid · Search · Filter.

class BrowserToolbar : public juce::Component
{
public:
    BrowserToolbar();

    void resized() override;
    void paint(juce::Graphics&) override;
    int idealHeight() const;

    /** Host the sidebar's search field in the search row. */
    void setSearchComponent(juce::Component* c);
    void setTitle(const juce::String&);
    void setCount(int, int);
    void setGrid(bool);
    void setSubfolders(bool);
    void setSaved(bool, bool);
    void setFiltersActive(bool open, int activeCount, const juce::String& summary = {});

    std::function<void()> onToggleGrid;
    std::function<void()> onToggleSubfolders;
    std::function<void()> onToggleSaved;
    std::function<void()> onRescan;
    std::function<void()> onToggleFilters;

private:
    juce::Component* search = nullptr;
    IconBtn btnSubfolders { icons::subfolders, "Include files in subfolders" };
    IconBtn btnGrid { icons::grid, "Grid view: files as clips in columns by kind" };
    IconBtn btnFilters { icons::sliders, "Filters" };
    bool filtersOpen = false;
    int filtersActiveCount = 0;
    juce::String filtersSummary;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserToolbar)
};

/** Floating filter menu (radius-md, shadow-popover) that hosts the sidebar's filter panel. */
class FilterPopover : public juce::Component
{
public:
    FilterPopover();
    void setContent(juce::Component* panel, std::function<int()> idealPanelHeight);
    void setCountText(const juce::String& t);
    int idealHeight() const;
    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;

    std::function<void()> onDone;
    std::function<void()> onSaveSearch;
    std::function<void()> onReset;

private:
    juce::Component* panel = nullptr;
    std::function<int()> panelHeight;
    juce::String countText;
    IconBtn btnReset { icons::undo, "Reset filters" };
    ChipBtn btnSave { "Save Search", icons::plus };
    ChipBtn btnDone { "Done" };
    juce::DropShadower shadower { juce::DropShadow(juce::Colours::black.withAlpha(0.6f), 12, { 0, 4 }) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterPopover)
};

} // namespace pflow
