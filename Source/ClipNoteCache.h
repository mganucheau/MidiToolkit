#pragma once
#include "EditModel.h"
#include <mutex>
#include <optional>
#include <vector>

namespace pflow {

/** Small LRU of fully parsed clips so arrow-key browsing does not re-read disk. */
class ClipNoteCache
{
public:
    static constexpr int kCapacity = 8;

    bool contains(const juce::String& path) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& e : entries)
            if (e.filePath == path)
                return true;
        return false;
    }

    std::optional<StepClip> find(const juce::String& path) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            if (entries[(size_t) i].filePath == path)
            {
                touch(i);
                return entries.back();
            }
        }
        return std::nullopt;
    }

    void store(StepClip clip)
    {
        if (clip.filePath.isEmpty())
            return;
        std::lock_guard<std::mutex> lock(mutex);
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            if (entries[(size_t) i].filePath == clip.filePath)
            {
                entries[(size_t) i] = std::move(clip);
                touch(i);
                return;
            }
        }
        entries.push_back(std::move(clip));
        while ((int) entries.size() > kCapacity)
            entries.erase(entries.begin());
    }

    int size() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return (int) entries.size();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        entries.clear();
    }

private:
    void touch(int index) const
    {
        if (index < 0 || index >= (int) entries.size() - 1)
            return;
        auto clip = std::move(entries[(size_t) index]);
        entries.erase(entries.begin() + index);
        entries.push_back(std::move(clip));
    }

    mutable std::mutex mutex;
    mutable std::vector<StepClip> entries;
};

} // namespace pflow
