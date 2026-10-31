// app-d.jsx — MidiToolkit "D" focused variant: compact file browser + full-height
// piano roll + folding Groove knob panel. Editor is always open (no right-dock toggle).
const { FOLDERS: FOLDERS_D, SAVED_DIRS: SAVED_DIRS_D, emptyEdit: emptyEditD, resolveClip: resolveClipD, editBadges: editBadgesD, emptyEdgeBars: emptyEdgeBarsD } = window.MB_DATA;

const ACCENTS_D = {
  blue:  { accent: '#4d87ff', ink: '#ffffff', soft: 'rgba(77,135,255,.16)',  line: 'rgba(77,135,255,.42)',  bright: '#9cbcff' },
  amber: { accent: '#f0a93b', ink: '#1c1304', soft: 'rgba(240,169,59,.16)',  line: 'rgba(240,169,59,.42)',  bright: '#ffd089' },
  mint:  { accent: '#2bd49f', ink: '#042019', soft: 'rgba(43,212,159,.15)',  line: 'rgba(43,212,159,.42)',  bright: '#79efc9' },
};
const THEMES_D = {
  charcoal: {
    bg: '#131419', panel: '#181a20', panel2: '#1c1f26', elev: '#24272f',
    line: 'rgba(255,255,255,.08)', lineStrong: 'rgba(255,255,255,.15)',
    text: '#e9eaee', text2: '#a6a9b3', text3: '#6c7079',
    rollBg: '#14161b', rollShade: 'rgba(255,255,255,.028)', rollRowline: 'rgba(255,255,255,.045)',
    rollNoteEdge: 'rgba(0,0,0,.28)', rollGhost: 'rgba(255,255,255,.28)', kbWhite: '#2b2e36', kbBlack: '#1a1c22',
  },
  graphite: {
    bg: '#18160f', panel: '#1e1b15', panel2: '#221e17', elev: '#2b261d',
    line: 'rgba(255,250,235,.08)', lineStrong: 'rgba(255,250,235,.15)',
    text: '#ece8df', text2: '#aca598', text3: '#726c5f',
    rollBg: '#15130d', rollShade: 'rgba(255,248,230,.03)', rollRowline: 'rgba(255,248,230,.05)',
    rollNoteEdge: 'rgba(0,0,0,.3)', rollGhost: 'rgba(255,248,230,.28)', kbWhite: '#2f2a20', kbBlack: '#1c1810',
  },
  ink: {
    bg: '#0d0f16', panel: '#11141d', panel2: '#141826', elev: '#1b2030',
    line: 'rgba(180,200,255,.09)', lineStrong: 'rgba(180,200,255,.16)',
    text: '#e6e9f3', text2: '#9ca3b8', text3: '#636b82',
    rollBg: '#0e1119', rollShade: 'rgba(150,180,255,.03)', rollRowline: 'rgba(150,180,255,.05)',
    rollNoteEdge: 'rgba(0,0,0,.32)', rollGhost: 'rgba(170,190,255,.3)', kbWhite: '#262c3d', kbBlack: '#161a26',
  },
};

function themeVarsD(themeName, accentName) {
  const t = THEMES_D[themeName] || THEMES_D.graphite;
  const a = ACCENTS_D[accentName] || ACCENTS_D.amber;
  return {
    '--mb-bg': t.bg, '--mb-panel': t.panel, '--mb-panel2': t.panel2, '--mb-elev': t.elev,
    '--mb-line': t.line, '--mb-line-strong': t.lineStrong,
    '--mb-text': t.text, '--mb-text2': t.text2, '--mb-text3': t.text3,
    '--mb-accent': a.accent, '--mb-accent-ink': a.ink, '--mb-accent-soft': a.soft, '--mb-accent-line': a.line, '--mb-accent-bright': a.bright,
    '--mb-roll-bg': t.rollBg, '--mb-roll-shade': t.rollShade, '--mb-roll-rowline': t.rollRowline,
    '--mb-roll-noteedge': t.rollNoteEdge, '--mb-roll-ghost': t.rollGhost,
    '--mb-kb-white': t.kbWhite, '--mb-kb-black': t.kbBlack, '--mb-playhead': '#ff5a52',
    '--mb-roll-bp-bg': '#0c1a25', '--mb-roll-bp-bar': 'rgba(110,200,255,.4)', '--mb-roll-bp-beat': 'rgba(110,200,255,.13)',
    '--mb-roll-bp-row': 'rgba(110,200,255,.06)', '--mb-roll-bp-note': '#46c2ff', '--mb-roll-bp-edge': 'rgba(190,235,255,.55)',
    '--mb-mono': "'JetBrains Mono', ui-monospace, SFMono-Regular, Menlo, monospace",
  };
}

const paramsD = new URLSearchParams(location.search);
const pD = (k, d) => paramsD.get(k) || d;

const TWEAK_DEFAULTS_D = /*EDITMODE-BEGIN*/{
  "grid": "minimal",
  "density": "compact",
  "accent": "amber",
  "theme": "graphite"
}/*EDITMODE-END*/;

const INITIAL_TWEAKS_D = {
  grid: pD('grid', TWEAK_DEFAULTS_D.grid),
  density: pD('density', TWEAK_DEFAULTS_D.density),
  accent: pD('accent', TWEAK_DEFAULTS_D.accent),
  theme: pD('theme', TWEAK_DEFAULTS_D.theme),
};

function AppD() {
  const [t, setTweak] = useTweaks(INITIAL_TWEAKS_D);

  const [savedDirs, setSavedDirs] = React.useState(SAVED_DIRS_D);
  const [activeDir, setActiveDir] = React.useState(pD('dir', 'bass'));
  const folder = FOLDERS_D.find((f) => f.id === activeDir) || FOLDERS_D[0];
  const files = folder.files;

  const [selectedId, setSelectedId] = React.useState(() => (files[3] ? files[3].id : files[0].id));
  const selected = files.find((f) => f.id === selectedId) || files[0];

  const [sidebarCollapsed, setSidebarCollapsed] = React.useState(true); // compact icon rail
  const [knobsOpen, setKnobsOpen] = React.useState(true);
  const [velOpen, setVelOpen] = React.useState(true);
  const [editorFolded, setEditorFolded] = React.useState(false);
  const [miniOpen, setMiniOpen] = React.useState(true);
  const [synced, setSynced] = React.useState(true);
  const [freeBpm, setFreeBpm] = React.useState(124);
  const [playing, setPlaying] = React.useState(false);
  const [playhead, setPlayhead] = React.useState(0);
  const bpm = synced ? selected.bpm : freeBpm;

  // Groove knob values, kept per clip.
  const [knobsByClip, setKnobsByClip] = React.useState({});
  const knobs = knobsByClip[selectedId] || DEFAULT_KNOBS;
  const setKnob = (key, val) => setKnobsByClip((prev) => ({ ...prev, [selectedId]: { ...(prev[selectedId] || DEFAULT_KNOBS), [key]: val } }));
  const resetKnobs = () => setKnobsByClip((prev) => ({ ...prev, [selectedId]: { ...DEFAULT_KNOBS } }));

  // demo edits so the roll reads as a real working session
  const [edits, setEdits] = React.useState(() => {
    const init = {};
    if (selected) {
      init[selected.id] = { ...emptyEditD(), octave: 1, fitScale: true, root: 'F', mode: 'Dorian',
        moves: { 2: { dPitch: 7, dStep: 4 }, 4: { dPitch: -5, dStep: 0 } } };
    }
    return init;
  });
  const edit = edits[selectedId] || emptyEditD();
  const setEdit = React.useCallback((patch) => {
    setEdits((prev) => ({ ...prev, [selectedId]: { ...(prev[selectedId] || emptyEditD()), ...patch } }));
  }, [selectedId]);

  const resolved = React.useMemo(() => resolveClipD(selected, edit), [selected, edit]);
  const badges = React.useMemo(() => editBadgesD(selected, edit), [selected, edit]);

  const preTrim = React.useMemo(() => resolveClipD(selected, { ...edit, trimLead: 0, trimTail: 0 }), [selected, edit]);
  const edges = React.useMemo(() => emptyEdgeBarsD(preTrim.notes, selected.bars), [preTrim, selected]);
  const canTrim = (edges.lead + edges.tail) > ((edit.trimLead || 0) + (edit.trimTail || 0));
  const isTrimmed = ((edit.trimLead || 0) + (edit.trimTail || 0)) > 0;

  const onTrim = () => (isTrimmed ? setEdit({ trimLead: 0, trimTail: 0 }) : setEdit({ trimLead: edges.lead, trimTail: edges.tail }));
  const onMoveNote = React.useCallback((noteId, dPitch, dStep) => {
    setEdits((prev) => {
      const e = prev[selectedId] || emptyEditD();
      const cur = e.moves[noteId] || { dPitch: 0, dStep: 0 };
      return { ...prev, [selectedId]: { ...e, moves: { ...e.moves, [noteId]: { dPitch: cur.dPitch + dPitch, dStep: cur.dStep + dStep } } } };
    });
  }, [selectedId]);
  const onRevert = (key) => {
    if (key === 'all') return setEdit({ ...emptyEditD() });
  };

  const totalSteps = resolved.bars * window.MB_DATA.STEPS_PER_BAR;
  React.useEffect(() => {
    if (!playing) return undefined;
    const msPerStep = (60 / bpm / 4) * 1000;
    const id = setInterval(() => setPlayhead((s) => (s + 1) % totalSteps), msPerStep);
    return () => clearInterval(id);
  }, [playing, bpm, totalSteps]);
  React.useEffect(() => { if (playhead >= totalSteps) setPlayhead(0); }, [totalSteps, playhead]);

  // Up/Down arrows step through the file list (ignored while a form control is focused).
  React.useEffect(() => {
    const onKey = (e) => {
      if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') return;
      const tag = (e.target && e.target.tagName || '').toLowerCase();
      if (tag === 'select' || tag === 'input' || tag === 'textarea') return;
      const idx = files.findIndex((f) => f.id === selectedId);
      if (idx < 0) return;
      e.preventDefault();
      const next = e.key === 'ArrowDown' ? Math.min(files.length - 1, idx + 1) : Math.max(0, idx - 1);
      if (files[next] && files[next].id !== selectedId) { setSelectedId(files[next].id); setPlayhead(0); }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [files, selectedId]);

  const pickDir = (id) => {
    setActiveDir(id);
    const fld = FOLDERS_D.find((f) => f.id === id);
    if (fld && fld.files[0]) setSelectedId(fld.files[0].id);
  };
  const removeDir = (id) => setSavedDirs((d) => d.filter((x) => x.id !== id));
  const addCurrent = () => setSavedDirs((d) => d.some((x) => x.id === folder.id) ? d : [...d, { id: folder.id, label: folder.name }]);

  const previewPlay = (id) => {
    if (id !== selectedId) { setSelectedId(id); setPlayhead(0); setPlaying(true); return; }
    setPlaying((v) => !v);
  };

  const editorProps = {
    clip: selected, edit, resolved, gridStyle: t.grid, density: t.density, playhead, playing,
    onEdit: setEdit, onMoveNote, onTrim, onRevert, badges, canTrim, isTrimmed,
    knobs, onKnob: setKnob, knobsOpen, onToggleKnobs: () => setKnobsOpen((v) => !v), onResetKnobs: resetKnobs,
    velOpen, onToggleVel: () => setVelOpen((v) => !v),
  };

  const winWidth = editorFolded ? 300 : 980;
  return (
    <div className="mb-desktop" style={{ ...themeVarsD(t.theme, t.accent), position: 'absolute', inset: 0, display: 'flex', alignItems: 'center', justifyContent: 'center',
      padding: 24, background: 'color-mix(in oklab, var(--mb-bg), #000 48%)',
      fontFamily: "'Schibsted Grotesk', ui-sans-serif, system-ui, -apple-system, sans-serif" }}>
      <div className="mb-root" style={{ width: winWidth, maxWidth: '100%', minWidth: 0, height: 648, maxHeight: '100%', display: 'flex', flexDirection: 'column',
        background: 'var(--mb-bg)', color: 'var(--mb-text)', borderRadius: 12, overflow: 'hidden',
        boxShadow: '0 30px 80px rgba(0,0,0,.55), 0 0 0 1px var(--mb-line-strong)' }}>
        <Transport playing={playing} onPlay={() => setPlaying((v) => !v)} onStop={() => { setPlaying(false); setPlayhead(0); }}
          bpm={bpm} synced={synced} folder={folder} onToggleSync={() => setSynced((v) => !v)} onBpm={setFreeBpm} density={t.density}
          compact={editorFolded} editorFolded={editorFolded} onToggleEditor={() => setEditorFolded((v) => !v)} />

        {/* body: compact rail | file browser | editor (foldable) */}
        {editorFolded ? (
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minHeight: 0 }}>
            {/* browser row: sidebar + file list, sits above the full-width preview */}
            <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
              <Sidebar collapsed={sidebarCollapsed} onToggle={() => setSidebarCollapsed((v) => !v)}
                dirs={savedDirs} folders={FOLDERS_D} activeId={activeDir} onPick={pickDir} onAdd={addCurrent} onRemove={removeDir} />
              <div style={{ flex: 1, minWidth: 0, display: 'flex', flexDirection: 'column', minHeight: 0 }}>
                <FileList folder={folder} files={files} selectedId={selectedId} onSelect={(id) => setSelectedId(id)}
                  edits={edits} playing={playing} playingId={selectedId} density={t.density} onPreviewPlay={previewPlay} />
              </div>
            </div>
            {/* full-width preview panel */}
            <div style={{ flex: '0 0 auto', borderTop: '1px solid var(--mb-line)', background: 'var(--mb-panel)' }}>
              <button type="button" onClick={() => setMiniOpen((v) => !v)} title={miniOpen ? 'Minimize preview' : 'Show preview'}
                style={{ display: 'flex', alignItems: 'center', gap: 8, width: '100%', height: 32, padding: '0 12px', border: 0, background: 'transparent', cursor: 'pointer', font: 'inherit' }}>
                <MBIcon name={miniOpen ? 'caret-down' : 'caret-up'} weight="bold" size={11} style={{ color: 'var(--mb-text3)', flex: '0 0 auto' }} />
                <MBIcon name="piano-keys" weight="fill" size={13} style={{ color: 'var(--mb-text2)', flex: '0 0 auto' }} />
                <span style={{ fontSize: 11.5, fontWeight: 600, color: 'var(--mb-text)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', minWidth: 0, flex: 1, textAlign: 'left' }}>{selected.name}</span>
                <span style={{ fontSize: 10.5, color: 'var(--mb-text3)', fontFamily: 'var(--mb-mono)', whiteSpace: 'nowrap', flex: '0 0 auto' }}>{resolved.bars} bars</span>
              </button>
              {miniOpen && <PianoRollMini clip={selected} resolved={resolved} gridStyle={t.grid} height={t.density === 'comfortable' ? 128 : 112} playhead={playhead} playing={playing} />}
            </div>
          </div>
        ) : (
          <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
            <Sidebar collapsed={sidebarCollapsed} onToggle={() => setSidebarCollapsed((v) => !v)}
              dirs={savedDirs} folders={FOLDERS_D} activeId={activeDir} onPick={pickDir} onAdd={addCurrent} onRemove={removeDir} />
            <div style={{ flex: '0 0 244px', width: 244, display: 'flex', flexDirection: 'column', minHeight: 0 }}>
              <FileList folder={folder} files={files} selectedId={selectedId} onSelect={(id) => setSelectedId(id)}
                edits={edits} playing={playing} playingId={selectedId} density={t.density} onPreviewPlay={previewPlay} />
            </div>
            <div style={{ flex: 1, minWidth: 0, display: 'flex', minHeight: 0 }}>
              <PianoRollEditorD {...editorProps} />
            </div>
          </div>
        )}
      </div>

      <TweaksPanel title="Tweaks">
        <TweakSection label="Piano roll" />
        <TweakRadio label="Note grid" value={t.grid} options={[{ value: 'lanes', label: 'Lanes' }, { value: 'minimal', label: 'Minimal' }, { value: 'blueprint', label: 'Blueprint' }]} onChange={(v) => setTweak('grid', v)} />
        <TweakSection label="Density" />
        <TweakRadio label="Spacing" value={t.density} options={[{ value: 'compact', label: 'Compact' }, { value: 'comfortable', label: 'Comfortable' }]} onChange={(v) => setTweak('density', v)} />
        <TweakSection label="Theme" />
        <TweakRadio label="Surface" value={t.theme} options={[{ value: 'charcoal', label: 'Charcoal' }, { value: 'graphite', label: 'Graphite' }, { value: 'ink', label: 'Ink' }]} onChange={(v) => setTweak('theme', v)} />
        <TweakColor label="Accent" value={t.accent === 'blue' ? '#4d87ff' : t.accent === 'amber' ? '#f0a93b' : '#2bd49f'}
          options={['#4d87ff', '#f0a93b', '#2bd49f']}
          onChange={(hex) => setTweak('accent', hex === '#4d87ff' ? 'blue' : hex === '#f0a93b' ? 'amber' : 'mint')} />
      </TweaksPanel>
    </div>
  );
}

ReactDOM.createRoot(document.getElementById('root')).render(<AppD />);
