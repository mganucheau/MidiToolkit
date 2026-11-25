---
name: genre-style
description: How MidiToolkit genre packs are written. Use when adding a genre or changing how House, Breakbeat, Dub, Techno, Funk, Reggae, Dub Techno, or DNB generates drums, bass, chords, or arps.
---

# Genre style

MidiToolkit plays genre from JSON packs. The numbers live only in `Source/GenrePacks/*.json`. This skill does not repeat those grids. The plugin embeds the same files, and the tests read them through `genrePack()`.

## File

One file per genre, named after the `DrumStyle` value in lowercase:

`house.json`, `breakbeat.json`, `dub.json`, `techno.json`, `funk.json`, `reggae.json`, `dubtechno.json`, `dnb.json`.

The Genre menu label is not the filename. `dnb.json` is shown as **DNB**. `dubtechno.json` is shown as **Dub Techno**.

Add the file to `juce_add_binary_data` in `CMakeLists.txt`, and add a `DrumStyle` value **after** the existing ones so saved genre indexes do not shift. Then add a case in `Source/GenrePacks.cpp` `bytesFor` and a label in `drumStyleLabel`.

## Fields

```json
{
  "bpm": [120, 126],
  "swing": 0.12,
  "feel": { "density": 1.0, "syncopation": 0.3, "sustain": 0.0, "offbeats": false },
  "drums": {
    "kick": [[118, 0], [0, 0]],
    "snare": [],
    "hat": [],
    "ghost": []
  },
  "bass": { "gate": 0.4, "steps": [] },
  "chords": { "gate": 0.5, "steps": [] },
  "arp": { "steps": [] }
}
```

Each step list is 16 entries, one per 16th, starting on beat 1. An entry is `[velocity, ticks]`.

- Velocity `0` is an empty step. `1`–`127` plays.
- `ticks` is a timing offset at 480 PPQ. Positive values play late. A 16th is 120 ticks.
- `swing` is a fraction of a 16th added to the off-beat 8ths (steps 2, 6, 10, 14).
- `ghost` is a quiet snare voice. Breaks and DNB use it. It must stay quieter than the backbeat snare.
- `bass.gate` and `chords.gate` are lengths in quarter notes.
- `feel` is only the melody bias (density, syncopation, sustain, off-beat preference). Drums, bass, and chords follow their step lists.
- `arp.steps` chooses which of the 16 arp pattern steps are allowed to sound. The arp rate stays the Speed control, and the notes stay on that grid.

Kick is MIDI 36, snare and ghost are 38, hat is 42.

## What each genre is for

Do not copy another pack and rename it. These are the differences the tests check:

- **House.** Kick on all four beats. Hats on the off-beats, not on the downbeat.
- **Breakbeat.** Slower than DNB. Syncopated kick, not four-on-the-floor.
- **Reggae.** Off-beat chord skank, a moving bassline, kick not on every beat.
- **Dub.** Reggae family with space. Kick and snare together on beat 3. Fewer skanks and fewer bass notes than Reggae. Long gaps. The delay and reverb are not in the MIDI.
- **Techno.** Kick on all four beats. Hats or riffs on most steps. Short gates.
- **Dub Techno.** Same four-on-the-floor kick as Techno, with fewer hats, fewer chords, a longer bass note, and more empty steps. Not a beat-3 one-drop.
- **DNB.** About 170–176 BPM. Snare on beats 2 and 4, syncopated kicks, at least one ghost snare, rolling 16th bass. The menu label is DNB.
- **Funk.** Syncopated kick, ghost snares, swung hats.

## Adding a genre

1. Append a `DrumStyle` value. Do not insert it before `Funk`.
2. Write the JSON file. Rewrite the grids. Do not paste them from another project.
3. Register the file in `CMakeLists.txt` and `bytesFor`.
4. Add a menu label.
5. Add a test that would fail if this pack matched a neighbour (kick grid, hat count, bass length, or BPM).

## Licences

`Source/GenrePacks/NOTICE` records the references. beat-gen is MIT; anything taken from it needs attribution in that NOTICE, and rewritten grids are preferred. Do not ship patterns from the Drum Machine Patterns books. Do not copy GenreGrid files; that project is GPL-3.0.
