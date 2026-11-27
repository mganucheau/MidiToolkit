#include "HostParams.h"
#include <algorithm>
#include <cmath>

namespace pflow {

namespace {

constexpr int kVersionHint = 1;

struct ScopedSuppress
{
    ScopedSuppress(std::atomic<int>& c, std::atomic<juce::Thread::ThreadID>& t) : counter(c)
    {
        t.store(juce::Thread::getCurrentThreadId());
        ++counter;
    }
    ~ScopedSuppress() { --counter; }
    std::atomic<int>& counter;
};

juce::ParameterID pid(const juce::String& id) { return juce::ParameterID(id, kVersionHint); }

template <typename E, int N = (int) E::Count>
juce::StringArray enumLabels(const char* (*label)(E))
{
    juce::StringArray out;
    for (int i = 0; i < N; ++i)
        out.add(label((E) i));
    return out;
}

juce::StringArray quantizeGridLabels()  { return enumLabels<QuantizeGrid>(quantizeGridLabel); }
juce::StringArray articulationLabels()  { return enumLabels<Articulation>(articulationLabel); }
juce::StringArray sustainLabels()       { return enumLabels<SustainPedalMode>(sustainPedalLabel); }
juce::StringArray delayTimeLabels()     { return enumLabels<DelayTime>(delayTimeLabel); }
juce::StringArray lengthLabels()        { return enumLabels<GeneratorLength>(generatorLengthLabel); }
juce::StringArray formLabels()          { return enumLabels<GeneratorForm>(generatorFormLabel); }

juce::StringArray modeLabels()
{
    juce::StringArray out;
    for (int i = 0; i < kNumModes; ++i)
        out.add(modeName((Mode) i));
    return out;
}

juce::StringArray keyLabels()
{
    juce::StringArray out { "Auto" };
    for (int i = 0; i < 12; ++i)
        out.add(kNoteNames[(size_t) i]);
    return out;
}

constexpr double kTempoMult[] = { 1.0, 0.5, 2.0 };
constexpr int kExtendMult[] = { 1, 2, 4, 8 };

int tempoIndexFor(double mult)
{
    return mult < 0.75 ? 1 : (mult > 1.5 ? 2 : 0);
}

int extendIndexFor(int mult)
{
    return mult == 2 ? 1 : (mult == 4 ? 2 : (mult == 8 ? 3 : 0));
}

constexpr bool GrooveParams::* kGeneratorEnabled[] = {
    &GrooveParams::arpEnabled, &GrooveParams::melodyEnabled, &GrooveParams::chordsEnabled,
    &GrooveParams::bassEnabled, &GrooveParams::drumsEnabled
};

} // namespace

HostParams::HostParams() = default;

HostParams::~HostParams()
{
    for (auto& b : bindings_)
        if (b.param != nullptr)
            b.param->removeListener(this);
    if (browserParam_ != nullptr)
        browserParam_->removeListener(this);
}

int HostParams::rawValue(const juce::RangedAudioParameter& p)
{
    return juce::roundToInt(p.convertFrom0to1(p.getValue()));
}

void HostParams::writeRaw(juce::RangedAudioParameter& p, int value, bool userGesture)
{
    if (rawValue(p) == value)
        return;
    const float norm = p.convertTo0to1((float) value);
    if (userGesture)
        p.beginChangeGesture();
    p.setValueNotifyingHost(norm);
    if (userGesture)
        p.endChangeGesture();
}

void HostParams::add(juce::RangedAudioParameter* param, Target target,
                     std::function<int(const GrooveParams&, const ClipEdit&, double)> get,
                     std::function<void(GrooveParams&, ClipEdit&, double&, int)> set)
{
    Binding b;
    b.param = param;
    b.target = target;
    b.get = std::move(get);
    b.set = std::move(set);
    bindings_.push_back(std::move(b));
}

void HostParams::addGrooveInt(juce::AudioProcessor& owner, const char* id, const char* name,
                              int lo, int hi, int def, int GrooveParams::* field,
                              std::function<juce::String(int)> text)
{
    auto attrs = juce::AudioParameterIntAttributes();
    if (text)
        attrs = attrs.withStringFromValueFunction(
            [text](int v, int) { return text(v); });
    auto* p = new juce::AudioParameterInt(pid(id), name, lo, hi, def, attrs);
    owner.addParameter(p);
    add(p, Target::Groove,
        [field](const GrooveParams& g, const ClipEdit&, double) { return g.*field; },
        [field, lo, hi](GrooveParams& g, ClipEdit&, double&, int v) { g.*field = juce::jlimit(lo, hi, v); });
}

void HostParams::addGrooveChoice(juce::AudioProcessor& owner, const char* id, const char* name,
                                 const juce::StringArray& labels, int def, int GrooveParams::* field)
{
    auto* p = new juce::AudioParameterChoice(pid(id), name, labels, def);
    owner.addParameter(p);
    const int hi = labels.size() - 1;
    add(p, Target::Groove,
        [field](const GrooveParams& g, const ClipEdit&, double) { return g.*field; },
        [field, hi](GrooveParams& g, ClipEdit&, double&, int v) { g.*field = juce::jlimit(0, hi, v); });
}

void HostParams::addGrooveBool(juce::AudioProcessor& owner, const char* id, const char* name,
                               bool def, bool GrooveParams::* field)
{
    auto* p = new juce::AudioParameterBool(pid(id), name, def);
    owner.addParameter(p);
    add(p, Target::Groove,
        [field](const GrooveParams& g, const ClipEdit&, double) { return g.*field ? 1 : 0; },
        [field](GrooveParams& g, ClipEdit&, double&, int v) { g.*field = v != 0; });
}

void HostParams::addEditBool(juce::AudioProcessor& owner, const char* id, const char* name,
                             bool def, bool ClipEdit::* field)
{
    auto* p = new juce::AudioParameterBool(pid(id), name, def);
    owner.addParameter(p);
    add(p, Target::Edit,
        [field](const GrooveParams&, const ClipEdit& e, double) { return e.*field ? 1 : 0; },
        [field](GrooveParams&, ClipEdit& e, double&, int v) { e.*field = v != 0; });
}

void HostParams::create(juce::AudioProcessor& owner)
{
    baseIndex_ = owner.getParameters().size();

    // Browser position first: it is the one control that is not Toolkit state.
    {
        auto attrs = juce::AudioParameterIntAttributes().withStringFromValueFunction(
            [](int v, int) { return juce::String(v); });
        auto* p = new juce::AudioParameterInt(pid(hostparam::kBrowserIndexId), "Browser Index",
                                              0, hostparam::kBrowserIndexMax, 0, attrs);
        owner.addParameter(p);
        browserParam_ = p;
    }

    const auto offOr = [](int off, const juce::String& prefix)
    {
        return [off, prefix](int v) { return v == off ? juce::String("Off") : prefix + juce::String(v); };
    };

    // ── Playback ────────────────────────────────────────────────────────────
    {
        auto* p = new juce::AudioParameterChoice(pid("tk_time"), "Time",
                                                 juce::StringArray { "1x", "0.5x", "2x" }, 0);
        owner.addParameter(p);
        add(p, Target::Tempo,
            [](const GrooveParams&, const ClipEdit&, double m) { return tempoIndexFor(m); },
            [](GrooveParams&, ClipEdit&, double& m, int v)
            {
                m = kTempoMult[juce::jlimit(0, 2, v)];
            });
    }
    {
        auto* p = new juce::AudioParameterChoice(pid("tk_extend"), "Extend",
                                                 juce::StringArray { "Off", "x2", "x4", "x8" }, 0);
        owner.addParameter(p);
        add(p, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double) { return extendIndexFor(e.extendMult); },
            [](GrooveParams&, ClipEdit& e, double&, int v)
            {
                e.extendMult = kExtendMult[juce::jlimit(0, 3, v)];
            });
    }

    // ── Timing ──────────────────────────────────────────────────────────────
    addGrooveChoice(owner, "tk_quantize_grid", "Quantize Grid", quantizeGridLabels(),
                    (int) QuantizeGrid::Eighth, &GrooveParams::quantizeGridIndex);
    addGrooveInt(owner, "tk_quantize_strength", "Quantize Strength", 0, 100, 0,
                 &GrooveParams::quantizeStrength);
    {
        const juce::StringArray labels { "1/16", "1/8", "1/4", "1/2", "1", "2" };
        auto* p = new juce::AudioParameterChoice(pid("tk_swing_grid"), "Swing Grid", labels, 1);
        owner.addParameter(p);
        add(p, Target::Groove,
            [](const GrooveParams& g, const ClipEdit&, double) { return g.swingGridIndex; },
            [](GrooveParams& g, ClipEdit&, double&, int v)
            {
                g.swingGridIndex = juce::jlimit(0, 5, v);
                g.swingBase = g.swingGridIndex == 0 ? SwingBase::Sixteenth : SwingBase::Eighth;
            });
    }
    addGrooveInt(owner, "tk_swing", "Swing Amount", 0, 100, 0, &GrooveParams::swing);
    idxLength_ = (int) bindings_.size();
    addGrooveInt(owner, "tk_length", "Length", 25, 200, 100, &GrooveParams::length);
    addGrooveInt(owner, "tk_pocket", "Pocket", -100, 100, 0, &GrooveParams::pocket);
    addGrooveInt(owner, "tk_humanize", "Humanize", 0, 100, 0, &GrooveParams::humanize);

    // ── Performance ─────────────────────────────────────────────────────────
    idxArticulation_ = (int) bindings_.size();
    addGrooveChoice(owner, "tk_articulation", "Articulation", articulationLabels(),
                    (int) Articulation::Off, &GrooveParams::articulationIndex);
    idxArticulationStrength_ = (int) bindings_.size();
    addGrooveInt(owner, "tk_articulation_strength", "Articulation Strength", 0, 100, 0,
                 &GrooveParams::articulationStrength);
    idxDynamics_ = (int) bindings_.size();
    addGrooveInt(owner, "tk_dynamics", "Dynamics", -100, 100, 0, &GrooveParams::dynamics);
    idxIntensity_ = (int) bindings_.size();
    addGrooveInt(owner, "tk_intensity", "Intensity", 0, 200, 100, &GrooveParams::intensity);
    {
        auto* p = new juce::AudioParameterInt(pid("tk_velocity_min"), "Velocity Min", 1, 127, 1);
        owner.addParameter(p);
        add(p, Target::Groove,
            [](const GrooveParams& g, const ClipEdit&, double) { return g.velocityRangeLo; },
            [](GrooveParams& g, ClipEdit&, double&, int v)
            {
                g.velocityRangeLo = juce::jlimit(1, 127, v);
                g.velocityRangeHi = juce::jmax(g.velocityRangeHi, g.velocityRangeLo);
            });
    }
    {
        auto* p = new juce::AudioParameterInt(pid("tk_velocity_max"), "Velocity Max", 1, 127, 127);
        owner.addParameter(p);
        add(p, Target::Groove,
            [](const GrooveParams& g, const ClipEdit&, double) { return g.velocityRangeHi; },
            [](GrooveParams& g, ClipEdit&, double&, int v)
            {
                g.velocityRangeHi = juce::jlimit(1, 127, v);
                g.velocityRangeLo = juce::jmin(g.velocityRangeLo, g.velocityRangeHi);
            });
    }
    addGrooveChoice(owner, "tk_sustain", "Sustain Pedal", sustainLabels(),
                    (int) SustainPedalMode::Off, &GrooveParams::sustainPedalMode);

    // ── Pitch & Scale ───────────────────────────────────────────────────────
    {
        auto attrs = juce::AudioParameterIntAttributes().withStringFromValueFunction(
            [](int v, int) { return v < 0 ? juce::String("Clip") : juce::String(v); });
        auto* p = new juce::AudioParameterInt(pid("tk_octave"), "Octave", -1, 6, -1, attrs);
        owner.addParameter(p);
        add(p, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double) { return e.octave; },
            [](GrooveParams&, ClipEdit& e, double&, int v) { e.octave = juce::jlimit(-1, 6, v); });
    }
    {
        auto* p = new juce::AudioParameterInt(pid("tk_transpose"), "Transpose", -12, 12, 0);
        owner.addParameter(p);
        add(p, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double) { return e.pitchShift; },
            [](GrooveParams&, ClipEdit& e, double&, int v) { e.pitchShift = juce::jlimit(-12, 12, v); });
    }
    addEditBool(owner, "tk_pitch_input", "Pitch Input", false, &ClipEdit::pitchInput);
    {
        auto* p = new juce::AudioParameterChoice(pid("tk_octave_range"), "Octave Range",
                                                 juce::StringArray { "Off", "1", "2", "3" }, 0);
        owner.addParameter(p);
        add(p, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double) { return e.octaveRange; },
            [](GrooveParams&, ClipEdit& e, double&, int v) { e.octaveRange = juce::jlimit(0, 3, v); });
    }
    {
        auto* p = new juce::AudioParameterChoice(pid("tk_outside_range"), "Outside Range",
                                                 juce::StringArray { "Clip", "Fold" }, 1);
        owner.addParameter(p);
        add(p, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double)
            {
                return e.outsideRange == OutsideRangeMode::Clip ? 0 : 1;
            },
            [](GrooveParams&, ClipEdit& e, double&, int v)
            {
                e.outsideRange = v == 0 ? OutsideRangeMode::Clip : OutsideRangeMode::Fold;
            });
    }
    {
        auto* p = new juce::AudioParameterChoice(pid("tk_key"), "Key", keyLabels(), 0);
        owner.addParameter(p);
        add(p, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double) { return e.root + 1; },
            [](GrooveParams&, ClipEdit& e, double&, int v) { e.root = juce::jlimit(0, 12, v) - 1; });
    }
    idxMode_ = (int) bindings_.size();
    {
        auto* p = new juce::AudioParameterChoice(pid("tk_mode"), "Mode", modeLabels(), 0);
        owner.addParameter(p);
        add(p, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double) { return (int) e.mode; },
            [](GrooveParams&, ClipEdit& e, double&, int v)
            {
                e.mode = (Mode) juce::jlimit(0, kNumModes - 1, v);
            });
    }
    idxFit_ = (int) bindings_.size();
    addEditBool(owner, "tk_fit_scale", "Fit to Scale", false, &ClipEdit::fitScale);
    idxMap_ = (int) bindings_.size();
    addEditBool(owner, "tk_map_root", "Map to Root", false, &ClipEdit::mapToRoot);
    {
        auto attrs = juce::AudioParameterIntAttributes().withStringFromValueFunction(
            [](int v, int) { return pitchName(v); });
        auto* lo = new juce::AudioParameterInt(pid("tk_pitch_min"), "Pitch Range Min", 0, 127, 0, attrs);
        owner.addParameter(lo);
        add(lo, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double) { return e.pitchMin; },
            [](GrooveParams&, ClipEdit& e, double&, int v)
            {
                e.pitchMin = juce::jlimit(0, 127, v);
                e.pitchMax = juce::jmax(e.pitchMax, e.pitchMin);
            });
        auto* hi = new juce::AudioParameterInt(pid("tk_pitch_max"), "Pitch Range Max", 0, 127, 127, attrs);
        owner.addParameter(hi);
        add(hi, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double) { return e.pitchMax; },
            [](GrooveParams&, ClipEdit& e, double&, int v)
            {
                e.pitchMax = juce::jlimit(0, 127, v);
                e.pitchMin = juce::jmin(e.pitchMin, e.pitchMax);
            });
    }
    {
        auto* p = new juce::AudioParameterChoice(pid("tk_filter_type"), "Filter Type",
                                                 juce::StringArray { "Mute", "Fold", "Off" }, 2);
        owner.addParameter(p);
        add(p, Target::Edit,
            [](const GrooveParams&, const ClipEdit& e, double)
            {
                if (e.noteFilterType == NoteFilterType::Fold) return 1;
                if (e.noteFilterType == NoteFilterType::Off) return 2;
                return 0;
            },
            [](GrooveParams&, ClipEdit& e, double&, int v)
            {
                e.noteFilterType = v == 1 ? NoteFilterType::Fold
                                 : v == 2 ? NoteFilterType::Off
                                          : NoteFilterType::Mute;
            });
    }
    {
        static const char* const kIdSuffix[12] = { "c", "cs", "d", "ds", "e", "f",
                                                   "fs", "g", "gs", "a", "as", "b" };
        for (int pc = 0; pc < 12; ++pc)
        {
            auto* p = new juce::AudioParameterBool(
                pid(juce::String("tk_notefilter_") + kIdSuffix[pc]),
                juce::String("Note Filter ") + kNoteNames[(size_t) pc], true);
            owner.addParameter(p);
            add(p, Target::Edit,
                [pc](const GrooveParams&, const ClipEdit& e, double) { return e.isNoteFilterEnabled(pc) ? 1 : 0; },
                [pc](GrooveParams&, ClipEdit& e, double&, int v) { e.setNoteFilterEnabled(pc, v != 0); });
        }
    }

    // ── Form ────────────────────────────────────────────────────────────────
    {
        auto attrs = juce::AudioParameterIntAttributes().withStringFromValueFunction(
            [](int v, int) { return v < 0 ? juce::String("Clip") : juce::String(v); });
        auto* p = new juce::AudioParameterInt(pid("tk_complexity"), "Complexity", -1, 100, -1, attrs);
        owner.addParameter(p);
        add(p, Target::Groove,
            [](const GrooveParams& g, const ClipEdit&, double) { return g.complexityTarget; },
            [](GrooveParams& g, ClipEdit&, double&, int v) { g.complexityTarget = juce::jlimit(-1, 100, v); });
    }
    addGrooveInt(owner, "tk_variations", "Variations", 0, kMaxVariations, 0,
                 &GrooveParams::variationIndex, offOr(0, "#"));
    addGrooveInt(owner, "tk_revoice", "Revoice", 0, kMaxRevoice, 0,
                 &GrooveParams::revoice, offOr(0, {}));
    addEditBool(owner, "tk_reverse", "Reverse", false, &ClipEdit::reverse);
    addEditBool(owner, "tk_invert", "Invert", false, &ClipEdit::invert);

    // ── Delay ───────────────────────────────────────────────────────────────
    addGrooveChoice(owner, "tk_delay_time", "Delay Time", delayTimeLabels(),
                    (int) DelayTime::Eighth, &GrooveParams::delayTimeIndex);
    addGrooveInt(owner, "tk_delay_amount", "Delay Amount", 0, 100, 0, &GrooveParams::delayAmount);
    addGrooveInt(owner, "tk_delay_feedback", "Delay Feedback", 0, 100, 40, &GrooveParams::delayFeedback);

    // ── Generators (one part sounds at a time) ──────────────────────────────
    {
        static const char* const kIds[5] = { "gen_arp_on", "gen_melody_on", "gen_chords_on",
                                             "gen_bass_on", "gen_drums_on" };
        static const char* const kNames[5] = { "Arp On", "Melody On", "Chords On", "Bass On", "Drums On" };
        for (int i = 0; i < 5; ++i)
        {
            idxGeneratorOn_[(size_t) i] = (int) bindings_.size();
            addGrooveBool(owner, kIds[i], kNames[i], false, kGeneratorEnabled[i]);
        }
    }

    // Arpeggiator
    addGrooveChoice(owner, "gen_arp_length", "Arp Length", lengthLabels(),
                    (int) GeneratorLength::FourBars, &GrooveParams::arpLengthIndex);
    addGrooveChoice(owner, "gen_arp_form", "Arp Form", formLabels(),
                    (int) GeneratorForm::Auto, &GrooveParams::arpFormIndex);
    addGrooveChoice(owner, "gen_arp_type", "Arp Type", enumLabels<ArpShape>(arpShapeLabel),
                    (int) ArpShape::Walk, &GrooveParams::arpShapeIndex);
    addGrooveChoice(owner, "gen_arp_rate", "Arp Rate", delayTimeLabels(),
                    (int) DelayTime::Sixteenth, &GrooveParams::arpRateIndex);
    addGrooveInt(owner, "gen_arp_density", "Arp Density", 0, 100, 100, &GrooveParams::arpDensity);

    // Melody
    addGrooveBool(owner, "gen_melody_input", "Melody Input", false, &GrooveParams::melodyInput);
    addGrooveChoice(owner, "gen_melody_length", "Melody Length", lengthLabels(),
                    (int) GeneratorLength::FourBars, &GrooveParams::melodyLengthIndex);
    addGrooveChoice(owner, "gen_melody_form", "Melody Form", formLabels(),
                    (int) GeneratorForm::Auto, &GrooveParams::melodyFormIndex);
    addGrooveChoice(owner, "gen_melody_style", "Melody Style", enumLabels<MelodyStyle>(melodyStyleLabel),
                    (int) MelodyStyle::March, &GrooveParams::melodyStyleIndex);
    addGrooveChoice(owner, "gen_melody_contour", "Melody Contour",
                    enumLabels<MelodyContour>(melodyContourLabel),
                    (int) MelodyContour::Even, &GrooveParams::melodyContourIndex);
    addGrooveChoice(owner, "gen_melody_speed", "Melody Speed", enumLabels<MelodySpeed>(melodySpeedLabel),
                    (int) MelodySpeed::Medium, &GrooveParams::melodySpeedIndex);

    // Chords
    addGrooveBool(owner, "gen_chords_input", "Chords Input", false, &GrooveParams::chordsInput);
    addGrooveChoice(owner, "gen_chords_length", "Chords Length", lengthLabels(),
                    (int) GeneratorLength::FourBars, &GrooveParams::chordLengthIndex);
    addGrooveChoice(owner, "gen_chords_form", "Chords Form", formLabels(),
                    (int) GeneratorForm::Auto, &GrooveParams::chordFormIndex);
    addGrooveChoice(owner, "gen_chords_voicing", "Chords Voicing",
                    enumLabels<ChordVoicing>(chordVoicingLabel),
                    (int) ChordVoicing::Close, &GrooveParams::chordVoicingIndex);
    addGrooveChoice(owner, "gen_chords_color", "Chords Color", enumLabels<ChordColor>(chordColorLabel),
                    (int) ChordColor::Triad, &GrooveParams::chordColorIndex);
    addGrooveChoice(owner, "gen_chords_rhythm", "Chords Rhythm", enumLabels<ChordRhythm>(chordRhythmLabel),
                    (int) ChordRhythm::Block, &GrooveParams::chordRhythmIndex);

    // Bass
    addGrooveBool(owner, "gen_bass_input", "Bass Input", false, &GrooveParams::bassInput);
    addGrooveChoice(owner, "gen_bass_length", "Bass Length", lengthLabels(),
                    (int) GeneratorLength::FourBars, &GrooveParams::bassLengthIndex);
    addGrooveChoice(owner, "gen_bass_form", "Bass Form", formLabels(),
                    (int) GeneratorForm::Auto, &GrooveParams::bassFormIndex);
    addGrooveChoice(owner, "gen_bass_motion", "Bass Motion", enumLabels<BassMotion>(bassMotionLabel),
                    (int) BassMotion::Pulse, &GrooveParams::bassMotionIndex);

    // Drums
    addGrooveChoice(owner, "gen_drums_style", "Drums Genre", enumLabels<DrumStyle>(drumStyleLabel),
                    (int) DrumStyle::House, &GrooveParams::drumStyleIndex);
    addGrooveChoice(owner, "gen_drums_length", "Drums Length", lengthLabels(),
                    (int) GeneratorLength::FourBars, &GrooveParams::drumLengthIndex);
    addGrooveChoice(owner, "gen_drums_form", "Drums Form", formLabels(),
                    (int) GeneratorForm::Auto, &GrooveParams::drumFormIndex);
    addGrooveInt(owner, "gen_drums_density", "Drums Density", 0, 100, 50, &GrooveParams::drumDensity);
    addGrooveInt(owner, "gen_drums_fill", "Drums Fill", 0, 100, 35, &GrooveParams::drumFill);
    addGrooveInt(owner, "gen_melody_density", "Melody Density", 0, 100, 100, &GrooveParams::melodyDensity);
    addGrooveInt(owner, "gen_chords_density", "Chords Density", 0, 100, 100, &GrooveParams::chordDensity);
    addGrooveInt(owner, "gen_bass_density", "Bass Density", 0, 100, 100, &GrooveParams::bassDensity);
    addEditBool(owner, "tk_drum_map", "Drums", false, &ClipEdit::drumMap);

    // Listen only after everything exists so construction never self-triggers.
    dirtyCount_ = (int) bindings_.size() + 1;
    dirty_.reset(new std::atomic<bool>[(size_t) dirtyCount_]);
    for (int i = 0; i < dirtyCount_; ++i)
        dirty_[(size_t) i].store(false);
    browserParam_->addListener(this);
    for (auto& b : bindings_)
        b.param->addListener(this);
}

void HostParams::parameterValueChanged(int parameterIndex, float)
{
    // Our own mirror writes land here synchronously on the writing thread — not host moves.
    if (suppress_.load() > 0 && suppressThread_.load() == juce::Thread::getCurrentThreadId())
        return;

    const int rel = parameterIndex - baseIndex_;
    if (rel < 0 || rel >= dirtyCount_ || dirty_ == nullptr)
        return;
    const int slot = rel == 0 ? dirtyCount_ - 1 : rel - 1;
    dirty_[(size_t) slot].store(true);
    if (onHostChange)
        onHostChange();
}

HostParams::Dirty HostParams::takeDirty()
{
    Dirty out;
    if (dirty_ == nullptr)
        return out;
    for (int i = 0; i < dirtyCount_ - 1; ++i)
        if (dirty_[(size_t) i].exchange(false))
            out.bindings.push_back(i);
    out.browserIndex = dirty_[(size_t) (dirtyCount_ - 1)].exchange(false);
    return out;
}

HostParams::Applied HostParams::applyBindings(const std::vector<int>& which, GrooveParams& groove,
                                              ClipEdit& edit, double& bpmMultiplier) const
{
    Applied out;
    bool articulationTouched = false;
    bool knobTouched = false;
    bool rootNeeded = false;
    int generatorWinner = -1;

    for (const int b : which)
    {
        if (!juce::isPositiveAndBelow(b, (int) bindings_.size()))
            continue;
        const auto& binding = bindings_[(size_t) b];
        const int v = rawValue(*binding.param);
        binding.set(groove, edit, bpmMultiplier, v);
        switch (binding.target)
        {
            case Target::Groove: out.groove = true; break;
            case Target::Edit:   out.edit = true; break;
            case Target::Tempo:  out.tempo = true; break;
        }

        if (b == idxArticulation_ || b == idxArticulationStrength_)
            articulationTouched = true;
        if (b == idxLength_ || b == idxIntensity_ || b == idxDynamics_)
            knobTouched = true;
        if ((b == idxMode_ || b == idxFit_ || b == idxMap_) && v != 0)
            rootNeeded = true;
        for (int i = 0; i < 5; ++i)
            if (b == idxGeneratorOn_[(size_t) i] && v != 0 && generatorWinner < 0)
                generatorWinner = i;
    }

    // Articulation derives Length / Intensity / Dynamics (same as the inspector).
    if (articulationTouched)
    {
        if (groove.articulation() == Articulation::Off)
            groove.articulationStrength = 0;
        if (!knobTouched)
            groove.applyArticulationToKnobs();
        out.groove = true;
    }

    // Fit / Map / Mode need a concrete root; the Key chips show C when unset.
    if (rootNeeded && edit.root < 0)
        edit.root = 0;

    // One generator part sounds at a time.
    if (generatorWinner >= 0)
    {
        for (int i = 0; i < 5; ++i)
            groove.*kGeneratorEnabled[i] = i == generatorWinner;
        out.groove = true;
    }
    return out;
}

void HostParams::pushFrom(const GrooveParams& groove, const ClipEdit& edit, double bpmMultiplier,
                          bool userGesture)
{
    if (dirty_ == nullptr)
        return;
    ScopedSuppress guard(suppress_, suppressThread_);
    for (auto& b : bindings_)
        writeRaw(*b.param, b.get(groove, edit, bpmMultiplier), userGesture);
}

int HostParams::browserIndex() const
{
    return browserParam_ != nullptr ? rawValue(*browserParam_) : 0;
}

void HostParams::pushBrowserIndex(int position, bool userGesture)
{
    if (browserParam_ == nullptr)
        return;
    ScopedSuppress guard(suppress_, suppressThread_);
    writeRaw(*browserParam_, juce::jlimit(0, hostparam::kBrowserIndexMax, position), userGesture);
}

juce::String HostParams::idOf(int binding) const
{
    if (!juce::isPositiveAndBelow(binding, (int) bindings_.size()))
        return {};
    return bindings_[(size_t) binding].param->paramID;
}

juce::RangedAudioParameter* HostParams::parameterFor(const juce::String& id) const
{
    if (browserParam_ != nullptr && browserParam_->paramID == id)
        return browserParam_;
    for (const auto& b : bindings_)
        if (b.param->paramID == id)
            return b.param;
    return nullptr;
}

} // namespace pflow
