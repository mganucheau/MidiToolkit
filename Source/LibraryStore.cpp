#include "LibraryStore.h"
#include "Theme.h"
#include <algorithm>

namespace pflow {

juce::File LibraryStore::libraryFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Midi Toolkit");
    dir.createDirectory();
    return dir.getChildFile("library.xml");
}

bool LibraryStore::searchesEqual(const BrowserSearch& a, const BrowserSearch& b)
{
    return a.query == b.query
        && std::abs(a.bpmMin - b.bpmMin) < 1.0e-6
        && std::abs(a.bpmMax - b.bpmMax) < 1.0e-6
        && a.keyRoot == b.keyRoot
        && a.keyMask == b.keyMask
        && a.barsMin == b.barsMin
        && a.barsMax == b.barsMax
        && a.complexityMin == b.complexityMin
        && a.complexityMax == b.complexityMax
        && a.kindMask == b.kindMask
        && a.tagMask == b.tagMask
        && a.timeSigMask == b.timeSigMask
        && a.subdirs == b.subdirs
        && a.removeDuplicates == b.removeDuplicates;
}

BrowserSearch LibraryStore::browserSearchFromXml(const juce::XmlElement& el)
{
    BrowserSearch s;
    s.query = el.getStringAttribute("query");
    s.keyRoot = el.getIntAttribute("key", -1);
    s.keyMask = (uint16_t) juce::jlimit(0, 0x0FFF, el.getIntAttribute("keyMask", 0));
    if (s.keyMask == 0 && s.keyRoot >= 0 && s.keyRoot < 12)
        s.keyMask = (uint16_t) (1u << s.keyRoot);
    s.subdirs = el.getIntAttribute("subdirs", 1) != 0;
    s.removeDuplicates = el.getIntAttribute("removeDuplicates", 1) != 0;
    s.complexityMin = juce::jmax(0, el.getIntAttribute("complexityMin", 0));
    s.complexityMax = juce::jmax(0, el.getIntAttribute("complexityMax", 0));
    // Mask to the four kinds (Bass/Piano/Lead/Drums); drop legacy Single bit.
    s.kindMask = (uint8_t) (el.getIntAttribute("kindMask", 0) & 0x0F);
    s.tagMask = (uint16_t) juce::jlimit(0, 0xFFFF, el.getIntAttribute("tagMask", 0));
    s.timeSigMask = (uint16_t) juce::jlimit(0, 0xFFFF, el.getIntAttribute("timeSigMask", 0));

    if (el.hasAttribute("bpmMin") || el.hasAttribute("bpmMax"))
    {
        s.bpmMin = el.getDoubleAttribute("bpmMin", 0.0);
        s.bpmMax = el.getDoubleAttribute("bpmMax", 0.0);
    }
    else
    {
        // Legacy: single bpm → both min and max.
        const double bpm = el.getDoubleAttribute("bpm", 0.0);
        s.bpmMin = bpm;
        s.bpmMax = bpm;
    }

    if (el.hasAttribute("barsMin") || el.hasAttribute("barsMax"))
    {
        s.barsMin = juce::jmax(0, el.getIntAttribute("barsMin", 0));
        s.barsMax = juce::jmax(0, el.getIntAttribute("barsMax", 0));
    }
    else
    {
        // Legacy: single bars → barsMax (exact match via max only was wrong;
        // old behaviour was exact bars, so set both min and max).
        const int bars = juce::jmax(0, el.getIntAttribute("bars", 0));
        s.barsMin = bars;
        s.barsMax = bars;
    }
    return s;
}

void LibraryStore::browserSearchToXml(juce::XmlElement& el, const BrowserSearch& s)
{
    el.setAttribute("query", s.query);
    el.setAttribute("bpmMin", s.bpmMin);
    el.setAttribute("bpmMax", s.bpmMax);
    el.setAttribute("key", s.keyRoot);
    el.setAttribute("keyMask", (int) s.keyMask);
    el.setAttribute("barsMin", s.barsMin);
    el.setAttribute("barsMax", s.barsMax);
    el.setAttribute("complexityMin", s.complexityMin);
    el.setAttribute("complexityMax", s.complexityMax);
    el.setAttribute("kindMask", (int) s.kindMask);
    el.setAttribute("tagMask", (int) s.tagMask);
    el.setAttribute("timeSigMask", (int) s.timeSigMask);
    el.setAttribute("subdirs", s.subdirs ? 1 : 0);
    el.setAttribute("removeDuplicates", s.removeDuplicates ? 1 : 0);
}

SavedLibraryFolder LibraryStore::savedLibraryFromXml(const juce::XmlElement& el)
{
    SavedLibraryFolder f;
    f.path = el.getStringAttribute("path");
    f.scanSubdirs = el.getIntAttribute("scanSubdirs", 0) != 0;
    if (el.getIntAttribute("hasFilter", 0) != 0)
        f.filter = browserSearchFromXml(el);
    f.filter.subdirs = f.scanSubdirs;
    return f;
}

void LibraryStore::savedLibraryToXml(juce::XmlElement& el, const SavedLibraryFolder& folder)
{
    el.setAttribute("path", folder.path);
    el.setAttribute("scanSubdirs", folder.scanSubdirs ? 1 : 0);
    el.setAttribute("hasFilter", 1);
    browserSearchToXml(el, folder.filter);
}

SharedUiSession LibraryStore::uiSessionFromXml(const juce::XmlElement& el)
{
    SharedUiSession s;
    s.lastBrowserDir = el.getStringAttribute("lastBrowserDir");
    s.browseMode = juce::jlimit(0, 2, el.getIntAttribute("browseMode", 0));
    s.starredFilter = el.getIntAttribute("starredFilter", 0) != 0;
    s.includeSubdirs = el.getIntAttribute("includeSubdirs", 0) != 0;
    s.selectedClipPath = el.getStringAttribute("selectedClipPath");
    s.activeSavedSearchIdx = el.getIntAttribute("activeSavedSearchIdx", -1);
    s.nameColumnWidth = juce::jlimit(120, 2400, el.getIntAttribute("nameColumnWidth", 140));
    s.browserPaneW = juce::jmax(0, el.getIntAttribute("browserPaneW", 0));
    s.editorOpen = el.getIntAttribute("editorOpen", 0) != 0;
    s.effectsOpen = el.getIntAttribute("effectsOpen", 0) != 0;
    s.previewOpen = el.getIntAttribute("previewOpen", 1) != 0;
    s.sidebarCollapsed = el.getIntAttribute("sidebarCollapsed", 0) != 0;
    s.passthrough = el.getIntAttribute("passthrough", 0) != 0;
    s.columnVisibility.key = el.getIntAttribute("colKey", 1) != 0;
    s.columnVisibility.tempo = el.getIntAttribute("colTempo", 1) != 0;
    s.columnVisibility.bars = el.getIntAttribute("colBars", 1) != 0;
    s.columnVisibility.kind = el.getIntAttribute("colKind", 1) != 0;
    s.columnVisibility.complexity = el.getIntAttribute("colComplexity", 0) != 0;
    s.columnVisibility.difNotes = el.getIntAttribute("colDifNotes", 0) != 0;
    s.columnVisibility.timeSig = el.getIntAttribute("colTimeSig", 0) != 0;
    s.columnVisibility.notes = el.getIntAttribute("colNotes", 0) != 0;
    s.tweakDensity = juce::jlimit(0, 1, el.getIntAttribute("tweakDensity", 1));
    s.tweakSize = juce::jlimit(0, 2, el.getIntAttribute("tweakSize", 1));
    s.tweakTextScalePct = juce::jlimit(60, 150, el.getIntAttribute("tweakTextScalePct", 100));
    const int themeCatalog = el.getIntAttribute("themeCatalog", 0);
    if (themeCatalog >= 2)
    {
        s.tweakAppearance = juce::jlimit(0, 2, el.getIntAttribute("tweakAppearance", 0));
        s.tweakLightTheme = juce::jlimit(0, 9, el.getIntAttribute("tweakLightTheme", 5));
        s.tweakDarkTheme = juce::jlimit(0, 9, el.getIntAttribute("tweakDarkTheme", 0));
    }
    else if (themeCatalog == 1)
    {
        const int named = juce::jlimit(0, 19, el.getIntAttribute("tweakAppearance", 5));
        s.tweakAppearance = named >= 10 ? (int) Appearance::Dark : (int) Appearance::Light;
        s.tweakLightTheme = named < 10 ? named : 5;
        s.tweakDarkTheme = named >= 10 ? named - 10 : 0;
    }
    else
    {
        s.tweakAppearance = migrateThemeIndex(el.getIntAttribute("tweakAppearance", 0), false);
        s.tweakLightTheme = 5;
        s.tweakDarkTheme = 0;
    }
    s.tweakShowTooltips = el.getIntAttribute("tweakShowTooltips", 1) != 0 ? 1 : 0;
    s.tweakFontFamily = juce::jlimit(0, 3, el.getIntAttribute("tweakFontFamily", 0));
    s.tweakSurfaceStyle = juce::jlimit(0, 2, el.getIntAttribute("tweakSurfaceStyle", 0));
    s.tweakAlternateRows = el.getIntAttribute("tweakAlternateRows", 1) != 0 ? 1 : 0;
    s.tweakSeparationPct = juce::jlimit(50, 300, el.getIntAttribute("tweakSeparationPct", 100));
    s.hasDefaultView = el.getIntAttribute("hasDefaultView", 0) != 0;
    s.defEditorOpen = el.getIntAttribute("defEditorOpen", 0) != 0;
    s.defEffectsOpen = el.getIntAttribute("defEffectsOpen", 0) != 0;
    s.defPreviewOpen = el.getIntAttribute("defPreviewOpen", 1) != 0;
    s.defSidebarCollapsed = el.getIntAttribute("defSidebarCollapsed", 0) != 0;
    s.defPassthrough = el.getIntAttribute("defPassthrough", 0) != 0;
    s.defColumns.key = el.getIntAttribute("defColKey", 1) != 0;
    s.defColumns.tempo = el.getIntAttribute("defColTempo", 1) != 0;
    s.defColumns.bars = el.getIntAttribute("defColBars", 1) != 0;
    s.defColumns.kind = el.getIntAttribute("defColKind", 1) != 0;
    s.defColumns.complexity = el.getIntAttribute("defColComplexity", 0) != 0;
    s.defColumns.difNotes = el.getIntAttribute("defColDifNotes", 0) != 0;
    s.defColumns.timeSig = el.getIntAttribute("defColTimeSig", 0) != 0;
    s.defColumns.notes = el.getIntAttribute("defColNotes", 0) != 0;
    s.defNameColumnWidth = juce::jlimit(120, 2400, el.getIntAttribute("defNameW", 140));
    s.defWindowW = el.getIntAttribute("defWindowW", 0);
    s.defWindowH = el.getIntAttribute("defWindowH", 700);
    s.defTweakSize = juce::jlimit(0, 2, el.getIntAttribute("defTweakSize", 1));
    s.defTweakTextScalePct = juce::jlimit(60, 150, el.getIntAttribute("defTweakText", 100));
    s.defTweakDensity = juce::jlimit(0, 1, el.getIntAttribute("defTweakDensity", 1));
    s.defTweakAppearance = themeCatalog >= 2
        ? juce::jlimit(0, 2, el.getIntAttribute("defTweakAppearance", 0))
        : migrateThemeIndex(el.getIntAttribute("defTweakAppearance", 0), themeCatalog != 0);
    s.browserSessionSearch = browserSearchFromXml(el);

    for (auto* child : el.getChildIterator())
    {
        if (child->hasTagName("SavedFolder"))
        {
            auto folder = savedLibraryFromXml(*child);
            if (folder.path.isNotEmpty() && !s.savedBrowserDirs.contains(folder.path))
            {
                s.savedBrowserDirs.add(folder.path);
                s.savedLibraries.push_back(std::move(folder));
            }
        }
        else if (child->hasTagName("Result"))
        {
            const auto path = child->getStringAttribute("path");
            if (path.isNotEmpty() && s.browserResultPaths.size() < kMaxBrowserResultPaths)
                s.browserResultPaths.add(path);
        }
    }
    return s;
}

void LibraryStore::uiSessionToXml(juce::XmlElement& el, const SharedUiSession& s)
{
    el.setAttribute("lastBrowserDir", s.lastBrowserDir);
    el.setAttribute("browseMode", s.browseMode);
    el.setAttribute("starredFilter", s.starredFilter ? 1 : 0);
    el.setAttribute("includeSubdirs", s.includeSubdirs ? 1 : 0);
    el.setAttribute("selectedClipPath", s.selectedClipPath);
    el.setAttribute("activeSavedSearchIdx", s.activeSavedSearchIdx);
    el.setAttribute("nameColumnWidth", s.nameColumnWidth);
    el.setAttribute("browserPaneW", s.browserPaneW);
    el.setAttribute("editorOpen", s.editorOpen ? 1 : 0);
    el.setAttribute("effectsOpen", s.effectsOpen ? 1 : 0);
    el.setAttribute("previewOpen", s.previewOpen ? 1 : 0);
    el.setAttribute("sidebarCollapsed", s.sidebarCollapsed ? 1 : 0);
    el.setAttribute("passthrough", s.passthrough ? 1 : 0);
    el.setAttribute("colKey", s.columnVisibility.key ? 1 : 0);
    el.setAttribute("colTempo", s.columnVisibility.tempo ? 1 : 0);
    el.setAttribute("colBars", s.columnVisibility.bars ? 1 : 0);
    el.setAttribute("colKind", s.columnVisibility.kind ? 1 : 0);
    el.setAttribute("colComplexity", s.columnVisibility.complexity ? 1 : 0);
    el.setAttribute("colDifNotes", s.columnVisibility.difNotes ? 1 : 0);
    el.setAttribute("colTimeSig", s.columnVisibility.timeSig ? 1 : 0);
    el.setAttribute("colNotes", s.columnVisibility.notes ? 1 : 0);
    el.setAttribute("tweakDensity", s.tweakDensity);
    el.setAttribute("tweakSize", s.tweakSize);
    el.setAttribute("tweakTextScalePct", s.tweakTextScalePct);
    el.setAttribute("themeCatalog", 2); // Appearance mode + light/dark theme pairs
    el.setAttribute("tweakAppearance", s.tweakAppearance);
    el.setAttribute("tweakLightTheme", s.tweakLightTheme);
    el.setAttribute("tweakDarkTheme", s.tweakDarkTheme);
    el.setAttribute("tweakShowTooltips", s.tweakShowTooltips);
    el.setAttribute("tweakFontFamily", s.tweakFontFamily);
    el.setAttribute("tweakSurfaceStyle", s.tweakSurfaceStyle);
    el.setAttribute("tweakAlternateRows", s.tweakAlternateRows);
    el.setAttribute("tweakSeparationPct", s.tweakSeparationPct);
    el.setAttribute("hasDefaultView", s.hasDefaultView ? 1 : 0);
    el.setAttribute("defEditorOpen", s.defEditorOpen ? 1 : 0);
    el.setAttribute("defEffectsOpen", s.defEffectsOpen ? 1 : 0);
    el.setAttribute("defPreviewOpen", s.defPreviewOpen ? 1 : 0);
    el.setAttribute("defSidebarCollapsed", s.defSidebarCollapsed ? 1 : 0);
    el.setAttribute("defPassthrough", s.defPassthrough ? 1 : 0);
    el.setAttribute("defColKey", s.defColumns.key ? 1 : 0);
    el.setAttribute("defColTempo", s.defColumns.tempo ? 1 : 0);
    el.setAttribute("defColBars", s.defColumns.bars ? 1 : 0);
    el.setAttribute("defColKind", s.defColumns.kind ? 1 : 0);
    el.setAttribute("defColComplexity", s.defColumns.complexity ? 1 : 0);
    el.setAttribute("defColDifNotes", s.defColumns.difNotes ? 1 : 0);
    el.setAttribute("defColTimeSig", s.defColumns.timeSig ? 1 : 0);
    el.setAttribute("defColNotes", s.defColumns.notes ? 1 : 0);
    el.setAttribute("defNameW", s.defNameColumnWidth);
    el.setAttribute("defWindowW", s.defWindowW);
    el.setAttribute("defWindowH", s.defWindowH);
    el.setAttribute("defTweakSize", s.defTweakSize);
    el.setAttribute("defTweakText", s.defTweakTextScalePct);
    el.setAttribute("defTweakDensity", s.defTweakDensity);
    el.setAttribute("defTweakAppearance", s.defTweakAppearance);
    browserSearchToXml(el, s.browserSessionSearch);

    if (!s.savedLibraries.empty())
    {
        for (const auto& folder : s.savedLibraries)
        {
            if (folder.path.isEmpty()) continue;
            auto* child = el.createNewChildElement("SavedFolder");
            savedLibraryToXml(*child, folder);
        }
    }
    else
    {
        for (const auto& folder : s.savedBrowserDirs)
        {
            if (folder.isEmpty()) continue;
            auto* child = el.createNewChildElement("SavedFolder");
            child->setAttribute("path", folder);
        }
    }
    int n = 0;
    for (const auto& path : s.browserResultPaths)
    {
        if (path.isEmpty()) continue;
        if (++n > kMaxBrowserResultPaths) break;
        auto* child = el.createNewChildElement("Result");
        child->setAttribute("path", path);
    }
}

void LibraryStore::load()
{
    starredFiles.clear();
    starredFolders.clear();
    savedSearches.clear();
    searchCache.clear();
    uiSession = {};
    hasUiSession = false;

    const auto file = libraryFile();
    if (!file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        if (!xml->hasTagName("MidiToolkitLibrary") && !xml->hasTagName("MidiBrowserLibrary"))
            return;

        for (auto* child : xml->getChildIterator())
        {
            if (child->hasTagName("StarredFile"))
            {
                const auto path = child->getStringAttribute("path");
                if (path.isNotEmpty() && !starredFiles.contains(path))
                    starredFiles.add(path);
            }
            else if (child->hasTagName("StarredFolder"))
            {
                const auto path = child->getStringAttribute("path");
                if (path.isNotEmpty() && !starredFolders.contains(path))
                    starredFolders.add(path);
            }
            else if (child->hasTagName("SavedSearch"))
            {
                SavedSearchEntry entry;
                entry.name = child->getStringAttribute("name");
                entry.search = browserSearchFromXml(*child);
                entry.rootPath = child->getStringAttribute("root");
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
                    savedSearches.push_back(std::move(entry));
            }
            else if (child->hasTagName("SearchCache"))
            {
                CachedSearch c;
                c.rootPath = child->getStringAttribute("root");
                c.criteria = browserSearchFromXml(*child);
                c.scannedAtMs = (juce::int64) child->getStringAttribute("scannedAt").getLargeIntValue();
                for (auto* pathEl : child->getChildIterator())
                    if (pathEl->hasTagName("Path"))
                    {
                        const auto p = pathEl->getStringAttribute("value");
                        if (p.isNotEmpty())
                            c.resultPaths.add(p);
                    }
                if (c.rootPath.isNotEmpty())
                    searchCache.push_back(std::move(c));
            }
            else if (child->hasTagName("UiSession"))
            {
                uiSession = uiSessionFromXml(*child);
                hasUiSession = true;
            }
        }
    }
}

juce::StringArray LibraryStore::readStarsFromDisk()
{
    juce::StringArray stars;
    const auto file = libraryFile();
    if (!file.existsAsFile())
        return stars;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        if (!xml->hasTagName("MidiToolkitLibrary") && !xml->hasTagName("MidiBrowserLibrary"))
            return stars;
        for (auto* child : xml->getChildIterator())
        {
            if (!child->hasTagName("StarredFile"))
                continue;
            const auto path = child->getStringAttribute("path");
            if (path.isNotEmpty() && !stars.contains(path))
                stars.add(path);
        }
    }
    return stars;
}

void LibraryStore::writeLibraryFile(juce::StringArray stars,
                                    juce::StringArray folders,
                                    std::vector<SavedSearchEntry> searches,
                                    std::vector<CachedSearch> cache,
                                    SharedUiSession session,
                                    bool writeSession)
{
    juce::XmlElement xml("MidiToolkitLibrary");
    xml.setAttribute("version", 2);

    for (const auto& star : stars)
    {
        if (star.isEmpty()) continue;
        auto* child = xml.createNewChildElement("StarredFile");
        child->setAttribute("path", star);
    }
    for (const auto& folder : folders)
    {
        if (folder.isEmpty()) continue;
        auto* child = xml.createNewChildElement("StarredFolder");
        child->setAttribute("path", folder);
    }

    for (const auto& ss : searches)
    {
        if (ss.name.isEmpty()) continue;
        auto* child = xml.createNewChildElement("SavedSearch");
        child->setAttribute("name", ss.name);
        if (ss.rootPath.isNotEmpty())
            child->setAttribute("root", ss.rootPath);
        browserSearchToXml(*child, ss.search);
        for (const auto& p : ss.resultPaths)
        {
            if (p.isEmpty()) continue;
            auto* pathEl = child->createNewChildElement("Path");
            pathEl->setAttribute("value", p);
        }
    }

    for (const auto& c : cache)
    {
        if (c.rootPath.isEmpty()) continue;
        auto* child = xml.createNewChildElement("SearchCache");
        child->setAttribute("root", c.rootPath);
        browserSearchToXml(*child, c.criteria);
        child->setAttribute("scannedAt", juce::String(c.scannedAtMs));
        for (const auto& p : c.resultPaths)
        {
            if (p.isEmpty()) continue;
            auto* pathEl = child->createNewChildElement("Path");
            pathEl->setAttribute("value", p);
        }
    }

    if (writeSession)
    {
        auto* sessionEl = xml.createNewChildElement("UiSession");
        uiSessionToXml(*sessionEl, session);
    }

    const auto file = libraryFile();
    file.getParentDirectory().createDirectory();
    // Atomic-ish replace so a crash mid-write doesn't wipe the library.
    const auto tmp = file.getSiblingFile("library.xml.tmp");
    tmp.deleteFile();
    if (xml.writeTo(tmp))
        tmp.moveFileTo(file);
}

void LibraryStore::save() const
{
    const auto gen = ++saveGeneration_;
    writeLibraryFile(starredFiles, starredFolders, savedSearches, searchCache, uiSession, hasUiSession);
    saveCompleted_.store(gen);
}

void LibraryStore::saveAsync() const
{
    const auto gen = ++saveGeneration_;
    auto* genAtom = &saveGeneration_;
    auto* doneAtom = &saveCompleted_;
    juce::Thread::launch([gen, genAtom, doneAtom,
                          stars = starredFiles,
                          folders = starredFolders,
                          searches = savedSearches,
                          cache = searchCache,
                          session = uiSession,
                          writeSession = hasUiSession]
    {
        if (gen != genAtom->load())
            return;
        writeLibraryFile(std::move(stars), std::move(folders), std::move(searches), std::move(cache),
                        std::move(session), writeSession);
        if (gen == genAtom->load())
            doneAtom->store(gen);
    });
}

void LibraryStore::putUiSession(const SharedUiSession& session)
{
    uiSession = session;
    hasUiSession = true;
    saveAsync();
}

void LibraryStore::flush() const
{
    // Always persist current memory — favorites must not depend on in-flight jobs.
    save();
}

void LibraryStore::mergeStarsFromDisk()
{
    for (const auto& s : readStarsFromDisk())
        if (s.isNotEmpty() && !starredFiles.contains(s))
            starredFiles.add(s);
}

void LibraryStore::toggleStarred(const juce::String& path)
{
    if (path.isEmpty()) return;
    // Accrue against disk so other plugin instances / prior sessions stick.
    mergeStarsFromDisk();
    if (!starredFiles.contains(path))
        starredFiles.add(path);
    else
        starredFiles.removeString(path);
    // Sync write — favorites must survive host quit / crash.
    save();
}

void LibraryStore::setStarredFiles(const juce::StringArray& paths)
{
    starredFiles.clear();
    for (const auto& p : paths)
        if (p.isNotEmpty() && !starredFiles.contains(p))
            starredFiles.add(p);
    save();
}

void LibraryStore::toggleStarredFolder(const juce::String& path)
{
    if (path.isEmpty()) return;
    if (!starredFolders.contains(path))
        starredFolders.add(path);
    else
        starredFolders.removeString(path);
    save();
}

void LibraryStore::setStarredFolders(const juce::StringArray& paths)
{
    starredFolders.clear();
    for (const auto& p : paths)
        if (p.isNotEmpty() && !starredFolders.contains(p))
            starredFolders.add(p);
    save();
}

void LibraryStore::addSavedSearch(const SavedSearchEntry& entry)
{
    if (entry.name.isEmpty()) return;
    savedSearches.push_back(entry);
    while ((int) savedSearches.size() > kMaxSavedSearches)
        savedSearches.erase(savedSearches.begin());
    saveAsync();
}

void LibraryStore::removeSavedSearch(int index)
{
    if (!juce::isPositiveAndBelow(index, (int) savedSearches.size()))
        return;
    savedSearches.erase(savedSearches.begin() + index);
    saveAsync();
}

void LibraryStore::setSavedSearches(std::vector<SavedSearchEntry> entries)
{
    savedSearches = std::move(entries);
    while ((int) savedSearches.size() > kMaxSavedSearches)
        savedSearches.erase(savedSearches.begin());
    saveAsync();
}

void LibraryStore::mergeFromPluginState(const juce::StringArray& stars,
                                        const std::vector<SavedSearchEntry>& searches)
{
    bool dirty = false;
    for (const auto& s : stars)
        if (s.isNotEmpty() && !starredFiles.contains(s))
        {
            starredFiles.add(s);
            dirty = true;
        }

    for (const auto& ss : searches)
    {
        if (ss.name.isEmpty()) continue;
        bool exists = false;
        for (auto& have : savedSearches)
            if (have.name == ss.name && searchesEqual(have.search, ss.search))
            {
                exists = true;
                // Prefer the copy that still carries result paths.
                if (have.resultPaths.isEmpty() && ss.resultPaths.size() > 0)
                {
                    have.resultPaths = ss.resultPaths;
                    if (have.rootPath.isEmpty())
                        have.rootPath = ss.rootPath;
                    dirty = true;
                }
                break;
            }
        if (!exists)
        {
            savedSearches.push_back(ss);
            dirty = true;
        }
    }
    while ((int) savedSearches.size() > kMaxSavedSearches)
    {
        savedSearches.erase(savedSearches.begin());
        dirty = true;
    }

    if (dirty)
        save(); // sync — keep accrued favorites durable across host state loads
}

const CachedSearch* LibraryStore::findSearchCache(const juce::File& root,
                                                  const BrowserSearch& s) const
{
    const auto rootPath = root.getFullPathName();
    for (const auto& c : searchCache)
        if (c.rootPath == rootPath && searchesEqual(c.criteria, s))
            return &c;
    return nullptr;
}

void LibraryStore::putSearchCache(const juce::File& root, const BrowserSearch& s,
                                  const juce::StringArray& resultPaths)
{
    const auto rootPath = root.getFullPathName();
    // Replace existing entry for this root+criteria.
    searchCache.erase(std::remove_if(searchCache.begin(), searchCache.end(),
                                     [&](const CachedSearch& c)
                                     {
                                         return c.rootPath == rootPath && searchesEqual(c.criteria, s);
                                     }),
                      searchCache.end());

    CachedSearch c;
    c.rootPath = rootPath;
    c.criteria = s;
    c.resultPaths = resultPaths;
    c.scannedAtMs = juce::Time::currentTimeMillis();
    searchCache.push_back(std::move(c));

    while ((int) searchCache.size() > kMaxSearchCache)
        searchCache.erase(searchCache.begin());

    saveAsync();
}

} // namespace pflow
