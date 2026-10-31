# Try Midi Toolkit

Midi Toolkit is a plugin (AU, VST3, and Standalone) for browsing a folder of MIDI files. Click a file, hear it, and shape it in the Toolkit. When it sounds right, drag it onto a track in your DAW.

## Getting around

The window is a folder sidebar, a file list, a **Preview** strip, and (when you open them) the **Piano Roll** and the **Toolkit**.

- Click a file to select it. **Space** starts and pauses preview. The stop button ends it. Up and down arrows move to the next file; if preview is already running, the new file keeps playing.
- **Preview** sits under the file list and shows a small roll of the selected file. Click the header to fold it.
- **Piano Roll** (or E) opens the full editor. **Effects** (or F) opens the Toolkit column.
- The tempo pill shows BPM. **Sync** follows the host; click it for a free tempo you can type.
- **Drag to DAW** drags the clip with your Toolkit settings. Dragging a row out of the file list sends the original file.
- **Passthrough** (or P) sends live MIDI through the Toolkit.

## Library

- **Open Folder...** picks a folder of MIDI files.
- The refresh icon on the folder card reloads that folder (hover tip: Refresh this folder).
- **Scan Subdirectories** includes MIDI in nested folders.
- The star on the folder card saves that folder plus the filters you have set. It shows up under **LIBRARY**. Click the star again to remove it (hover tip: Remove from Library). A star on a file row adds that file to **Favorites**.
- The number beside the folder name is how many files match.
- **Filters** opens:
  - **KEY**, **KIND** (Bass, Piano, Lead, Drums, Single), and **TIME** (4/4, 3/4, 2/4, 6/8, 5/4, 7/8, 12/8, 9/8). Tap any you want; you can pick more than one.
  - **TEMPO**, **BARS**, and **COMPLEXITY** range sliders.
  - **Hide duplicates** collapses identical files. **Reset** clears the filters.
- Under **SEARCH**, type to narrow the list. **Save** keeps that search under **SAVED SEARCHES**.

## Piano roll

Double-click a file (or use the **Piano Roll** button) to open it.

**VELOCITY** is the lane along the bottom. Numbers on the left are 127, 96, 64, 32, and 0, with lines across the lane. Each bar is one note. Drag a bar up or down to change that note’s velocity. Click the **VELOCITY** header to fold the lane.

**Drag to DAW** in the header carries the edited clip onto a track.

## Toolkit

The column is titled **Toolkit**. Each block folds from its header.

**Playback.** **Tempo** is **Half** or **Double** against the clip or the host. **Trim empty measures** drops silent bars at the start and end (the same control on the piano roll reads **Restore** once it is on). **Extend** repeats the clip: Off, x2, x4, or x8, so a delay can run past the file.

**Timing.** **Quantize** snaps notes to a grid; **Strength** sets how far they move. **Swing** delays the offbeats; pick the grid, then **Amount**. **Pocket** pushes or lays back the feel. **Humanize** adds small timing variation. **Length** scales how long notes hold (100% is the file).

**Performance.** **Dynamics** changes accent contrast. **Intensity** scales velocities (100% is the file). **Articulation** is Off, Legato, Staccato, Tenuto, Pianissimo, Sforzando, or Slur, with a **Strength** slider. **Velocity** squeezes note velocities into a low–high range. **Sustain Pedal** is Off, Start of Clip, or Auto.

**Pitch & Scale.** **Octave** moves the clip to octave 0–6. **Pitch** shifts by semitones. **Key** and **Mode** (Major, Dorian, Phrygian, Lydian, Mixolydian, Minor, Locrian) choose the scale. **Fit to Scale** snaps notes into that scale. **Map to Root** transposes so the clip’s root matches the key. **Note filter** is a one-octave keyboard; turn pitches off, then set **Filter Type** to Mute or Fold.

**Effects.** **Complexity** simplifies or fills out the part around the file’s own level (the slider reads Clip at center). **Variations** cycles slight alternate takes (Off, then #1 and up). **Delay** repeats notes: pick a time, then **Amount** and **Feedback**. Complexity and Variations work on a file, so they hide in Passthrough.

## Locks

This is the point of browsing with effects on. The padlock on a section keeps that section’s settings and applies them to every file you play. The padlock in the **Toolkit** header locks every section at once (hover tip: Lock all Toolkit parameters while browsing).

Set swing, lock **Timing**, then arrow through the list. Each file keeps that swing. The header padlock does the same for the whole column.

## Passthrough

Turn on **Passthrough** and the window shows the header and the Toolkit. Play your keyboard through Timing, Performance, Pitch & Scale, and Delay. **Playback**, **Complexity**, and **Variations** hide here, because they need a whole file rather than a live stream. The window height follows which sections are open: fold a section and it gets shorter, open one and it grows.

## Things to try

1. **Open Folder...**, click a file, and press Space. Arrow up and down while it plays.
2. Open **Filters**, tap a **KEY** and a **TIME** such as 4/4, and watch the count beside the folder name. Hit **Reset** when you want the full list back.
3. Turn on **Scan Subdirectories** if files live in nested folders. Star the folder card so that folder and those filters stay under **LIBRARY**.
4. Click **Hide duplicates**, then click it again, and see how the count changes.
5. Raise **Swing** **Amount**, lock **Timing**, and arrow through files. Then lock the **Toolkit** header and change **Complexity** the same way.
6. Open a file and turn on **Trim empty measures**. The silent bars at the ends drop off. **Restore** puts them back.
7. In **Pitch & Scale**, pick a **Key** and **Mode**, turn on **Fit to Scale**, and audition a few files.
8. Shape a clip, then drag **Drag to DAW** onto a track. The clip on the track includes the Toolkit settings.
9. Turn on **Passthrough**, play a few notes, and raise **Swing** or **Pitch**. Open and close Toolkit sections so the window height follows.
