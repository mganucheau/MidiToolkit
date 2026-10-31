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
