#include "GenrePacks.h"
#include "BinaryData.h"
#include <mutex>

namespace pflow {

namespace {

void readSteps(const juce::var& value, std::array<PackStep, 16>& out)
{
    out.fill({});
    if (!value.isArray())
        return;
    auto* rows = value.getArray();
    if (rows == nullptr)
        return;
    const int n = juce::jmin(16, rows->size());
    for (int i = 0; i < n; ++i)
    {
        auto* pair = (*rows)[i].getArray();
        if (pair == nullptr || pair->isEmpty())
            continue;
        out[(size_t) i].velocity = juce::jlimit(0, 127, (int) (*pair)[0]);
        if (pair->size() > 1)
            out[(size_t) i].ticks = (int) (*pair)[1];
    }
}

const char* bytesFor(DrumStyle style, int& size)
{
    size = 0;
    switch (style)
    {
        case DrumStyle::House:
            size = BinaryData::house_jsonSize;
            return BinaryData::house_json;
        case DrumStyle::Breakbeat:
            size = BinaryData::breakbeat_jsonSize;
            return BinaryData::breakbeat_json;
        case DrumStyle::Dub:
            size = BinaryData::dub_jsonSize;
            return BinaryData::dub_json;
        case DrumStyle::Techno:
            size = BinaryData::techno_jsonSize;
            return BinaryData::techno_json;
        case DrumStyle::Funk:
            size = BinaryData::funk_jsonSize;
            return BinaryData::funk_json;
        case DrumStyle::Reggae:
            size = BinaryData::reggae_jsonSize;
            return BinaryData::reggae_json;
        case DrumStyle::DubTechno:
            size = BinaryData::dubtechno_jsonSize;
            return BinaryData::dubtechno_json;
        case DrumStyle::Dnb:
            size = BinaryData::dnb_jsonSize;
            return BinaryData::dnb_json;
        case DrumStyle::Count:
            break;
    }
    return nullptr;
}

} // namespace

bool GenrePack::anyBass() const
{
    for (const auto& s : bass)
        if (s.velocity > 0)
            return true;
    return false;
}

bool GenrePack::anyChords() const
{
    for (const auto& s : chords)
        if (s.velocity > 0)
            return true;
    return false;
}

bool GenrePack::anyArp() const
{
    for (const auto& s : arp)
        if (s.velocity > 0)
            return true;
    return false;
}

GenrePack genrePackFromJson(const juce::String& json)
{
    GenrePack pack;
    const auto parsed = juce::JSON::parse(json);
    if (!parsed.isObject())
        return pack;

    const auto bpmVar = parsed.getProperty("bpm", {});
    if (auto* bpm = bpmVar.getArray(); bpm != nullptr && bpm->size() >= 2)
    {
        pack.bpmMin = (int) (*bpm)[0];
        pack.bpmMax = (int) (*bpm)[1];
    }
    pack.swing = (double) parsed.getProperty("swing", 0.0);

    const auto feel = parsed.getProperty("feel", juce::var());
    if (feel.isObject())
    {
        pack.densityMul = (float) (double) feel.getProperty("density", 1.0);
        pack.syncopation = (float) (double) feel.getProperty("syncopation", 0.35);
        pack.sustainBias = (float) (double) feel.getProperty("sustain", 0.0);
        pack.preferOffbeats = (bool) feel.getProperty("offbeats", false);
    }

    const auto drums = parsed.getProperty("drums", juce::var());
    if (drums.isObject())
    {
        readSteps(drums.getProperty("kick", {}), pack.kick);
        readSteps(drums.getProperty("snare", {}), pack.snare);
        readSteps(drums.getProperty("hat", {}), pack.hat);
        readSteps(drums.getProperty("ghost", {}), pack.ghost);
    }

    const auto bass = parsed.getProperty("bass", juce::var());
    if (bass.isObject())
    {
        pack.bassGate = (double) bass.getProperty("gate", pack.bassGate);
        readSteps(bass.getProperty("steps", {}), pack.bass);
    }
    const auto chords = parsed.getProperty("chords", juce::var());
    if (chords.isObject())
    {
        pack.chordGate = (double) chords.getProperty("gate", pack.chordGate);
        readSteps(chords.getProperty("steps", {}), pack.chords);
    }
    const auto arp = parsed.getProperty("arp", juce::var());
    if (arp.isObject())
        readSteps(arp.getProperty("steps", {}), pack.arp);

    return pack;
}

const GenrePack& genrePack(DrumStyle style)
{
    static std::array<GenrePack, (size_t) DrumStyle::Count> packs;
    static std::once_flag once;
    std::call_once(once, []
    {
        for (int i = 0; i < (int) DrumStyle::Count; ++i)
        {
            int size = 0;
            const char* bytes = bytesFor((DrumStyle) i, size);
            if (bytes == nullptr || size <= 0)
                continue;
            packs[(size_t) i] = genrePackFromJson(juce::String::fromUTF8(bytes, size));
        }
    });
    const int idx = juce::jlimit(0, (int) DrumStyle::Count - 1, (int) style);
    return packs[(size_t) idx];
}

} // namespace pflow
