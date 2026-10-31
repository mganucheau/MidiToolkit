#include "NativeWindowChrome.h"

#if JUCE_MAC
 #import <AppKit/AppKit.h>
 #import <objc/runtime.h>
 #include <cmath>

/** Invisible toolbar item whose height makes the unified titlebar match the app header,
    so AppKit centres the real traffic lights on that bar. A visible item would draw a pill. */
@interface MidiHeaderToolbarSpacer : NSObject <NSToolbarDelegate>
@property (nonatomic, assign) CGFloat headerHeight;
@end

@implementation MidiHeaderToolbarSpacer
- (NSArray<NSToolbarItemIdentifier>*) toolbarAllowedItemIdentifiers: (NSToolbar*) toolbar
{
    juce::ignoreUnused (toolbar);
    return @[ @"midi.header.pad" ];
}

- (NSArray<NSToolbarItemIdentifier>*) toolbarDefaultItemIdentifiers: (NSToolbar*) toolbar
{
    juce::ignoreUnused (toolbar);
    return @[ @"midi.header.pad" ];
}

- (NSToolbarItem*) toolbar: (NSToolbar*) toolbar
     itemForItemIdentifier: (NSToolbarItemIdentifier) itemIdentifier
 willBeInsertedIntoToolbar: (BOOL) flag
{
    juce::ignoreUnused (toolbar, flag);
    auto* item = [[NSToolbarItem alloc] initWithItemIdentifier: itemIdentifier];
    auto* view = [[NSView alloc] initWithFrame: NSMakeRect (0, 0, 0, self.headerHeight)];
    item.view = view;
    item.minSize = NSMakeSize (0, self.headerHeight);
    item.maxSize = NSMakeSize (0, self.headerHeight);
    return item;
}
@end
#endif

namespace pflow {
namespace nativeChrome {

#if JUCE_MAC

namespace {

NSWindow* windowForComponent(juce::Component& c)
{
    if (auto* peer = c.getPeer())
        if (auto* view = (NSView*) peer->getNativeHandle())
            return [view window];
    return nil;
}

NSView* editorViewFor(juce::Component& c)
{
    if (auto* peer = c.getPeer())
        return (NSView*) peer->getNativeHandle();
    return nil;
}

/** Floating plugin/standalone windows are filled by the editor. A device view
    inside the host's main window is a small slice of that content view. */
bool editorFillsWindow(NSView* editor, NSWindow* window)
{
    if (editor == nil || window == nil || window.contentView == nil)
        return false;
    NSView* content = window.contentView;
    if (editor == content)
        return true;
    const NSRect editorInContent = [editor convertRect:editor.bounds toView:content];
    const CGFloat contentArea = std::abs(NSWidth(content.bounds) * NSHeight(content.bounds));
    const CGFloat editorArea = std::abs(NSWidth(editorInContent) * NSHeight(editorInContent));
    return contentArea > 1.0 && editorArea >= contentArea * 0.85;
}

void stretchEditorToContent(NSView* editor, NSView* content)
{
    if (editor == nil || content == nil || editor == content)
        return;
    const NSRect editorInContent = [editor convertRect:editor.bounds toView:content];
    const CGFloat topGap = NSHeight(content.bounds) - NSMaxY(editorInContent);
    if (topGap < 2.0 && std::abs(NSMinY(editorInContent)) < 2.0)
        return;

    NSMutableArray* chain = [NSMutableArray array];
    for (NSView* v = editor; v != nil && v != content; v = v.superview)
        [chain addObject:v];
    for (NSInteger i = (NSInteger) [chain count] - 1; i >= 0; --i)
    {
        NSView* v = [chain objectAtIndex:(NSUInteger) i];
        if (v.superview == nil)
            continue;
        v.frame = v.superview.bounds;
        v.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    }
}

// Hide yellow/green and enlarge the red close button so it sits on the header midline.
// Skips the call when the buttons are already in that state.
void styleCloseButtonOnly(NSWindow* window)
{
    if (window == nil)
        return;

    NSButton* mini = [window standardWindowButton:NSWindowMiniaturizeButton];
    NSButton* zoom = [window standardWindowButton:NSWindowZoomButton];
    if (mini != nil && !mini.hidden)
        mini.hidden = YES;
    if (zoom != nil && !zoom.hidden)
        zoom.hidden = YES;

    NSButton* close = [window standardWindowButton:NSWindowCloseButton];
    if (close == nil || close.superview == nil)
        return;

    NSView* parent = close.superview;
    const CGFloat parentH = NSHeight(parent.bounds);
    if (parentH < 20.0)
        return;

    const CGFloat d = 16.0;
    const CGFloat y = std::round((parentH - d) * 0.5);
    NSRect f = close.frame;
    if (std::abs(f.size.width - d) < 0.5 && std::abs(f.size.height - d) < 0.5
        && std::abs(f.origin.y - y) < 0.5)
        return;

    f.size = NSMakeSize(d, d);
    f.origin.y = y;
    [close setFrame:f];
}

} // namespace

void preferNativeTitleBar(juce::Component& anyComponentInWindow)
{
    for (auto* c = &anyComponentInWindow; c != nullptr; c = c->getParentComponent())
    {
        if (auto* tl = dynamic_cast<juce::TopLevelWindow*>(c))
        {
            tl->setUsingNativeTitleBar(true);
            if (auto* dw = dynamic_cast<juce::DocumentWindow*>(tl))
            {
                // Keep real traffic lights (close / miniaturize / zoom) on the left.
                dw->setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, true);
            }
            break;
        }
    }
}

void applyCupertinoTitlebar(juce::Component& anyComponentInWindow, int headerHeight)
{
    preferNativeTitleBar(anyComponentInWindow);

    NSWindow* window = windowForComponent(anyComponentInWindow);
    NSView* editorView = editorViewFor(anyComponentInWindow);
    if (window == nil || editorView == nil)
        return;
    if ((window.styleMask & NSWindowStyleMaskFullScreen) != 0)
        return;
    // Never restyle a host document window that merely contains this view.
    if (! editorFillsWindow(editorView, window))
        return;

    static bool folding = false;
    if (folding)
        return;
    folding = true;

    // Editor area on screen, before the titlebar is folded into the content.
    const NSRect editorOnScreen = [window convertRectToScreen:[editorView convertRect:editorView.bounds toView:nil]];

    NSWindowStyleMask mask = [window styleMask];
    mask |= NSWindowStyleMaskTitled
          | NSWindowStyleMaskClosable
          | NSWindowStyleMaskMiniaturizable
          | NSWindowStyleMaskResizable
          | NSWindowStyleMaskFullSizeContentView;
    if (mask != [window styleMask])
        [window setStyleMask:mask];
    window.titlebarAppearsTransparent = YES;
    window.titleVisibility = NSWindowTitleHidden;
    window.movableByWindowBackground = NO;

    // A unified toolbar whose only item is a zero-width view of headerHeight makes the
    // titlebar that tall and centres the real traffic lights in it. FullSizeContentView
    // draws the app header underneath, so this does not add a second strip.
    static const void* spacerKey = &spacerKey;
    auto* spacer = (MidiHeaderToolbarSpacer*) objc_getAssociatedObject (window, spacerKey);
    if (spacer == nil)
    {
        spacer = [MidiHeaderToolbarSpacer new];
        objc_setAssociatedObject (window, spacerKey, spacer, OBJC_ASSOCIATION_RETAIN);
    }
    const CGFloat barH = (CGFloat) juce::jmax (28, headerHeight);
    const bool heightChanged = std::abs (spacer.headerHeight - barH) > 0.5;
    spacer.headerHeight = barH;
    if (window.toolbar == nil || heightChanged)
    {
        auto* toolbar = [[NSToolbar alloc] initWithIdentifier: @"midi.toolkit.header"];
        toolbar.delegate = spacer;
        toolbar.allowsUserCustomization = NO;
        toolbar.autosavesConfiguration = NO;
        if (@available (macOS 11.0, *))
            window.toolbarStyle = NSWindowToolbarStyleUnified;
        window.toolbar = toolbar;
    }

    if (@available(macOS 11.0, *))
        window.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;

    // Fold the titlebar into the content so the editor's header is the only bar.
    const NSRect fitted = [window frameRectForContentRect:editorOnScreen];
    if (! NSEqualRects(fitted, window.frame))
        [window setFrame:fitted display:YES animate:NO];

    if (NSView* content = window.contentView)
        stretchEditorToContent(editorView, content);

    // One shot, and only while this window is being folded — never from a timer
    // or an NSWindow observer. Repeating setHidden/setFrame crashed Live's Touch Bar.
    styleCloseButtonOnly(window);

    folding = false;
}

bool titlebarFolded(juce::Component& anyComponentInWindow)
{
    NSWindow* window = windowForComponent(anyComponentInWindow);
    if (window == nil)
        return false;
    if ((window.styleMask & NSWindowStyleMaskFullScreen) != 0)
        return true;
    NSView* editorView = editorViewFor(anyComponentInWindow);
    if (! editorFillsWindow(editorView, window))
        return true;
    const bool full = (window.styleMask & NSWindowStyleMaskFullSizeContentView) != 0;
    return full
        && window.titleVisibility == NSWindowTitleHidden
        && window.titlebarAppearsTransparent;
}

void layoutTrafficLights(juce::Component&, int)
{
    // Do not hide or move NSWindow's standard buttons. Live walks that responder
    // chain for the Touch Bar and crashes (objc_retain on a freed nextResponder)
    // when a plugin calls setHidden or setFrame on them.
}

int trafficLightsTrailingEdge(juce::Component& anyComponentInWindow, int trailingGap)
{
    NSWindow* window = windowForComponent(anyComponentInWindow);
    if (window == nil)
        return 0;
    if ((window.styleMask & NSWindowStyleMaskFullScreen) != 0)
        return 0;
    if (! editorFillsWindow(editorViewFor(anyComponentInWindow), window))
        return 0;

    // Right edge of the rightmost visible light, so the logo follows the red
    // button while yellow and green are hidden.
    CGFloat maxX = -1.0;
    const NSWindowButton kinds[] = { NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton };
    for (NSWindowButton kind : kinds)
    {
        NSButton* button = [window standardWindowButton:kind];
        if (button == nil || button.isHidden || button.superview == nil)
            continue;
        const NSRect r = [button convertRect:button.bounds toView:nil];
        maxX = std::max(maxX, (CGFloat) NSMaxX(r));
    }
    if (maxX < 0.0)
        return 0;
    return (int) std::ceil(maxX) + trailingGap;
}

void observeWindowChrome(juce::Component&, std::function<void()>)
{
    // Do not observe the host NSWindow. NSWindowDidBecomeKeyNotification
    // delivered the block synchronously inside becomeKeyWindow, and CFRetain
    // on the captured life token crashed Live (SIGTRAP, CFRetain.cold.2).
}

bool performWindowDrag(juce::Component& anyComponentInWindow)
{
    NSWindow* window = windowForComponent(anyComponentInWindow);
    if (window == nil)
        return false;
    NSEvent* event = [NSApp currentEvent];
    if (event == nil)
        return false;
    [window performWindowDragWithEvent:event];
    return true;
}

void zoomWindow(juce::Component& anyComponentInWindow)
{
    NSWindow* window = windowForComponent(anyComponentInWindow);
    if (window != nil)
        [window zoom:nil];
}

bool isWindowKey(juce::Component& anyComponentInWindow)
{
    NSWindow* window = windowForComponent(anyComponentInWindow);
    if (window != nil)
        return [window isKeyWindow];
    return true;
}

#endif // JUCE_MAC

} // namespace nativeChrome
} // namespace pflow
