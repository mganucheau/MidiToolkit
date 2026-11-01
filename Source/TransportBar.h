#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UiAtoms.h"
#include "NativeWindowChrome.h"

namespace pflow {

/** Groove Panel 40px header: wordmark · BPM · play/stop · record · sync · through · panel toggles. */
class TransportBar : public juce::Component,
                     private juce::Timer
{
public:
    TransportBar();

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void setPlaying(bool);
    void setRecording(bool);
    void setSynced(bool, juce::NotificationType notify = juce::dontSendNotification);
    void setHostBpm(double);
    void setBpmMultiplier(double);
    void setFreeBpm(double);
    void setClipBpm(double);
    void setEditorOpen(bool);
    void setEffectsOpen(bool);
    void setPassthrough(bool);
    void setHasClip(bool has);
    void setClipName(const juce::String& name);
    /** Standalone macOS reserves a traffic-light gutter; plugins must not. */
    void setReserveTrafficLights(bool reserve);
    /** Measured right edge of the zoom button, including the gap before the logo.
        0 collapses the zone to 16 px (fullscreen or no lights). */
    void setLightsZoneWidth(int px);
    juce::Rectangle<int> getTitleBounds() const { return titleBounds; }
    /** Glyph + gap + “MIDI Toolkit”, independent of the current squeeze. */
    int logoIdealWidth() const;
    /** 2 = MIDI Toolkit, 1 = MIDI, 0 = hidden. */
    int logoWordmarkLevel() const { return logoLevel; }

    std::function<void()> onPlayPause;
    std::function<void()> onStop;
    std::function<void()> onToggleRecord;
    std::function<void()> onToggleEditor;
    std::function<void()> onToggleEffects;
    std::function<void()> onTogglePassthrough;
    std::function<void(bool)> onSyncChanged;
    std::function<void(double)> onFreeBpmChanged;
    std::function<void()> onDragToDaw;
    std::function<void()> onCopyToFolder;
    std::function<void()> onOpenSettings;

private:
    void applyHeaderCaptions(bool withText);
    void refreshBpm();
    void timerCallback() override;
    bool hitInteractive(juce::Point<int> p) const;
    void paintLogo(juce::Graphics&, juce::Rectangle<int> area, float alpha) const;
    void paintLogoGlyph(juce::Graphics&, juce::Point<float> origin, float alpha) const;
    int logoMidiWidth() const;

    class StatusPill : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
    {
    public:
        std::function<void()> onToggleSync;
        std::function<void(double)> onCommitBpm;
        bool synced = true;
        bool editable = false;
        float foregroundAlpha = 1.0f;

        void setText(const juce::String& t);
        void paint(juce::Graphics&) override;
        int idealWidth() const;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseEnter(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;
        void beginEditing();
        bool keyPressed(const juce::KeyPress&) override;
        void focusLost(FocusChangeType) override;
        void timerCallback() override;

        juce::String text;
        bool editing = false;
        juce::String editBuffer;
        bool pendingSyncToggle = false;
        double value = 124.0;
        double dragStartBpm = 124.0;
    };

    class DragChip : public ChipBtn
    {
    public:
        using ChipBtn::ChipBtn;
        std::function<void()> onDragStart;
        std::function<void()> onCopyToFolder;
        juce::String clipName;
        float foregroundAlpha = 1.0f;

        void paintButton(juce::Graphics& g, bool over, bool down) override;
        int idealWidth() const;

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem(1, "Copy to Folder...", onCopyToFolder != nullptr);
                m.showMenuAsync(juce::PopupMenu::Options()
                                    .withTargetComponent(this)
                                    .withDeletionCheck(*this),
                    [this](int result)
                    {
                        if (result == 1 && onCopyToFolder)
                            onCopyToFolder();
                    });
                return;
            }
            ChipBtn::mouseDown(e);
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!dragging && e.getDistanceFromDragStart() > 5 && onDragStart)
            {
                dragging = true;
                onDragStart();
            }
        }
        void mouseUp(const juce::MouseEvent& e) override
        {
            dragging = false;
            ChipBtn::mouseUp(e);
        }
    private:
        bool dragging = false;
    };

    IconBtn btnPlay { icons::play, "Play preview" };
    IconBtn btnStop { icons::stop, "Stop preview" };
    IconBtn btnRecord { icons::record, "Record" };
    StatusPill statusPill;
    IconBtn btnSync { icons::sync, "Sync to host tempo" };
    DragChip btnDragToDaw { "Drag to DAW" };
    IconBtn btnPassthrough { icons::midi, "Through" };
    IconBtn btnEditor { icons::noteKeys, "Piano Roll" };
    IconBtn btnEffects { icons::sliders, "Effects" };
    IconBtn btnSettings { icons::gear, "Settings" };

    juce::Rectangle<int> titleBounds, transportGroupBounds, dividerBounds;

    bool playing = false;
    bool synced = true;
    bool editorOpen = false;
    bool effectsOpen = false;
    bool passthrough = false;
    bool hasClip = false;
    bool windowActive = true;
    bool reserveTrafficLights = false;
    double hostBpm = 124.0;
    double freeBpm = 124.0;
    double multiplier = 1.0;
    int lightsZoneW = 16;
    int measuredLightsZone = 0;
    int logoLevel = 2;
    juce::ComponentDragger windowDragger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace pflow
