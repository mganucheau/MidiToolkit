#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <functional>

namespace pflow {
namespace nativeChrome {

#if JUCE_MAC

/** FullSizeContentView + transparent titlebar so native traffic lights
    float over the app header. */
/** Folds the system titlebar into the app header. headerHeight is the bar the
    traffic lights should sit in (the transport bar). */
void applyCupertinoTitlebar(juce::Component& anyComponentInWindow, int headerHeight = 52);

/** [NSWindow performWindowDragWithEvent:] — header drag + double-click zoom. */
bool performWindowDrag(juce::Component& anyComponentInWindow);

void zoomWindow(juce::Component& anyComponentInWindow);
bool isWindowKey(juce::Component& anyComponentInWindow);
void preferNativeTitleBar(juce::Component& anyComponentInWindow);

/** Right edge (component px, from the window left) of the rightmost visible
    traffic light plus trailingGap. Returns 0 when every light is hidden
    (fullscreen) or there is no window. */
int trafficLightsTrailingEdge(juce::Component& anyComponentInWindow, int trailingGap = 14);

/** Intentionally a no-op. Hiding or moving the host window's traffic lights
    crashes Ableton Live. */
void layoutTrafficLights(juce::Component& anyComponentInWindow, int headerHeight);

/** True when there is nothing left to do: no window yet is false; a view that
    does not fill its window, fullscreen, or an already-folded titlebar is true. */
bool titlebarFolded(juce::Component& anyComponentInWindow);

/** Re-runs onChange on the message thread when fullscreen, resize, or backing scale changes.
    Replaces any previous observer on this component. Removed when the component is destroyed. */
void observeWindowChrome(juce::Component& anyComponentInWindow, std::function<void()> onChange);

/** While sync is true, space in this plugin window toggles the host transport.
    Returns an opaque token; pass it to removeSpaceToHostMonitor. */
void* installSpaceToHostMonitor(juce::Component& editor, std::atomic<bool>& syncToHost);
void removeSpaceToHostMonitor(void* token);

#else

inline void applyCupertinoTitlebar(juce::Component&, int = 52) {}
inline bool performWindowDrag(juce::Component&) { return false; }
inline void zoomWindow(juce::Component&) {}
inline bool isWindowKey(juce::Component&) { return true; }
inline void preferNativeTitleBar(juce::Component&) {}
inline int trafficLightsTrailingEdge(juce::Component&, int = 14) { return 0; }
inline void layoutTrafficLights(juce::Component&, int) {}
inline bool titlebarFolded(juce::Component&) { return true; }
inline void observeWindowChrome(juce::Component&, std::function<void()>) {}
inline void* installSpaceToHostMonitor(juce::Component&, std::atomic<bool>&) { return nullptr; }
inline void removeSpaceToHostMonitor(void*) {}

#endif

} // namespace nativeChrome
} // namespace pflow
