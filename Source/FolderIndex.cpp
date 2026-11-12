#include "FolderIndex.h"
#include <cstring>
#include <algorithm>

#if JUCE_WINDOWS
 #include <windows.h>
#else
 #include <dirent.h>
#endif

namespace pflow {

namespace {

juce::String indexKey(const juce::File& root, bool recursive)
{
    auto path = root.getFullPathName();
#if JUCE_WINDOWS
    path = path.toLowerCase();
#endif
    path += recursive ? "|r" : "|1";
    return juce::String::toHexString(static_cast<juce::int64>(path.hashCode64()));
}

bool isMidiFileName(const char* name)
{
    if (name == nullptr || name[0] == 0 || name[0] == '.')
        return false;
    const auto len = std::strlen(name);
    if (len >= 4)
    {
        const char* ext4 = name + len - 4;
        if (ext4[0] == '.' &&
            (ext4[1] == 'm' || ext4[1] == 'M') &&
            (ext4[2] == 'i' || ext4[2] == 'I') &&
            (ext4[3] == 'd' || ext4[3] == 'D'))
            return true;
    }
    if (len >= 5)
    {
        const char* ext5 = name + len - 5;
        if (ext5[0] == '.' &&
            (ext5[1] == 'm' || ext5[1] == 'M') &&
            (ext5[2] == 'i' || ext5[2] == 'I') &&
            (ext5[3] == 'd' || ext5[3] == 'D') &&
            (ext5[4] == 'i' || ext5[4] == 'I'))
            return true;
    }
    return false;
}

FolderIndexEntry clipToEntry(const StepClip& clip)
{
    FolderIndexEntry e;
    e.path = clip.filePath;
    e.name = clip.name;
    e.kind = (int) clip.kind;
    e.kindConfidence = clip.kindConfidence;
    e.hasDrumContent = clip.hasDrumContent;
    e.tagMask = clip.tagMask;
    e.root = clip.root;
    e.bpm = clip.bpm;
    e.bars = clip.bars;
    e.timeSigNum = clip.timeSigNum;
    e.timeSigDen = clip.timeSigDen;
    e.noteCount = clip.noteCount;
    e.difNotes = clip.difNotes;
    e.complexity = clip.complexity;

    const juce::File f(clip.filePath);
    if (f.existsAsFile())
    {
        e.modMs = f.getLastModificationTime().toMilliseconds();
        e.sizeBytes = f.getSize();
        if (e.name.isEmpty())
            e.name = f.getFileNameWithoutExtension();
    }
    return e;
}

void writeString(juce::MemoryOutputStream& out, const juce::String& s)
{
    const auto utf8 = s.toRawUTF8();
    const auto n = (int) std::strlen(utf8);
    out.writeInt(n);
    if (n > 0)
        out.write(utf8, (size_t) n);
}

juce::String readString(juce::MemoryInputStream& in)
{
    const int n = in.readInt();
    if (n <= 0 || n > 1'000'000)
        return {};
    juce::HeapBlock<char> buf((size_t) n + 1);
    in.read(buf.getData(), n);
    buf[(size_t) n] = 0;
    return juce::String::fromUTF8(buf.getData(), n);
}

void listFlatNative(const juce::File& root, QuickDirListing& out, int midiCap, bool midisOnly)
{
#if JUCE_WINDOWS
    const auto wildcard = root.getFullPathName() + "\\*";
    WIN32_FIND_DATAA data;
    HANDLE h = FindFirstFileA(wildcard.toRawUTF8(), &data);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
    {
        const char* name = data.cFileName;
        if (name[0] == '.')
            continue;
        const bool isDir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDir)
        {
            if (!midisOnly)
                out.directories.push_back(root.getChildFile(name));
        }
        else if (isMidiFileName(name))
        {
            ++out.totalMidiFiles;
            if (midisOnly)
            {
                if (midiCap > 0)
                    out.midiFiles.push_back(root.getChildFile(name));
            }
            else if ((int) out.midiFiles.size() < midiCap)
            {
                out.midiFiles.push_back(root.getChildFile(name));
            }
        }
    } while (FindNextFileA(h, &data));
    FindClose(h);
#else
    DIR* d = opendir(root.getFullPathName().toRawUTF8());
    if (d == nullptr)
        return;
    while (auto* ent = readdir(d))
    {
        const char* name = ent->d_name;
        if (name[0] == '.')
            continue;

        bool isDir = (ent->d_type == DT_DIR);
        bool isFile = (ent->d_type == DT_REG);
        if (ent->d_type == DT_UNKNOWN || ent->d_type == DT_LNK)
        {
            const auto child = root.getChildFile(name);
            isDir = child.isDirectory();
            isFile = child.existsAsFile();
        }

        if (isDir)
        {
            if (!midisOnly)
                out.directories.push_back(root.getChildFile(name));
        }
        else if (isFile && isMidiFileName(name))
        {
            ++out.totalMidiFiles;
            if (midisOnly)
            {
                if (midiCap > 0)
                    out.midiFiles.push_back(root.getChildFile(name));
            }
            else if ((int) out.midiFiles.size() < midiCap)
            {
                out.midiFiles.push_back(root.getChildFile(name));
            }
        }
    }
    closedir(d);
#endif
}

void collectRecursiveNative(const juce::File& dir, juce::Array<juce::File>& out)
{
    QuickDirListing flat;
    listFlatNative(dir, flat, INT_MAX, false);
    for (auto& f : flat.midiFiles)
        out.add(std::move(f));
    for (const auto& sub : flat.directories)
        collectRecursiveNative(sub, out);
}

} // namespace

QuickDirListing listDirectoryQuick(const juce::File& dir, int midiCap)
{
    QuickDirListing out;
    if (!dir.isDirectory())
        return out;

    listFlatNative(dir, out, juce::jmax(0, midiCap), false);
    std::sort(out.directories.begin(), out.directories.end(),
              [](const juce::File& a, const juce::File& b)
              { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });
    std::sort(out.midiFiles.begin(), out.midiFiles.end(),
              [](const juce::File& a, const juce::File& b)
              { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });
    return out;
}

juce::Array<juce::File> collectMidiFiles(const juce::File& root, bool recursive)
{
    juce::Array<juce::File> files;
    if (!root.isDirectory())
        return files;

    if (!recursive)
    {
        QuickDirListing listing;
        listFlatNative(root, listing, INT_MAX, true);
        for (auto& f : listing.midiFiles)
            files.add(std::move(f));
        files.sort();
        return files;
    }

    collectRecursiveNative(root, files);
    files.sort();
    return files;
}

FolderFingerprint FolderIndex::computeFingerprint(const juce::File& root, bool recursive)
{
    FolderFingerprint fp;
    if (!root.isDirectory())
        return fp;

    fp.dirModMs = root.getLastModificationTime().toMilliseconds();
    if (!recursive)
    {
        QuickDirListing listing;
        listFlatNative(root, listing, 0, true);
        // midisOnly with midiCap 0 still counts via totalMidiFiles
        // Fix: listFlatNative with midisOnly=true and cap 0 still increments total
        // but won't push. Good.
        // Wait - with midisOnly true and cap 0, condition is midisOnly || size<cap -> true, so it pushes all!
        // Need fix: for count-only use a dedicated path.
        fp.fileCount = listing.totalMidiFiles;
        // If midisOnly pushed everything, clear to avoid huge alloc in fingerprint.
        return fp;
    }

    fp.fileCount = collectMidiFiles(root, true).size();
    return fp;
}

juce::File FolderIndex::indexDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Midi Toolkit")
                   .getChildFile("folder-indexes");
    dir.createDirectory();
    return dir;
}

juce::File FolderIndex::indexFileFor(const juce::File& root, bool recursive)
{
    return indexDir().getChildFile(indexKey(root, recursive) + ".mbidx");
}

FolderIndex FolderIndex::fromClips(const juce::File& root, bool recursive,
                                   const std::vector<StepClip>& clips)
{
    FolderIndex idx;
    idx.rootPath = root.getFullPathName();
    idx.recursive = recursive;
    idx.scannedAtMs = juce::Time::getCurrentTime().toMilliseconds();
    idx.fingerprint.dirModMs = root.getLastModificationTime().toMilliseconds();
    idx.files.reserve(clips.size());
    juce::int64 bytes = 0;
    for (const auto& c : clips)
    {
        if (c.filePath.isEmpty())
            continue;
        auto e = clipToEntry(c);
        bytes += e.sizeBytes;
        idx.files.push_back(std::move(e));
    }
    idx.fingerprint.fileCount = (int) idx.files.size();
    idx.fingerprint.totalBytes = bytes;
    return idx;
}

StepClip FolderIndex::entryToClip(const FolderIndexEntry& e)
{
    StepClip c;
    c.filePath = e.path;
    c.name = e.name.isNotEmpty() ? e.name
                                 : juce::File(e.path).getFileNameWithoutExtension();
    int kind = e.kind;
    if (kind < 0 || kind >= kNumClipKinds)
        kind = (int) ClipKind::Lead;
    c.kind = (ClipKind) kind;
    applyFilenameKindTag(c);
    c.kindConfidence = juce::jlimit(0, 100, e.kindConfidence);
    c.hasDrumContent = e.hasDrumContent;
    c.tagMask = e.tagMask;
    c.root = e.root;
    c.bpm = e.bpm > 0.0 ? e.bpm : 120.0;
    c.bars = juce::jmax(1, e.bars);
    c.timeSigNum = juce::jmax(1, e.timeSigNum);
    c.timeSigDen = juce::jmax(1, e.timeSigDen);
    c.noteCount = juce::jmax(0, e.noteCount);
    c.difNotes = juce::jmax(0, e.difNotes);
    c.complexity = juce::jlimit(1, 100, e.complexity);
    return c;
}

std::vector<StepClip> FolderIndex::toClips(const FolderIndex& idx)
{
    std::vector<StepClip> out;
    out.reserve(idx.files.size());
    for (const auto& e : idx.files)
        out.push_back(entryToClip(e));
    return out;
}

bool FolderIndex::save() const
{
    juce::MemoryOutputStream out;
    out.writeInt((int) 0x4D424958);
    out.writeInt(3); // v3: Keys/Arp kinds + keyword-first tagging
    writeString(out, rootPath);
    out.writeBool(recursive);
    out.writeInt64(scannedAtMs);
    out.writeInt(fingerprint.fileCount);
    out.writeInt64(fingerprint.dirModMs);
    out.writeInt64(fingerprint.totalBytes);
    out.writeInt((int) files.size());
    for (const auto& f : files)
    {
        writeString(out, f.path);
        writeString(out, f.name);
        out.writeInt64(f.modMs);
        out.writeInt64(f.sizeBytes);
        out.writeInt(f.kind);
        out.writeInt(f.kindConfidence);
        out.writeBool(f.hasDrumContent);
        out.writeInt((int) f.tagMask);
        out.writeInt(f.root);
        out.writeDouble(f.bpm);
        out.writeInt(f.bars);
        out.writeInt(f.timeSigNum);
        out.writeInt(f.timeSigDen);
        out.writeInt(f.noteCount);
        out.writeInt(f.difNotes);
        out.writeInt(f.complexity);
    }
    return indexFileFor(juce::File(rootPath), recursive)
        .replaceWithData(out.getData(), out.getDataSize());
}

std::optional<FolderIndex> FolderIndex::load(const juce::File& root, bool recursive)
{
    const auto file = indexFileFor(root, recursive);
    if (!file.existsAsFile())
        return std::nullopt;

    juce::MemoryBlock mb;
    if (!file.loadFileAsData(mb) || mb.getSize() < 16)
        return std::nullopt;

    juce::MemoryInputStream in(mb, false);
    if (in.readInt() != (int) 0x4D424958)
        return std::nullopt;
    const int version = in.readInt();
    // v3 remapped kinds (Piano→Keys, added Arp). Older caches still load; keywords re-tag on read.
    if (version != 1 && version != 2 && version != 3)
        return std::nullopt;

    FolderIndex idx;
    idx.rootPath = readString(in);
    idx.recursive = in.readBool();
    if (idx.recursive != recursive)
        return std::nullopt;
    idx.scannedAtMs = in.readInt64();
    idx.fingerprint.fileCount = in.readInt();
    idx.fingerprint.dirModMs = in.readInt64();
    idx.fingerprint.totalBytes = in.readInt64();

    const int n = in.readInt();
    if (n < 0 || n > 5'000'000)
        return std::nullopt;
    idx.files.reserve((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        FolderIndexEntry e;
        e.path = readString(in);
        e.name = readString(in);
        e.modMs = in.readInt64();
        e.sizeBytes = in.readInt64();
        e.kind = in.readInt();
        // v1/v2: Bass=0 Piano=1 Lead=2 Drums=3. v3 inserts Arp at 3 and shifts Drums to 4.
        if (version < 3 && e.kind == 3)
            e.kind = (int) ClipKind::Drums;
        if (version >= 2)
        {
            e.kindConfidence = in.readInt();
            e.hasDrumContent = in.readBool();
            e.tagMask = (uint16_t) juce::jlimit(0, 0xFFFF, in.readInt());
        }
        e.root = in.readInt();
        e.bpm = in.readDouble();
        e.bars = in.readInt();
        e.timeSigNum = in.readInt();
        e.timeSigDen = in.readInt();
        e.noteCount = in.readInt();
        e.difNotes = in.readInt();
        e.complexity = in.readInt();
        if (e.path.isNotEmpty())
            idx.files.push_back(std::move(e));
    }
    return idx;
}

std::optional<std::vector<StepClip>> FolderIndex::loadValidClips(const juce::File& root,
                                                                bool recursive)
{
    auto idx = load(root, recursive);
    if (!idx.has_value() || idx->files.empty())
        return std::nullopt;

    const auto live = computeFingerprint(root, recursive);
    if (!live.matchesCount(idx->fingerprint))
        return std::nullopt;
    if (idx->fingerprint.fileCount != (int) idx->files.size())
        return std::nullopt;

    return toClips(*idx);
}

} // namespace pflow
