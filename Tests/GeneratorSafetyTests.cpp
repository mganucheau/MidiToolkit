#include "GeneratorEngines.h"
#include "GrooveEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <limits>
#include <set>
#include <vector>

using namespace pflow;
using namespace pflow::gen;

namespace {

GrooveParams partGroove(Part part, int genre)
{
    GrooveParams g;
    switch (part)
    {
        case Part::Arp:
            g.arpEnabled = true;
            g.arpGenreIndex = genre;
            g.arpDensity = 100;
            break;
        case Part::Melody:
            g.melodyEnabled = true;
            g.melodyGenreIndex = genre;
            break;
        case Part::Chords:
            g.chordsEnabled = true;
            g.chordGenreIndex = genre;
            break;
        case Part::Bass:
            g.bassEnabled = true;
            g.bassGenreIndex = genre;
            break;
        case Part::Drums:
            g.drumsEnabled = true;
            g.drumStyleIndex = genre;
            g.drumFill = 0;
            break;
        case Part::Count:
            break;
    }
    return g;
}

int noteOns(const std::vector<Event>& events)
{
    int n = 0;
    for (const auto& e : events)
        if (e.velocity > 0 && std::isfinite(e.ppq))
            ++n;
    return n;
}

} // namespace

TEST_CASE("every generator part and genre stays bounded", "[generator]")
{
    for (int part = 0; part < (int) Part::Count; ++part)
    {
        for (int genre = 0; genre < (int) DrumStyle::Count; ++genre)
        {
            const auto g = partGroove((Part) part, genre);
            const auto clip = renderPartClip((Part) part, g, 0, Mode::Ionian);
            REQUIRE(clip.notes.size() < 8000);
            for (const auto& n : clip.notes)
            {
                REQUIRE(std::isfinite(n.start));
                REQUIRE(std::isfinite(n.len));
                REQUIRE(n.pitch >= 0);
                REQUIRE(n.pitch <= 127);
            }
        }
    }
}

TEST_CASE("a far playhead does not scan the whole song", "[generator]")
{
    GrooveParams g;
    g.arpEnabled = true;
    g.arpDensity = 100;
    g.arpGenreIndex = (int) DrumStyle::Techno;
    std::vector<Event> out;
    const auto t0 = std::chrono::steady_clock::now();
    renderArp(g, 1.0e6, 1.0e6 + 1.0, nullptr, 0, 0, Mode::Ionian, out);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    REQUIRE(ms < 500);
    REQUIRE(out.size() < 4000);
    REQUIRE(noteOns(out) > 0);

    g.arpEnabled = false;
    g.melodyEnabled = true;
    g.melodyGenreIndex = (int) DrumStyle::Dub;
    out.clear();
    renderMelody(g, 1.0e7, 1.0e7 + 1.0e12, nullptr, 0, 0, Mode::Aeolian, out);
    REQUIRE(out.size() < 8000);
    REQUIRE(noteOns(out) > 0);

    g.melodyEnabled = false;
    g.drumsEnabled = true;
    g.drumStyleIndex = (int) DrumStyle::Dnb;
    g.drumFill = 0;
    out.clear();
    renderDrums(g, std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity(), out);
    REQUIRE(out.empty());
}

TEST_CASE("non-finite timing sorts without crashing", "[generator]")
{
    std::vector<Event> events = {
        { std::numeric_limits<double>::quiet_NaN(), 60, 100 },
        { 0.0, 60, 0 },
        { 1.0, 64, 90 },
        { 1.5, 64, 0 },
    };
    const auto notes = eventsToRollNotes(events);
    REQUIRE(notes.size() < 8);
}

TEST_CASE("complexity on a selection leaves the other notes", "[groove]")
{
    std::vector<RollNote> notes;
    for (int i = 0; i < 8; ++i)
    {
        RollNote n;
        n.id = i;
        n.pitch = 60 + (i % 5);
        n.start = (double) (i * 4);
        n.len = 1.0;
        notes.push_back(n);
    }
    std::set<int> selected { 4, 5 };
    NoteProcessScope scope;
    scope.selectedIds = &selected;
    GrooveParams g;
    g.complexityTarget = 100;
    const auto out = applyGroove(notes, g, 20, ClipKind::Lead, 0, Mode::Ionian, scope);
    for (const auto& src : notes)
    {
        if (selected.count(src.id) != 0)
            continue;
        bool found = false;
        for (const auto& n : out)
        {
            if (n.id != src.id)
                continue;
            found = true;
            CHECK(n.pitch == src.pitch);
            CHECK(n.start == src.start);
        }
        CHECK(found);
    }
}
