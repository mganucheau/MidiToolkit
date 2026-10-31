// data.jsx — MidiToolkit mock data, music theory, and non-destructive transforms.
// No mutation of source clips: every edit lives in a separate edit record.

// ── Music theory ────────────────────────────────────────────────────────────
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const ROOTS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const MODES = {
  Ionian:     [0, 2, 4, 5, 7, 9, 11],
  Dorian:     [0, 2, 3, 5, 7, 9, 10],
  Phrygian:   [0, 1, 3, 5, 7, 8, 10],
  Lydian:     [0, 2, 4, 6, 7, 9, 11],
  Mixolydian: [0, 2, 4, 5, 7, 9, 10],
  Aeolian:    [0, 2, 3, 5, 7, 8, 10],
  Locrian:    [0, 1, 3, 5, 6, 8, 10],
};
const MODE_NAMES = Object.keys(MODES);

function pitchName(midi) {
  const n = NOTE_NAMES[((midi % 12) + 12) % 12];
  const oct = Math.floor(midi / 12) - 1; // MIDI 60 = C4
  return `${n}${oct}`;
}
function isBlackKey(midi) { return [1, 3, 6, 8, 10].includes(((midi % 12) + 12) % 12); }
function rootIndex(name) { return ROOTS.indexOf(name); }

// Quantize a midi pitch to the nearest tone of scale(root, mode), preserving register.
function fitToScale(midi, rootName, modeName) {
  const root = rootIndex(rootName);
  const intervals = MODES[modeName] || MODES.Ionian;
  const scalePCs = intervals.map((i) => (root + i) % 12);
  const pc = ((midi % 12) + 12) % 12;
  if (scalePCs.includes(pc)) return midi;
  // nearest scale pitch-class by absolute semitone distance (search ±6)
  let best = midi, bestDist = 99;
  for (let d = -6; d <= 6; d++) {
    const cand = midi + d;
    if (scalePCs.includes(((cand % 12) + 12) % 12)) {
      const dist = Math.abs(d);
      if (dist < bestDist) { bestDist = dist; best = cand; }
    }
  }
  return best;
}

// ── PRNG ──────────────────────────────────────────────────────────────────
function hashStr(s) {
  let h = 1779033703 ^ s.length;
  for (let i = 0; i < s.length; i++) {
    h = Math.imul(h ^ s.charCodeAt(i), 3432918353);
    h = (h << 13) | (h >>> 19);
  }
  return h >>> 0;
}
function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

const STEPS_PER_BAR = 16; // 4/4, 16th grid

// Generate a believable monophonic bass line for a clip.
// Some bars are intentionally left empty (esp. at the head/tail) so Trim has work to do.
function genBass(name, rootName, bars) {
  const rng = mulberry32(hashStr(name));
  const root = 36 + rootIndex(rootName); // around C2
  const scale = MODES.Aeolian.map((i) => i); // bass tends minor-ish
  const degreesPitches = scale.map((i) => root + i);
  const choices = [
    root, root, root + 12, degreesPitches[4] || root + 7,
    degreesPitches[2] || root + 3, degreesPitches[5] || root + 8, root - 12,
  ];
  const notes = [];
  let id = 0;
  // leading empty bars
  const lead = rng() < 0.7 ? (rng() < 0.5 ? 1 : 2) : 0;
  const tail = rng() < 0.6 ? 1 : 0;
  for (let bar = lead; bar < bars - tail; bar++) {
    // a few hits per bar
    const density = 2 + Math.floor(rng() * 3);
    let step = 0;
    for (let k = 0; k < density && step < STEPS_PER_BAR; k++) {
      const gap = [2, 2, 3, 4, 4][Math.floor(rng() * 5)];
      const len = Math.min([2, 3, 4][Math.floor(rng() * 3)], STEPS_PER_BAR - step);
      const pitch = choices[Math.floor(rng() * choices.length)];
      notes.push({ id: id++, pitch, start: bar * STEPS_PER_BAR + step, len });
      step += len + (gap - len > 0 ? 0 : 0) + gap;
      step = Math.min(step, STEPS_PER_BAR);
      if (step + 1 >= STEPS_PER_BAR) break;
    }
  }
  return notes;
}

// Lead/keys clip: a sparse two-note motif (still simple, reads clearly in roll).
function genKeys(name, rootName, bars) {
  const rng = mulberry32(hashStr(name + 'k'));
  const root = 60 + rootIndex(rootName); // around C4
  const intervals = MODES.Dorian;
  const pitches = intervals.map((i) => root + i).concat(intervals.map((i) => root + 12 + i));
  const notes = [];
  let id = 0;
  const lead = rng() < 0.5 ? 1 : 0;
  for (let bar = lead; bar < bars; bar++) {
    if (rng() < 0.18) continue; // occasional rest bar
    const hits = 1 + Math.floor(rng() * 3);
    let step = Math.floor(rng() * 3);
    for (let k = 0; k < hits; k++) {
      const len = [2, 4, 4, 6][Math.floor(rng() * 4)];
      const pitch = pitches[Math.floor(rng() * pitches.length)];
      notes.push({ id: id++, pitch, start: bar * STEPS_PER_BAR + step, len });
      step += len + 1 + Math.floor(rng() * 3);
      if (step >= STEPS_PER_BAR - 1) break;
    }
  }
  return notes;
}

function genDrums(name, _root, bars) {
  // map to a tight pitch band so the roll reads like a drum lane
  const rng = mulberry32(hashStr(name + 'd'));
  const KICK = 36, SNARE = 38, HAT = 42;
  const notes = [];
  let id = 0;
  for (let bar = 0; bar < bars; bar++) {
    for (let s = 0; s < STEPS_PER_BAR; s += 2) {
      if (rng() < 0.85) notes.push({ id: id++, pitch: HAT, start: bar * STEPS_PER_BAR + s, len: 1 });
    }
    notes.push({ id: id++, pitch: KICK, start: bar * STEPS_PER_BAR, len: 2 });
    if (rng() < 0.6) notes.push({ id: id++, pitch: KICK, start: bar * STEPS_PER_BAR + 10, len: 2 });
    notes.push({ id: id++, pitch: SNARE, start: bar * STEPS_PER_BAR + 4, len: 2 });
    notes.push({ id: id++, pitch: SNARE, start: bar * STEPS_PER_BAR + 12, len: 2 });
  }
  return notes;
}

// ── File set ────────────────────────────────────────────────────────────────
// Mirrors the original screenshot's bass folder, plus extra saved directories.
const BASS_ROOTS = ['G', 'G#', 'A', 'F', 'G#', 'B', 'D', 'Gm', 'F', 'E', 'G', 'A#', 'C#', 'D#', 'F#', 'A', 'C', 'E', 'G', 'B'];
function makeBassFiles() {
  return BASS_ROOTS.map((r, i) => {
    const clean = r.replace('m', '');
    const name = `${i + 1}. Bass 124 bpm ${r}.mid`;
    const bars = 8;
    return { id: `bass-${i}`, name, kind: 'bass', root: clean, bpm: 124, bars, notes: genBass(name, clean, bars) };
  });
}
function makeKeysFiles() {
  const set = [
    ['Rhodes Em7 verse', 'E'], ['Pad swell Cmaj', 'C'], ['Pluck motif A', 'A'],
    ['Wurli chords Dm', 'D'], ['Bell arp F#', 'F#'], ['Organ stab G', 'G'],
    ['Choir bed Bb', 'A#'], ['Lead hook E', 'E'],
  ];
  return set.map(([label, root], i) => {
    const name = `${label}.mid`;
    const bars = 8;
    return { id: `keys-${i}`, name, kind: 'keys', root, bpm: 120, bars, notes: genKeys(name, root, bars) };
  });
}
function makeDrumFiles() {
  const set = ['Break 90 dusty', 'Four-floor 124', 'Half-time 140', 'Garage shuffle 130', 'Trap hats 145', 'Boom-bap 88'];
  return set.map((label, i) => {
    const name = `${label}.mid`;
    const bars = 4;
    return { id: `drum-${i}`, name, kind: 'drums', root: 'C', bpm: 124, bars, notes: genDrums(name, 'C', bars) };
  });
}

const FOLDERS = [
  { id: 'bass', name: 'Bass Loops 124', path: '~/Samples/Bass Loops 124', files: makeBassFiles() },
  { id: 'keys', name: 'Keys & Pads', path: '~/Samples/Keys + Pads', files: makeKeysFiles() },
  { id: 'drums', name: 'Drum MIDI', path: '~/Samples/Drum Kits/MIDI', files: makeDrumFiles() },
];

// Saved directories the user has pinned (favorites). The first is the active one.
const SAVED_DIRS = [
  { id: 'bass', label: 'Bass Loops 124' },
  { id: 'keys', label: 'Keys & Pads' },
  { id: 'drums', label: 'Drum MIDI' },
];

// ── Non-destructive transform pipeline ───────────────────────────────────────
// edit = { octave, fitScale, root, mode, moves:{[noteId]:{dPitch,dStep}}, trimLead, trimTail }
function emptyEdit() {
  return { octave: 0, fitScale: false, mapToRoot: false, root: null, mode: 'Dorian', moves: {}, trimLead: 0, trimTail: 0 };
}
function editIsClean(e) {
  if (!e) return true;
  return e.octave === 0 && !e.fitScale && !e.mapToRoot && Object.keys(e.moves || {}).length === 0
    && (e.trimLead || 0) === 0 && (e.trimTail || 0) === 0;
}

// Count how many leading / trailing full bars are empty in the (move+octave applied) clip.
function emptyEdgeBars(notes, bars) {
  if (!notes.length) return { lead: 0, tail: 0 };
  let minStep = Infinity, maxStep = -Infinity;
  for (const n of notes) { minStep = Math.min(minStep, n.start); maxStep = Math.max(maxStep, n.start + n.len); }
  const lead = Math.floor(minStep / STEPS_PER_BAR);
  const tail = bars - Math.ceil(maxStep / STEPS_PER_BAR);
  return { lead: Math.max(0, lead), tail: Math.max(0, tail) };
}

// Produce the effective notes + effective bar count for display & playback.
function resolveClip(clip, edit) {
  const e = edit || emptyEdit();
  let notes = clip.notes.map((n) => {
    const mv = e.moves[n.id] || { dPitch: 0, dStep: 0 };
    let pitch = n.pitch + mv.dPitch + e.octave * 12;
    if (e.mapToRoot && e.root && clip.root) {
      let d = (rootIndex(e.root) - rootIndex(clip.root)) % 12;
      if (d > 6) d -= 12; if (d < -6) d += 12;
      pitch += d;
    }
    if (e.fitScale && e.root) pitch = fitToScale(pitch, e.root, e.mode);
    return { ...n, pitch, start: n.start + mv.dStep, moved: !!(mv.dPitch || mv.dStep) };
  });
  let bars = clip.bars;
  const lead = e.trimLead || 0, tail = e.trimTail || 0;
  if (lead || tail) {
    notes = notes.map((n) => ({ ...n, start: n.start - lead * STEPS_PER_BAR }));
    bars = clip.bars - lead - tail;
  }
  return { notes, bars: Math.max(1, bars) };
}

function editBadges(clip, edit) {
  const e = edit || emptyEdit();
  const out = [];
  if (e.octave) out.push({ key: 'oct', label: `Oct ${e.octave > 0 ? '+' : ''}${e.octave}` });
  if (e.fitScale && e.root) out.push({ key: 'scale', label: `${e.root} ${e.mode}` });
  if (e.mapToRoot && e.root) out.push({ key: 'map', label: `→ ${e.root} root` });
  const moved = Object.values(e.moves || {}).filter((m) => m.dPitch || m.dStep).length;
  if (moved) out.push({ key: 'moves', label: `${moved} note${moved > 1 ? 's' : ''} moved` });
  const trimBars = (e.trimLead || 0) + (e.trimTail || 0);
  if (trimBars) out.push({ key: 'trim', label: `Trim −${trimBars} bar${trimBars > 1 ? 's' : ''}` });
  return out;
}

Object.assign(window, {
  MB_DATA: {
    FOLDERS, SAVED_DIRS, MODES, MODE_NAMES, ROOTS, STEPS_PER_BAR,
    pitchName, isBlackKey, rootIndex, fitToScale,
    emptyEdit, editIsClean, resolveClip, editBadges, emptyEdgeBars,
  },
});
