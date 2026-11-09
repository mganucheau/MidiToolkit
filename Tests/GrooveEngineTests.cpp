#include "GrooveEngine.h"
#include "TestHelpers.h"
#include <catch2/catch_approx.hpp>

using namespace pflow;
using Catch::Approx;

namespace {

std::vector<RollNote> testNotes()
{
    return {
        { 0, 60, 0.0, 4.0 },
        { 1, 62, 2.0, 2.0 },
        { 2, 64, 5.0, 1.0 },
        { 3, 65, 6.0, 2.0 },
        { 4, 67, 14.0, 2.0 },
        { 5, 69, 31.0, 3.0 },
    };
}

} // namespace

TEST_CASE("knob defs match DAW-aligned ranges", "[groove]")
{
    REQUIRE(kKnobDefs.size() == 6);
    CHECK(juce::String(kKnobDefs[0].key) == "swing");
    CHECK(kKnobDefs[0].min == 0);   CHECK(kKnobDefs[0].max == 100); CHECK(kKnobDefs[0].def == 0);
    CHECK_FALSE(kKnobDefs[0].bipolar());
    CHECK_FALSE(kKnobDefs[0].fillFromDefault());

    CHECK(juce::String(kKnobDefs[1].key) == "pocket");
    CHECK(kKnobDefs[1].min == -100); CHECK(kKnobDefs[1].max == 100); CHECK(kKnobDefs[1].def == 0);
    CHECK(kKnobDefs[1].bipolar());

    CHECK(juce::String(kKnobDefs[3].key) == "dynamics");
    CHECK(kKnobDefs[3].min == -100); CHECK(kKnobDefs[3].max == 100); CHECK(kKnobDefs[3].def == 0);
    CHECK(kKnobDefs[3].bipolar());

    CHECK(juce::String(kKnobDefs[4].key) == "length");
    CHECK(kKnobDefs[4].min == 25);  CHECK(kKnobDefs[4].max == 200); CHECK(kKnobDefs[4].def == 100);
    CHECK(kKnobDefs[4].fillFromDefault());

    CHECK(juce::String(kKnobDefs[5].key) == "intensity");
    CHECK(kKnobDefs[5].min == 0);   CHECK(kKnobDefs[5].max == 200); CHECK(kKnobDefs[5].def == 100);
    CHECK(kKnobDefs[5].fillFromDefault());
}

TEST_CASE("GrooveParams defaults and activeCount", "[groove]")
{
    GrooveParams k;
    CHECK(k.isDefault());
    CHECK(k.activeCount() == 0);
    CHECK(k.intensity == 100);
    CHECK(k.length == 100);
    CHECK(k.swingBase == SwingBase::Eighth);

    k.swing = 30;
    k.intensity = 60;
    CHECK(k.activeCount() == 2);
    CHECK_FALSE(k.isDefault());

    k.set(0, 999);   // clamps to knob max
    CHECK(k.swing == 100);
    k.set(1, -999);
    CHECK(k.pocket == -100);

    GrooveParams t;
    t.swingGridIndex = 0;
    t.swingBase = SwingBase::Sixteenth;
    CHECK(t.activeCount() == 1);
    CHECK_FALSE(t.isDefault());
}

TEST_CASE("baseVel is deterministic and in range", "[groove]")
{
    for (int id : { 0, 1, 2, 3, 4, 5, 17, 100 })
    {
        const double v = baseVel(id);
        CHECK(v >= 0.45);
        CHECK(v <= 1.01);
        CHECK(baseVel(id) == Approx(v));   // stable
    }
}

TEST_CASE("swing delays only offbeat 8ths at 1/8 base", "[groove]")
{
    GrooveParams k;
    k.swing = 50;   // half of max delay (period/2 = 1.0 → +0.5)
    const auto g = applyGroove(testNotes(), k);
    CHECK(g[0].start == Approx(0.0));    // on-beat 8th
    CHECK(g[1].start == Approx(2.5));    // offbeat 8th at step 2
    CHECK(g[2].start == Approx(5.0));    // not on offbeat grid
    CHECK(g[3].start == Approx(6.5));    // offbeat 8th at step 6
    CHECK(g[4].start == Approx(14.5));
    CHECK(g[5].start == Approx(31.5));
}

TEST_CASE("swing at 1/16 base delays odd sixteenth slots", "[groove]")
{
    GrooveParams k;
    k.swing = 100;
    k.swingGridIndex = 0;
    k.swingBase = SwingBase::Sixteenth;
    // Max delay = 0.5 step; odd floor(start) slots move.
    const auto g = applyGroove(testNotes(), k);
    CHECK(g[0].start == Approx(0.0));    // slot 0 even
    CHECK(g[1].start == Approx(2.0));    // slot 2 even
    CHECK(g[2].start == Approx(5.5));    // slot 5 odd → +0.5
    CHECK(g[3].start == Approx(6.0));    // slot 6 even
}

TEST_CASE("pocket shifts weak beats more than strong beats", "[groove]")
{
    GrooveParams k;
    k.pocket = 100;   // laid-back loose
    // Downbeat (weight 1) vs 16th (weight 0.38) — weak moves further behind.
    std::vector<RollNote> notes = {
        { 0, 60, 0.0, 1.0 },
        { 1, 62, 1.0, 1.0 },
    };
    const auto g = applyGroove(notes, k);
    CHECK(g[0].start > notes[0].start);
    CHECK(g[1].start > notes[1].start);
    CHECK((g[1].start - notes[1].start) > (g[0].start - notes[0].start));
}

TEST_CASE("humanize applies deterministic per-id jitter", "[groove]")
{
    GrooveParams k;
    k.humanize = 60;
    const auto g = applyGroove(testNotes(), k);
    const auto g2 = applyGroove(testNotes(), k);
    for (size_t i = 0; i < g.size(); ++i)
    {
        CHECK(g[i].start == Approx(g2[i].start));
        // Jittered away from original (except pathological zero hash)
        CHECK(std::abs(g[i].start - testNotes()[i].start) < 1.0);
    }
}

TEST_CASE("length scales note duration; floor at 0.5", "[groove]")
{
    GrooveParams k;
    k.length = 25;
    const auto g = applyGroove(testNotes(), k);
    CHECK(g[0].len == Approx(1.0));    // 4 * 0.25
    CHECK(g[1].len == Approx(0.5));    // 2 * 0.25 floored
    CHECK(g[2].len == Approx(0.5));    // 1 * 0.25 floored
}

TEST_CASE("applyGroove never mutates its input", "[groove]")
{
    const auto notes = testNotes();
    GrooveParams k;
    k.swing = 75;
    k.pocket = 100;
    k.humanize = 100;
    k.length = 25;
    (void) applyGroove(notes, k);
    CHECK(notes[1].start == Approx(2.0));
    CHECK(notes[1].len == Approx(2.0));
}

TEST_CASE("noteVelocity: intensity 100 leaves base unchanged", "[groove]")
{
    GrooveParams k;   // intensity 100, dynamics 0
    const auto notes = testNotes();
    CHECK(noteVelocity(notes[0], k) == Approx(baseVel(0)));
    CHECK(noteVelocity(notes[1], k) == Approx(baseVel(1)));
}

TEST_CASE("noteVelocity: intensity scales flat", "[groove]")
{
    GrooveParams k;
    k.intensity = 50;
    const auto notes = testNotes();
    CHECK(noteVelocity(notes[0], k) == Approx(baseVel(0) * 0.5));
}

TEST_CASE("noteVelocity: positive dynamics accents downbeats", "[groove]")
{
    GrooveParams k;
    k.dynamics = 100;
    k.intensity = 100;
    RollNote down { 0, 60, 0.0, 1.0 };
    RollNote soft { 0, 60, 15.0, 1.0 };
    const double vd = noteVelocity(down, k);
    const double vs = noteVelocity(soft, k);
    CHECK(vd > vs);
    CHECK(vd == Approx(std::min(1.0, baseVel(0) * 1.0)));  // full weight
}

TEST_CASE("noteVelocity: negative dynamics inverts accents", "[groove]")
{
    GrooveParams k;
    k.dynamics = -100;
    k.intensity = 100;
    RollNote down { 0, 60, 0.0, 1.0 };
    RollNote off  { 0, 60, 15.0, 1.0 };
    CHECK(noteVelocity(off, k) > noteVelocity(down, k));
}

TEST_CASE("noteVelocity clamps to [0.04, 1]", "[groove]")
{
    GrooveParams k;
    k.intensity = 1;
    RollNote n { 0, 60, 0.0, 1.0 };
    CHECK(noteVelocity(n, k) == Approx(0.04));

    k.intensity = 200;
    k.dynamics = 100;
    CHECK(noteVelocity(n, k) == Approx(1.0));
}

TEST_CASE("noteVelocityWithBase uses file velocities as the base", "[groove]")
{
    GrooveParams k;
    k.intensity = 100;
    RollNote n { 42, 60, 0.0, 1.0 };
    CHECK(noteVelocityWithBase(0.5, n, k) == Approx(0.5));

    k.intensity = 200;
    CHECK(noteVelocityWithBase(0.5, n, k) == Approx(1.0));   // clamped
}

TEST_CASE("effects lock freezes follow-clip complexity", "[groove]")
{
    GrooveParams src;
    src.delayAmount = 55;
    src.complexityTarget = -1;

    GrooveParams locked;
    captureGrooveSectionLock(src, locked, toolkitLock::Effects, 37);
    CHECK(locked.complexityTarget == 37);
    CHECK(locked.delayAmount == 55);

    GrooveParams other;
    other.complexityTarget = 90;
    other.delayAmount = 0;
    other.swing = 12;
    applyGrooveSectionLock(locked, other, toolkitLock::Effects);
    CHECK(other.complexityTarget == 37);
    CHECK(other.delayAmount == 55);
    CHECK(other.swing == 12);

    GrooveParams explicitTarget;
    explicitTarget.complexityTarget = 80;
    GrooveParams kept;
    captureGrooveSectionLock(explicitTarget, kept, toolkitLock::Effects, 10);
    CHECK(kept.complexityTarget == 80);
}

TEST_CASE("performance lock keeps articulation length when timing is unlocked", "[groove]")
{
    GrooveParams src;
    src.articulationIndex = (int) Articulation::Staccato;
    src.articulationStrength = 80;
    src.length = 40;
    src.swing = 25;

    GrooveParams locked;
    captureGrooveSectionLock(src, locked, toolkitLock::Performance);
    CHECK(locked.length == 40);
    CHECK(locked.swing == 0);

    GrooveParams file;
    file.length = 100;
    file.swing = 10;
    applyGrooveSectionLock(locked, file, toolkitLock::Performance);
    CHECK(file.length == 40);
    CHECK(file.swing == 10);
    CHECK(file.articulationIndex == (int) Articulation::Staccato);

    GrooveParams both = src;
    both.length = 70;
    GrooveParams bothLocked;
    captureGrooveSectionLock(both, bothLocked, toolkitLock::Timing | toolkitLock::Performance);
    GrooveParams timed;
    timed.length = 100;
    applyGrooveSectionLock(bothLocked, timed, toolkitLock::Timing | toolkitLock::Performance);
    CHECK(timed.length == 70);
    CHECK(timed.swing == 25);
}

TEST_CASE("grooveValueText formats bipolar and percent", "[groove]")
{
    CHECK(grooveValueText(kKnobDefs[1], -12) == "-12");
    CHECK(grooveValueText(kKnobDefs[1], 12) == "+12");
    CHECK(grooveValueText(kKnobDefs[0], 50) == "50%");
    CHECK(grooveValueText(kKnobDefs[5], 100) == "100%");
}

TEST_CASE("drum complexity keeps kick/snare backbeat anchors", "[groove]")
{
    // Channel-1 backbeat: kick on 1/3, snare on 2/4, hats on 8ths.
    std::vector<RollNote> drums = {
        { 0, 36, 0.0, 0.5, 100, 1 },   // kick
        { 1, 42, 0.0, 0.5, 80, 1 },    // hat
        { 2, 42, 2.0, 0.5, 70, 1 },
        { 3, 38, 4.0, 0.5, 110, 1 },   // snare
        { 4, 42, 4.0, 0.5, 80, 1 },
        { 5, 42, 6.0, 0.5, 70, 1 },
        { 6, 36, 8.0, 0.5, 100, 1 },
        { 7, 42, 8.0, 0.5, 80, 1 },
        { 8, 42, 10.0, 0.5, 70, 1 },
        { 9, 38, 12.0, 0.5, 110, 1 },
        { 10, 42, 12.0, 0.5, 80, 1 },
        { 11, 42, 14.0, 0.5, 70, 1 },
    };

    GrooveParams k;
    k.complexityTarget = 85;
    const auto denser = applyGroove(drums, k, 40, ClipKind::Drums);

    auto hasKickAt = [&](double step)
    {
        for (const auto& n : denser)
            if (n.pitch == 36 && std::abs(n.start - step) < 0.35)
                return true;
        return false;
    };
    auto hasSnareAt = [&](double step)
    {
        for (const auto& n : denser)
            if (n.pitch == 38 && std::abs(n.start - step) < 0.35)
                return true;
        return false;
    };
    CHECK(hasKickAt(0.0));
    CHECK(hasKickAt(8.0));
    CHECK(hasSnareAt(4.0));
    CHECK(hasSnareAt(12.0));

    // New pitches must stay on the kit map.
    for (const auto& n : denser)
        CHECK((n.pitch >= 35 && n.pitch <= 81));

    k.complexityTarget = 15;
    const auto sparser = applyGroove(drums, k, 70, ClipKind::Drums);
    CHECK(sparser.size() <= drums.size());
    bool kickOk = false, snareOk = false;
    for (const auto& n : sparser)
    {
        if (n.pitch == 36 && std::abs(n.start - 0.0) < 0.35) kickOk = true;
        if (n.pitch == 38 && std::abs(n.start - 4.0) < 0.35) snareOk = true;
    }
    CHECK(kickOk);
    CHECK(snareOk);
}

TEST_CASE("piano complexity adds in-scale tones, not chromatic neighbors", "[groove]")
{
    // C major triad repeating.
    std::vector<RollNote> piano = {
        { 0, 60, 0.0, 4.0, 90, 1 },
        { 1, 64, 0.0, 4.0, 85, 1 },
        { 2, 67, 0.0, 4.0, 85, 1 },
        { 3, 60, 8.0, 4.0, 90, 1 },
        { 4, 64, 8.0, 4.0, 85, 1 },
        { 5, 67, 8.0, 4.0, 85, 1 },
    };

    GrooveParams k;
    k.complexityTarget = 80;
    const auto denser = applyGroove(piano, k, 35, ClipKind::Keys, 0, Mode::Ionian);

    for (const auto& n : denser)
    {
        const bool fromSource = (n.pitch == 60 || n.pitch == 64 || n.pitch == 67);
        CHECK((fromSource || pitchInScale(n.pitch, 0, Mode::Ionian)));
        // No random ±1/±2 chromatic ornaments off the triad.
        CHECK(n.pitch != 61);
        CHECK(n.pitch != 62);
        CHECK(n.pitch != 63);
        CHECK(n.pitch != 65);
        CHECK(n.pitch != 66);
    }
}

TEST_CASE("drum variation leaves kick/snare anchors in place", "[groove]")
{
    std::vector<RollNote> drums = {
        { 0, 36, 0.0, 0.5, 100, 1 },
        { 1, 42, 2.0, 0.5, 70, 1 },
        { 2, 38, 4.0, 0.5, 110, 1 },
        { 3, 42, 6.0, 0.5, 70, 1 },
        { 4, 36, 8.0, 0.5, 100, 1 },
        { 5, 42, 10.0, 0.5, 70, 1 },
        { 6, 38, 12.0, 0.5, 110, 1 },
        { 7, 42, 14.0, 0.5, 70, 1 },
    };
    GrooveParams k;
    k.variationIndex = 5;
    const auto varied = applyGroove(drums, k, -1, ClipKind::Drums);
    auto kickAt = [&](double step)
    {
        for (const auto& n : varied)
            if (n.pitch == 36 && std::abs(n.start - step) < 0.2)
                return true;
        return false;
    };
    CHECK(kickAt(0.0));
    CHECK(kickAt(8.0));
}
