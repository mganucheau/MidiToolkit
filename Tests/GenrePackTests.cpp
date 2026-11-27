#include "GenrePacks.h"
#include "GeneratorEngines.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstring>
#include <set>
#include <utility>
#include <vector>

using namespace pflow;
using namespace pflow::gen;

namespace {

struct Hit
{
    double ppq = 0.0;
    int note = 0;
    int velocity = 0;
};

std::vector<Hit> noteOns(const std::vector<Event>& events, double lo, double hi)
{
    std::vector<Hit> out;
    for (const auto& e : events)
    {
        if (e.velocity <= 0 || e.ppq < lo || e.ppq >= hi)
            continue;
        out.push_back({ e.ppq, e.note, e.velocity });
    }
    return out;
}

int countNear(const std::vector<Hit>& hits, int note, double ppq, double tol)
{
    int n = 0;
    for (const auto& h : hits)
        if (h.note == note && std::abs(h.ppq - ppq) <= tol)
            ++n;
    return n;
}

GrooveParams drums(DrumStyle style)
{
    GrooveParams g;
    g.drumsEnabled = true;
    g.drumStyleIndex = (int) style;
    g.drumFill = 0;
    return g;
}

GrooveParams bass(DrumStyle style)
{
    GrooveParams g;
    g.bassEnabled = true;
    g.bassGenreIndex = (int) style;
    return g;
}

GrooveParams chords(DrumStyle style)
{
    GrooveParams g;
    g.chordsEnabled = true;
    g.chordGenreIndex = (int) style;
    return g;
}

int stepsUsed(const std::vector<Hit>& hits)
{
    bool used[16] = {};
    for (const auto& h : hits)
    {
        const int step = (int) std::lround(h.ppq / 0.25);
        if (step >= 0 && step < 16)
            used[step] = true;
    }
    int n = 0;
    for (bool u : used)
        if (u) ++n;
    return n;
}

} // namespace

TEST_CASE("house kick is on all four beats and hats are off-beat", "[genre]")
{
    std::vector<Event> ev;
    renderDrums(drums(DrumStyle::House), 0.0, 4.0, ev);
    const auto ons = noteOns(ev, 0.0, 4.0);
    CHECK(countNear(ons, 36, 0.0, 0.08) == 1);
    CHECK(countNear(ons, 36, 1.0, 0.08) == 1);
    CHECK(countNear(ons, 36, 2.0, 0.08) == 1);
    CHECK(countNear(ons, 36, 3.0, 0.08) == 1);

    int offHats = 0, downHats = 0;
    for (const auto& h : ons)
    {
        if (h.note != 42) continue;
        const double frac = std::fmod(h.ppq, 1.0);
        if (frac > 0.35 && frac < 0.75)
            ++offHats;
        if (frac < 0.12)
            ++downHats;
    }
    CHECK(offHats >= 4);
    CHECK(downHats == 0);
}

TEST_CASE("a pack timing offset survives into the rendered notes", "[genre]")
{
    std::vector<Event> ev;
    renderDrums(drums(DrumStyle::House), 0.0, 4.0, ev);
    bool shifted = false;
    for (const auto& e : ev)
    {
        if (e.velocity <= 0) continue;
        const double nearest = std::round(e.ppq / 0.25) * 0.25;
        if (std::abs(e.ppq - nearest) > 0.01)
            shifted = true;
    }
    CHECK(shifted);
}

TEST_CASE("ghost notes are quieter than the backbeat snare", "[genre]")
{
    std::vector<Event> ev;
    renderDrums(drums(DrumStyle::House), 0.0, 4.0, ev);
    int loud = 0, quiet = 127;
    for (const auto& e : ev)
    {
        if (e.velocity <= 0 || e.note != 38) continue;
        loud = std::max(loud, e.velocity);
        quiet = std::min(quiet, e.velocity);
    }
    CHECK(loud >= 100);
    CHECK(quiet < loud);
    CHECK(quiet <= 60);
}

TEST_CASE("reggae dub techno and dnb packs stay distinct", "[genre]")
{
    std::vector<Event> reggaeDrums, dubDrums, technoDrums, dubTechnoDrums, dnbDrums, houseDrums;
    renderDrums(drums(DrumStyle::Reggae), 0.0, 4.0, reggaeDrums);
    renderDrums(drums(DrumStyle::Dub), 0.0, 4.0, dubDrums);
    renderDrums(drums(DrumStyle::Techno), 0.0, 4.0, technoDrums);
    renderDrums(drums(DrumStyle::DubTechno), 0.0, 4.0, dubTechnoDrums);
    renderDrums(drums(DrumStyle::Dnb), 0.0, 4.0, dnbDrums);
    renderDrums(drums(DrumStyle::House), 0.0, 4.0, houseDrums);

    const auto reggaeOn = noteOns(reggaeDrums, 0.0, 4.0);
    const auto dubOn = noteOns(dubDrums, 0.0, 4.0);
    const auto technoOn = noteOns(technoDrums, 0.0, 4.0);
    const auto dubTechOn = noteOns(dubTechnoDrums, 0.0, 4.0);
    const auto dnbOn = noteOns(dnbDrums, 0.0, 4.0);

    CHECK(countNear(reggaeOn, 36, 0.0, 0.08) + countNear(reggaeOn, 36, 1.0, 0.08)
              + countNear(reggaeOn, 36, 2.0, 0.08) + countNear(reggaeOn, 36, 3.0, 0.08) < 4);
    CHECK(countNear(dubOn, 36, 2.0, 0.08) == 1);
    CHECK(countNear(dubOn, 38, 2.0, 0.08) == 1);
    CHECK(16 - stepsUsed(dubOn) >= 8);

    std::vector<Event> reggaeChords, dubChords;
    renderChords(chords(DrumStyle::Reggae), 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, reggaeChords);
    renderChords(chords(DrumStyle::Dub), 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, dubChords);
    CHECK(noteOns(reggaeChords, 0.0, 4.0).size() > noteOns(dubChords, 0.0, 4.0).size());

    std::vector<Event> reggaeBass, dubBass, dnbBass;
    renderBass(bass(DrumStyle::Reggae), 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, reggaeBass);
    renderBass(bass(DrumStyle::Dub), 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, dubBass);
    renderBass(bass(DrumStyle::Dnb), 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, dnbBass);
    CHECK(noteOns(reggaeBass, 0.0, 4.0).size() > noteOns(dubBass, 0.0, 4.0).size());

    int technoHats = 0, dubTechHats = 0;
    for (const auto& h : technoOn)
        if (h.note == 42) ++technoHats;
    for (const auto& h : dubTechOn)
        if (h.note == 42) ++dubTechHats;
    CHECK(technoHats > dubTechHats);
    CHECK(countNear(dubTechOn, 36, 0.0, 0.08) == 1);
    CHECK(countNear(dubTechOn, 36, 1.0, 0.08) == 1);
    CHECK(countNear(dubTechOn, 36, 2.0, 0.08) == 1);
    CHECK(countNear(dubTechOn, 36, 3.0, 0.08) == 1);
    CHECK(countNear(technoOn, 36, 0.0, 0.08) == 1);

    std::vector<Event> technoChords, dubTechChords, technoBass, dubTechBass;
    renderChords(chords(DrumStyle::Techno), 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, technoChords);
    renderChords(chords(DrumStyle::DubTechno), 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, dubTechChords);
    renderBass(bass(DrumStyle::Techno), 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, technoBass);
    renderBass(bass(DrumStyle::DubTechno), 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, dubTechBass);
    CHECK(noteOns(technoChords, 0.0, 4.0).size() > noteOns(dubTechChords, 0.0, 4.0).size());

    double technoGate = 0.0, dubTechGate = 0.0;
    int technoBassOns = 0, dubTechBassOns = 0;
    for (const auto& e : technoBass)
    {
        if (e.velocity <= 0) continue;
        ++technoBassOns;
    }
    for (size_t i = 0; i < dubTechBass.size(); ++i)
    {
        if (dubTechBass[i].velocity <= 0) continue;
        ++dubTechBassOns;
        for (size_t j = i + 1; j < dubTechBass.size(); ++j)
        {
            if (dubTechBass[j].note == dubTechBass[i].note && dubTechBass[j].velocity == 0)
            {
                dubTechGate = std::max(dubTechGate, dubTechBass[j].ppq - dubTechBass[i].ppq);
                break;
            }
        }
    }
    juce::ignoreUnused(technoGate, technoBassOns);
    CHECK(dubTechGate > 0.25);
    CHECK(dubTechBassOns < (int) noteOns(technoBass, 0.0, 4.0).size());

    const auto& dnb = genrePack(DrumStyle::Dnb);
    CHECK(dnb.bpmMin >= 170);
    CHECK(dnb.bpmMax <= 176);
    CHECK(std::strcmp(drumStyleLabel(DrumStyle::Dnb), "DNB") == 0);
    CHECK(countNear(dnbOn, 38, 1.0, 0.08) >= 1);
    CHECK(countNear(dnbOn, 38, 3.0, 0.08) >= 1);
    CHECK(countNear(dnbOn, 36, 0.0, 0.08)
              + countNear(dnbOn, 36, 1.0, 0.08)
              + countNear(dnbOn, 36, 2.0, 0.08)
              + countNear(dnbOn, 36, 3.0, 0.08) < 4);
    int dnbGhost = 127, dnbSnare = 0;
    for (const auto& h : dnbOn)
    {
        if (h.note != 38) continue;
        dnbSnare = std::max(dnbSnare, h.velocity);
        dnbGhost = std::min(dnbGhost, h.velocity);
    }
    CHECK(dnbSnare >= 100);
    CHECK(dnbGhost < dnbSnare);
    CHECK(noteOns(dnbBass, 0.0, 4.0).size() >= 12);
    CHECK(genrePack(DrumStyle::Breakbeat).bpmMax < 140);
    CHECK(countNear(noteOns(houseDrums, 0.0, 4.0), 36, 0.0, 0.08) == 1);
}

TEST_CASE("genre pack json is the source the plugin loads", "[genre]")
{
    const auto& house = genrePack(DrumStyle::House);
    CHECK(house.bpmMin >= 110);
    CHECK(house.kick[0].velocity > 0);
    CHECK(house.kick[4].ticks != 0);
    CHECK(house.hat[2].velocity > 0);
    CHECK(house.hat[0].velocity == 0);
    CHECK(house.ghost[7].velocity > 0);
    CHECK(house.ghost[7].velocity < house.snare[4].velocity);
}

namespace {

using HitKey = std::set<std::pair<int, int>>;

HitKey hitKeys(const std::vector<Event>& events)
{
    HitKey keys;
    for (const auto& e : events)
    {
        if (e.velocity <= 0 || e.ppq < 0.0 || e.ppq >= 4.0) continue;
        keys.insert({ (int) std::lround(e.ppq / 0.25), e.note });
    }
    return keys;
}

int downbeatKicks(const std::vector<Event>& events)
{
    const auto ons = noteOns(events, 0.0, 4.0);
    return countNear(ons, 36, 0.0, 0.08) + countNear(ons, 36, 1.0, 0.08)
         + countNear(ons, 36, 2.0, 0.08) + countNear(ons, 36, 3.0, 0.08);
}

template <typename Render>
bool findsDifferentSeed(Render render)
{
    const auto base = render(1);
    for (int nonce = 2; nonce <= 24; ++nonce)
        if (render(nonce) != base)
            return true;
    return false;
}

} // namespace

TEST_CASE("a generate nonce rolls a different in-style groove", "[genre][generate]")
{
    CHECK(findsDifferentSeed([](int nonce)
    {
        auto g = drums(DrumStyle::House);
        g.drumNonce = nonce;
        g.drumVariation = 4;
        std::vector<Event> ev;
        renderDrums(g, 0.0, 4.0, ev);
        return hitKeys(ev);
    }));

    for (int raw = 0; raw < (int) DrumStyle::Count; ++raw)
    {
        const auto style = (DrumStyle) raw;
        const bool different = findsDifferentSeed([style](int nonce)
        {
            auto g = drums(style);
            g.drumNonce = nonce;
            g.drumVariation = 4;
            std::vector<Event> ev;
            renderDrums(g, 0.0, 4.0, ev);
            return hitKeys(ev);
        });
        CHECK(different);
    }

    for (int nonce = 1; nonce <= 12; ++nonce)
    {
        for (auto style : { DrumStyle::House, DrumStyle::Techno })
        {
            auto g = drums(style);
            g.drumNonce = nonce;
            g.drumVariation = 4;
            std::vector<Event> ev;
            renderDrums(g, 0.0, 4.0, ev);
            CHECK(downbeatKicks(ev) == 4);
        }
    }

    bool reggaeOneDrop = false;
    for (int nonce = 1; nonce <= 24 && !reggaeOneDrop; ++nonce)
    {
        auto g = drums(DrumStyle::Reggae);
        g.drumNonce = nonce;
        g.drumVariation = 4;
        std::vector<Event> ev;
        renderDrums(g, 0.0, 4.0, ev);
        reggaeOneDrop = downbeatKicks(ev) < 4;
    }
    CHECK(reggaeOneDrop);

    CHECK(findsDifferentSeed([](int nonce)
    {
        GrooveParams g;
        g.melodyEnabled = true;
        g.melodyNonce = nonce;
        g.melodyVariation = 4;
        std::vector<Event> ev;
        renderMelody(g, 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, ev);
        return hitKeys(ev);
    }));

    CHECK(findsDifferentSeed([](int nonce)
    {
        GrooveParams g;
        g.bassEnabled = true;
        g.bassNonce = nonce;
        g.bassVariation = 4;
        std::vector<Event> ev;
        renderBass(g, 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, ev);
        return hitKeys(ev);
    }));

    CHECK(findsDifferentSeed([](int nonce)
    {
        GrooveParams g;
        g.chordsEnabled = true;
        g.chordNonce = nonce;
        g.chordVariation = 4;
        std::vector<Event> ev;
        renderChords(g, 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, ev);
        return hitKeys(ev);
    }));

    CHECK(findsDifferentSeed([](int nonce)
    {
        GrooveParams g;
        g.arpEnabled = true;
        g.arpNonce = nonce;
        g.arpVariation = 4;
        std::vector<Event> ev;
        renderArp(g, 0.0, 4.0, nullptr, 0, 0, Mode::Ionian, ev);
        return hitKeys(ev);
    }));
}

TEST_CASE("funk drums keep a syncopated backbeat pocket", "[genre][funk]")
{
    auto check = [](int nonce)
    {
        auto g = drums(DrumStyle::Funk);
        g.drumNonce = nonce;
        g.drumVariation = 4;
        std::vector<Event> ev;
        renderDrums(g, 0.0, 4.0, ev);
        const auto ons = noteOns(ev, 0.0, 4.0);
        CHECK(countNear(ons, 38, 1.0, 0.12) >= 1);
        CHECK(countNear(ons, 38, 3.0, 0.12) >= 1);
        CHECK(downbeatKicks(ev) < 4);

        int loud = 0, quiet = 127, andHats = 0;
        for (const auto& h : ons)
        {
            if (h.note == 38)
            {
                loud = std::max(loud, h.velocity);
                quiet = std::min(quiet, h.velocity);
            }
            if (h.note != 42) continue;
            const double frac = std::fmod(h.ppq, 1.0);
            if (frac > 0.35 && frac < 0.75 && h.velocity >= 90)
                ++andHats;
        }
        CHECK(loud >= 100);
        CHECK(quiet < 70);
        CHECK(andHats >= 2);
    };
    check(0);
    check(1);
    check(7);
}
