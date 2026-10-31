# Design Brief: Midi Toolkit — VST3/AU Plugin UI Redesign

## Role

You are redesigning the user interface for **Midi Toolkit**, a compact **DAW plugin** (VST3, AU, Standalone on macOS) that lets music producers **browse local MIDI files**, **preview them in sync with the host transport**, and **drag files into their DAW**. Produce high-fidelity mockups with clear component specs, states, spacing, typography, and interaction notes suitable for handoff to a JUCE/C++ engineer.

---

## Product summary

**Midi Toolkit** is a **MIDI instrument plugin** (not an audio effect). It produces **no audible audio** — only **MIDI note output** routed to the host. Producers use it to audition `.mid` / `.midi` files while the DAW is playing, then drag files into arrangement slots or other tracks.

**Core value proposition:** Fast auditioning of a MIDI library without leaving the DAW, with preview locked to session tempo and playhead.

**Primary users:** Electronic music producers, beatmakers, and composers who keep large folders of MIDI loops, chords, and patterns.

**Usage context:** Lives inside a narrow plugin window on a DAW track, often alongside synths and mixers. Must feel native, readable at small sizes, and usable during playback.

---

## What the app DOES (must preserve in redesign)

### 1. Folder-based MIDI library browser

- User opens a **folder** on disk (not individual files at root).
- Supported extensions: `.mid`, `.midi` (case-insensitive).
- Files are shown in a **hierarchical tree** (folders + MIDI files).
- Tree supports **vertical and horizontal scroll** when paths or names are long.
- **Empty state** before a folder is chosen: centered hint text — *"Open a folder to browse MIDI files / Drag files into your DAW"*.
- **Last opened folder** is restored when the plugin reopens (persisted in DAW project state).

### 2. File selection & navigation

- **Single-click** a MIDI file → loads it into the preview panel.
- **Up / Down arrow keys** → select previous/next MIDI file in tree order (skips folders).
- **Previous / Next toolbar buttons** → same as arrow keys.
- Selected row uses a **highlight tint** (system accent blue at ~28% opacity).
- Tree row height ~22px; filename at 13pt body size.

### 3. Drag and drop to DAW

- User **drags a selected MIDI file** out of the tree into the DAW (external file drag).
- Drag starts after ~4px movement threshold.
- This is the **only** way to "export" into the DAW — the plugin cannot create clips directly.

### 4. Saved folders (bookmarks)

- **Star / bookmarks button** opens a popup menu:
  - **Save current folder** (disabled if no folder open)
  - List of saved folders (shows folder name; full path on selection)
  - Submenu: **Remove saved folder** (per entry)
- Up to **24** saved folders; most recent first; deduplicated.
- Selecting a saved folder switches the browser root immediately.

### 5. Live session-synced MIDI preview

When the **DAW transport is playing** and preview is **unmuted** and a file is selected:

- Plugin outputs **note-on/note-off MIDI** for the selected clip in real time.
- Preview **loops** to match host behavior:
  - If host loop is active → preview phase follows host loop points.
  - Otherwise → preview wraps over a **session length** (default **4 bars** / 16 beats, configurable in state but **no UI control yet**).
- Clip itself also loops if shorter than the session phase window.
- When transport **stops** → all notes off on all 16 channels.
- On transport **jumps/discontinuities** → all-notes-off to avoid stuck notes.

**Preview is silent in the plugin** — MIDI is routed to the host/instrument track.

### 6. Piano roll preview panel (read-only)

Bottom panel (~168px tall) shows a **mini piano roll** of the selected file:

- **Left strip:** piano keys (white/black) auto-scaled to note range (minimum 2 octaves visible).
- **Grid:** horizontal pitch rows + vertical time.
- **Notes:** rounded rectangles; opacity/brightness reflects velocity.
- **Playhead:** red vertical line (2px), only when host is playing + unmuted + clip loaded; position reflects session-synced phase within clip length.
- **Empty state:** *"Select a MIDI file to preview"* centered in roll area.
- Panel uses **grouped inset** surface (rounded rectangle, ~10pt radius).

### 7. Preview controls (bottom of preview panel)

Two **capsule toggle chips** (right-aligned):

| Control | Label / icon | Behavior |
|---------|----------------|----------|
| **Trim** | "Trim" + scissors icon | Toggle. When ON, preview **visually and audibly** trims empty measures (4/4 bars with no notes) from the clip. **Does not modify the file on disk.** Keyboard shortcut: **T**. State persisted in plugin save. |
| **Mute** | Speaker-slash icon (no text) | Toggle. When ON, **no MIDI output** to host; playhead hidden. Default: unmuted. |

Active chips: filled **system blue** with white label/icon.  
Inactive: neutral gray fill (~36% opacity).

### 8. Toolbar (top of browser panel)

Single row, 32px height, left-to-right:

| Control | Type | Behavior |
|---------|------|----------|
| **Open…** | Primary filled button (system blue, white text, HIG ellipsis) | Opens native OS folder picker. Sets browser root. |
| **Bookmarks** | Icon button (star) | Opens saved-folders menu. |
| **Previous file** | Icon button (chevron up) | Previous MIDI in tree. |
| **Next file** | Icon button (chevron down) | Next MIDI in tree. |

Icon buttons: 28×28px, subtle hover/press fill.

### 9. Plugin header (above browser)

- **Title:** "Midi Toolkit" — Large Title scale (~28pt semibold).
- **Hairline separator** below header (~52px header zone).
- **Ableton Live only:** optional footnote banner (40px) with routing instructions:  
  *"Ableton: use on its own MIDI track. Set your instrument track's MIDI From to this plugin and Monitor to In."*  
  Shown only when host is detected as Ableton. Secondary label color.

### 10. Persistence (invisible but affects reopen behavior)

Saved in DAW project/plugin state:

- Last browser folder path
- Trim preview toggle on/off
- Saved folder bookmarks list
- Session sync bars (default 4 — no UI yet)
- Theme ID (single dark theme today)

---

## What the app does NOT do (do not design these as active features)

- No audio synthesis or waveform display
- No arrangement timeline, lanes, regions, or comping UI (future product scope)
- No in-plugin MIDI editing (no note move, resize, draw, delete)
- No tempo/BPM control (follows host)
- No direct "insert clip into DAW slot" button
- No search/filter bar (yet)
- No waveform or audio file support
- No cloud/sync library
- No multi-file selection
- No solo button (mute only)
- No transport controls (play/stop) — host owns transport
- No settings/preferences panel
- No light mode UI today (dark only, but light mode is acceptable in redesign)

---

## Layout architecture (current)

```
┌─────────────────────────────────────────┐
│  Midi Toolkit                    (52px) │  ← Large title + separator
├─────────────────────────────────────────┤
│  [Ableton hint banner]           (40px) │  ← conditional
├─────────────────────────────────────────┤
│  [Open…] [★] [▲] [▼]             (32px) │  ← toolbar
│  ┌─────────────────────────────────┐    │
│  │  File tree (grouped inset)      │    │  ← flex: fills remaining
│  │  folders + .mid files           │    │
│  └─────────────────────────────────┘    │
│  ┌─────────────────────────────────┐    │
│  │  Piano roll preview      (168px)│    │  ← grouped inset
│  │  [Trim chip] [Mute chip]  (32px) │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

**Window defaults:** 300 × 520 px  
**Resize limits:** min 280 × 480, max 1200 × 2000  
**Padding:** 16px horizontal margins (8pt grid)  
**Gap between tree and preview:** 8px

---

## Interaction flows (design all states)

### Flow A — First launch

1. Empty tree hidden; empty-state hint visible.
2. User taps **Open…** → OS folder picker.
3. Tree populates; hint hidden.
4. User selects first file → preview roll + notes appear.

### Flow B — Audition while producing

1. User selects file in tree.
2. DAW playing → red playhead moves; MIDI flows to instrument (if unmuted).
3. User toggles **Mute** → playhead disappears; MIDI stops.
4. User toggles **Trim** → roll and output reflect trimmed clip instantly.

### Flow C — Drag into arrangement

1. User selects file.
2. Drags from tree into DAW arrangement → `.mid` file dropped.

### Flow D — Bookmark workflow

1. User opens deep library folder.
2. Taps star → Save current folder.
3. Later: star → picks saved folder from list → instant navigation.

### Flow E — Keyboard power use

- ↑/↓ navigate files
- **T** toggles trim (works when plugin has keyboard focus)

---

## Visual system (current reference — redesign freely)

Current implementation follows **Apple HIG dark mode**. You may evolve this or propose a new system, but keep **high contrast** and **compact density**.

| Token | Value | Usage |
|-------|-------|-------|
| Window background | `#1C1C1E` | Root surface |
| Grouped panels | `#2C2C2E` | Tree + preview insets |
| System accent | `#0A84FF` | Primary button, selection, active chips, notes |
| Primary text | `#FFFFFF` | Labels, filenames |
| Secondary text | `#EBEBF5` @ 60% | Hints, footnotes |
| Separator | `#545458` @ 65% | Hairlines |
| Playhead | `#FF453A` | Red line |
| Control fill | `#787880` @ 36% | Inactive chips, icon hover |

**Typography:** SF Pro / system UI font

- Large Title 28pt semibold (app title)
- Headline 13pt semibold (section labels if used)
- Body 13pt regular (tree, buttons)
- Callout 12pt (hints)
- Footnote 10pt (Ableton banner)

**Radii:** grouped panels 10pt, buttons/chips 6–8pt  
**Icons (SF Symbol–style strokes):** star, chevron up/down, scissors, speaker.slash

---

## Technical constraints for Figma → code handoff

- Target framework: **JUCE (C++)** native desktop UI — not React/web.
- Avoid designs requiring: CSS grid with complex nesting, blur-heavy glassmorphism, video, web fonts that aren't system-available on macOS.
- Prefer: flat fills, 1px hairlines, rounded rects, simple vector icons, standard buttons/labels/lists.
- UI repaints at **30 Hz** during playback for playhead animation — keep preview graphics lightweight.
- Plugin window must work at **280px width** minimum.
- All controls must have clear **normal / hover / pressed / disabled / active (toggle on)** states.
- Design for **macOS first**; Windows/Linux are secondary (VST3/Standalone).

---

## Host & routing context (inform copy and onboarding)

| Host | Plugin type | Routing notes |
|------|-------------|---------------|
| **Ableton Live** | Instrument on dedicated MIDI track | Instrument track must set MIDI From → this plugin; Monitor → In |
| **Logic Pro** | AU instrument | Standard MIDI instrument routing |
| **Other DAWs** | VST3 instrument | MIDI out to same or downstream instrument |

Plugin accepts MIDI input and produces MIDI output; outputs silent stereo audio for host compatibility.

---

## Deliverables requested

### For Figma

1. **Main frame** at 300×520 (default) + **wide** variant at 400×600.
2. **Component library:** primary button, icon button, capsule toggle, tree row (default/selected/hover), empty states, popup menu (bookmarks), piano roll cell, playhead, note bar.
3. **All states:** empty (no folder), empty (no selection), file selected (stopped), file selected (playing + playhead), trim on, mute on, Ableton banner visible.
4. **Spacing annotations** on 8pt grid.
5. **Color + type styles** as Figma variables.
6. Optional: **dark + light** themes.

### For Google Stitch

Generate a polished UI from this brief. Prioritize:

- Clarity at small width
- Obvious primary action (Open folder)
- Readable file tree
- Preview panel that reads as a piano roll, not a generic chart
- Distinct mute vs trim controls
- Professional music-production aesthetic (not consumer/media-player)

---

## Design goals / creative direction

- Feel like a **native macOS utility** or **pro audio tool**, not a generic file manager.
- Optimize for **one-handed auditioning**: select → hear → drag, with minimal clicks.
- Tree and preview should feel like **one cohesive panel**, not two unrelated widgets.
- Consider improvements (optional, not required): search field, current folder breadcrumb, filename in preview header, transport status label (Playing/Stopped), side-docked preview layout toggle, subtle note velocity legend.
- **Do not** add arrangement/timeline/comping unless explicitly marked "future."

---

## Acceptance criteria

A successful redesign:

1. Includes every control and state listed above.
2. Works at 280px minimum width without truncation of primary actions.
3. Makes preview/playhead/mute relationship visually obvious.
4. Documents all interactive states for engineering handoff.
5. Respects DAW plugin constraints (no web-only patterns).

---

**Product name:** Midi Toolkit  
**Version:** 1.0  
**Platform:** macOS VST3 / AU / Standalone  
**Repo:** github.com/mganucheau/MidiToolkit
