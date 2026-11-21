#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "EditModel.h"
#include "GrooveEngine.h"
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace pflow {

/** Host-visible (automatable) parameters.
    Manual AudioProcessorParameters — not an APVTS and not a second state system:
    per-clip XML (clipEdits / clipGrooves) stays the persisted source, and these
    parameters mirror the active clip's Toolkit + Generator state so a host can
    automate it. IDs are fixed (version hint 1) so projects survive; never rename
    or reorder an ID, only append.

    Threading: the host may change a parameter from the audio or message thread.
    The listener only flags it dirty and pings `onHostChange`; the processor then
    applies the change on the message thread (takeDirty / applyBindings), the same
    path the inspector uses to publish LiveFxState. */
namespace hostparam
{
    /** Browser selection: 0-based position among the files in the current list. */
    inline constexpr const char* kBrowserIndexId = "browser_index";
    /** Highest file position the host can address (the list clamps below this). */
    constexpr int kBrowserIndexMax = 1023;
}

class HostParams : private juce::AudioProcessorParameter::Listener
{
public:
    /** Which inspector field set a binding writes. */
    enum class Target { Groove, Edit, Tempo };

    /** What changed after applyBindings(); lets callers fire the matching inspector hooks. */
    struct Applied
    {
        bool groove = false;
        bool edit = false;
        bool tempo = false;
        bool any() const { return groove || edit || tempo; }
    };

    /** Dirty parameters consumed from the host since the last call. */
    struct Dirty
    {
        std::vector<int> bindings;
        bool browserIndex = false;
    };

    HostParams();
    ~HostParams() override;

    /** Create every parameter and hand ownership to `owner` (call once, in its constructor). */
    void create(juce::AudioProcessor& owner);

    /** Called (possibly from the audio thread) after the host moves a parameter. */
    std::function<void()> onHostChange;

    /** Message thread. Returns the host-changed bindings (and browser flag) and clears them. */
    Dirty takeDirty();

    /** Apply only `bindings` onto the structs; fields not listed keep their value. */
    Applied applyBindings(const std::vector<int>& bindings, GrooveParams& groove, ClipEdit& edit,
                          double& bpmMultiplier) const;

    /** Mirror state into parameters. Only values that differ are written; the listener
        ignores these writes. `userGesture` brackets each write in a change gesture so a
        host can record the tweak; selection mirrors pass false. */
    void pushFrom(const GrooveParams& groove, const ClipEdit& edit, double bpmMultiplier,
                  bool userGesture);

    /** Current host browser position (unclamped). */
    int browserIndex() const;
    void pushBrowserIndex(int position, bool userGesture);

    int numBindings() const { return (int) bindings_.size(); }
    /** Parameter ID of a binding (tests / diagnostics). */
    juce::String idOf(int binding) const;
    juce::RangedAudioParameter* parameterFor(const juce::String& id) const;
    juce::RangedAudioParameter* browserIndexParameter() const { return browserParam_; }

private:
    struct Binding
    {
        juce::RangedAudioParameter* param = nullptr;
        Target target = Target::Groove;
        std::function<int(const GrooveParams&, const ClipEdit&, double)> get;
        std::function<void(GrooveParams&, ClipEdit&, double&, int)> set;
    };

    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int, bool) override {}

    void add(juce::RangedAudioParameter* param, Target target,
             std::function<int(const GrooveParams&, const ClipEdit&, double)> get,
             std::function<void(GrooveParams&, ClipEdit&, double&, int)> set);
    void addGrooveInt(juce::AudioProcessor& owner, const char* id, const char* name, int lo, int hi,
                      int def, int GrooveParams::* field,
                      std::function<juce::String(int)> text = nullptr);
    void addGrooveChoice(juce::AudioProcessor& owner, const char* id, const char* name,
                         const juce::StringArray& labels, int def, int GrooveParams::* field);
    void addGrooveBool(juce::AudioProcessor& owner, const char* id, const char* name, bool def,
                       bool GrooveParams::* field);
    void addEditBool(juce::AudioProcessor& owner, const char* id, const char* name, bool def,
                     bool ClipEdit::* field);

    static int rawValue(const juce::RangedAudioParameter& p);
    void writeRaw(juce::RangedAudioParameter& p, int value, bool userGesture);

    std::vector<Binding> bindings_;
    juce::RangedAudioParameter* browserParam_ = nullptr;
    int baseIndex_ = 0;
    /** Binding slots that need special handling in applyBindings (-1 = unset). */
    int idxArticulation_ = -1;
    int idxArticulationStrength_ = -1;
    int idxLength_ = -1;
    int idxIntensity_ = -1;
    int idxDynamics_ = -1;
    int idxMode_ = -1;
    int idxFit_ = -1;
    int idxMap_ = -1;
    std::array<int, 5> idxGeneratorOn_ { -1, -1, -1, -1, -1 };
    /** One flag per binding, then the browser index last. */
    std::unique_ptr<std::atomic<bool>[]> dirty_;
    int dirtyCount_ = 0;
    std::atomic<int> suppress_ { 0 };
    /** Thread that is mirroring state into parameters (its listener callbacks are ignored). */
    std::atomic<juce::Thread::ThreadID> suppressThread_ {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HostParams)
};

} // namespace pflow
