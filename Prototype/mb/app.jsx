// app.jsx — composes MidiToolkit: state, theme tokens, layout modes, playback, tweaks.
const { FOLDERS, SAVED_DIRS, emptyEdit, editIsClean, resolveClip, editBadges, emptyEdgeBars } = window.MB_DATA;

// ── Theme + accent token maps ────────────────────────────────────────────────
const ACCENTS = {
  blue:  { accent: '#4d87ff', ink: '#ffffff', soft: 'rgba(77,135,255,.16)',  line: 'rgba(77,135,255,.42)',  bright: '#9cbcff' },
  amber: { accent: '#f0a93b', ink: '#1c1304', soft: 'rgba(240,169,59,.16)',  line: 'rgba(240,169,59,.42)',  bright: '#ffd089' },
  mint:  { accent: '#2bd49f', ink: '#042019', soft: 'rgba(43,212,159,.15)',  line: 'rgba(43,212,159,.42)',  bright: '#79efc9' },
};
const THEMES = {
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

function themeVars(themeName, accentName) {
  const t = THEMES[themeName] || THEMES.charcoal;
  const a = ACCENTS[accentName] || ACCENTS.blue;
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

const params = new URLSearchParams(location.search);
const p = (k, d) => params.get(k) || d;

const TWEAK_DEFAULTS = /*EDITMODE-BEGIN*/{
  "grid": "lanes",
  "density": "compact",
  "accent": "blue",
  "theme": "charcoal"
}/*EDITMODE-END*/;

// URL params override tweak defaults (used by the canvas to show distinct states).
const INITIAL_TWEAKS = {
  grid: p('grid', TWEAK_DEFAULTS.grid),
  density: p('density', TWEAK_DEFAULTS.density),
  accent: p('accent', TWEAK_DEFAULTS.accent),
  theme: p('theme', TWEAK_DEFAULTS.theme),
};

function App() {
  const [t, setTweak] = useTweaks(INITIAL_TWEAKS);

  const [savedDirs, setSavedDirs] = React.useState(SAVED_DIRS);
  const [activeDir, setActiveDir] = React.useState(p('dir', 'bass'));
  const folder = FOLDERS.find((f) => f.id === activeDir) || FOLDERS[0];
  const files = folder.files;

  const [selectedId, setSelectedId] = React.useState(() => p('sel', files[3] ? files[3].id : files[0].id));
  const selected = files.find((f) => f.id === selectedId) || files[0];

  const [instrumentOpen, setInstrumentOpen] = React.useState(p('inst', '') === '1');
  const [sidebarCollapsed, setSidebarCollapsed] = React.useState(p('sidebar', '') === 'collapsed');
  const [rollOpen, setRollOpen] = React.useState(p('roll', '1') !== '0');                       // bottom preview folded?
  const [editorOpen, setEditorOpen] = React.useState(p('editor', '') === '1' || p('expanded', '') === '1'); // right dock
  const [synced, setSynced] = React.useState(true);
  const [playing, setPlaying] = React.useState(false);
  const [playhead, setPlayhead] = React.useState(0);

  // edits keyed by clip id (non-destructive). Optional demo edits for the canvas.
  const [edits, setEdits] = React.useState(() => {
    const init = {};
    if (p('demo', '') === 'edits' && selected) {
      init[selected.id] = { ...emptyEdit(), octave: 1, fitScale: true, root: 'F', mode: 'Dorian',
        moves: { 2: { dPitch: 7, dStep: 4 }, 4: { dPitch: -5, dStep: 0 } } };
    }
    return init;
  });
  const edit = edits[selectedId] || emptyEdit();
  const setEdit = React.useCallback((patch) => {
    setEdits((prev) => ({ ...prev, [selectedId]: { ...(prev[selectedId] || emptyEdit()), ...patch } }));
  }, [selectedId]);

  const resolved = React.useMemo(() => resolveClip(selected, edit), [selected, edit]);
  const badges = React.useMemo(() => editBadges(selected, edit), [selected, edit]);

  // trim availability
  const preTrim = React.useMemo(() => resolveClip(selected, { ...edit, trimLead: 0, trimTail: 0 }), [selected, edit]);
  const edges = React.useMemo(() => emptyEdgeBars(preTrim.notes, selected.bars), [preTrim, selected]);
  const canTrim = (edges.lead + edges.tail) > ((edit.trimLead || 0) + (edit.trimTail || 0));

  const onTrim = () => setEdit({ trimLead: edges.lead, trimTail: edges.tail });
  const onMoveNote = React.useCallback((noteId, dPitch, dStep) => {
    setEdits((prev) => {
      const e = prev[selectedId] || emptyEdit();
      const cur = e.moves[noteId] || { dPitch: 0, dStep: 0 };
      return { ...prev, [selectedId]: { ...e, moves: { ...e.moves, [noteId]: { dPitch: cur.dPitch + dPitch, dStep: cur.dStep + dStep } } } };
    });
  }, [selectedId]);
  const onRevert = (key) => {
    if (key === 'all') return setEdit({ ...emptyEdit() });
    if (key === 'oct') return setEdit({ octave: 0 });
    if (key === 'scale') return setEdit({ fitScale: false });
    if (key === 'moves') return setEdit({ moves: {} });
    if (key === 'trim') return setEdit({ trimLead: 0, trimTail: 0 });
  };

  // playback clock
  const totalSteps = resolved.bars * window.MB_DATA.STEPS_PER_BAR;
  React.useEffect(() => {
    if (!playing) return undefined;
    const msPerStep = (60 / selected.bpm / 4) * 1000;
    const id = setInterval(() => setPlayhead((s) => (s + 1) % totalSteps), msPerStep);
    return () => clearInterval(id);
  }, [playing, selected.bpm, totalSteps]);
  React.useEffect(() => { if (playhead >= totalSteps) setPlayhead(0); }, [totalSteps, playhead]);

  const pickDir = (id) => {
    setActiveDir(id);
    const fld = FOLDERS.find((f) => f.id === id);
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
    instrumentOpen, onToggleInstrument: () => setInstrumentOpen((v) => !v), onEdit: setEdit,
    onMoveNote, onTrim, onRevert, onCollapse: () => setEditorOpen(false), badges, canTrim,
  };

  // ── Bottom dock: foldable mini piano-roll preview ──────────────────────────
  const bottomDock = (
    <div style={{ flex: '0 0 auto', borderTop: '1px solid var(--mb-line)', background: 'var(--mb-panel)' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '0 8px 0 8px', height: 34 }}>
        <button type="button" onClick={() => setRollOpen((v) => !v)} title={rollOpen ? 'Fold piano roll' : 'Show piano roll'}
          style={{ width: 24, height: 24, flex: '0 0 auto', borderRadius: 6, border: 0, background: 'transparent', color: 'var(--mb-text2)', cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
          <MBIcon name={rollOpen ? 'caret-down' : 'caret-up'} weight="bold" size={12} />
        </button>
        <MBIcon name="piano-keys" weight="fill" size={13} style={{ color: 'var(--mb-text2)', flex: '0 0 auto' }} />
        <span style={{ flex: '0 1 auto', minWidth: 0, fontSize: 12, fontWeight: 600, color: 'var(--mb-text)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', maxWidth: 200 }}>{selected.name}</span>
        <div style={{ display: 'flex', gap: 5, overflow: 'hidden', flexShrink: 1, minWidth: 0 }}>{badges.map((b) => <Badge key={b.key} tone="mute">{b.label}</Badge>)}</div>
        <div style={{ flex: 1, minWidth: 8 }} />
        {!editorOpen
          ? <Btn icon="arrows-out-line-horizontal" variant="accent" onClick={() => setEditorOpen(true)}>Open editor</Btn>
          : <Btn icon="sliders-horizontal" iconWeight="fill" variant="accent" active onClick={() => setEditorOpen(false)}>Editing</Btn>}
      </div>
      {rollOpen && <PianoRollMini clip={selected} resolved={resolved} gridStyle={t.grid} height={t.density === 'comfortable' ? 134 : 116} playhead={playhead} playing={playing} />}
    </div>
  );

  // Workspace: sidebar | (file list + foldable bottom roll) | foldable editor dock
  const body = (
    <div style={{ flex: 1, display: 'flex', minHeight: 0 }}>
      <Sidebar collapsed={sidebarCollapsed} onToggle={() => setSidebarCollapsed((v) => !v)}
        dirs={savedDirs} folders={FOLDERS} activeId={activeDir} onPick={pickDir} onAdd={addCurrent} onRemove={removeDir} />
      <div style={{ flex: 1, minWidth: 0, display: 'flex', flexDirection: 'column', minHeight: 0 }}>
        <FileList folder={folder} files={files} selectedId={selectedId} onSelect={(id) => { setSelectedId(id); }}
          edits={edits} playing={playing} playingId={selectedId} density={t.density} onPreviewPlay={previewPlay} />
        {bottomDock}
      </div>
      {editorOpen && (
        <div style={{ flex: '0 0 600px', width: 600, display: 'flex', minHeight: 0 }}>
          <PianoRollEditor {...editorProps} />
        </div>
      )}
    </div>
  );

  // The app presents as a desktop window: opening the editor dock grows the whole
  // window by the dock's width (the browser pane keeps its size) rather than squeezing it.
  const winWidth = editorOpen ? 560 + 600 : 560;
  return (
    <div className="mb-desktop" style={{ ...themeVars(t.theme, t.accent), position: 'absolute', inset: 0, display: 'flex', alignItems: 'center', justifyContent: 'center',
      padding: 24, background: 'color-mix(in oklab, var(--mb-bg), #000 48%)',
      fontFamily: "'Schibsted Grotesk', ui-sans-serif, system-ui, -apple-system, sans-serif" }}>
      <div className="mb-root" style={{ width: winWidth, maxWidth: '100%', height: '100%', maxHeight: 900, display: 'flex', flexDirection: 'column',
        background: 'var(--mb-bg)', color: 'var(--mb-text)', borderRadius: 12, overflow: 'hidden',
        boxShadow: '0 30px 80px rgba(0,0,0,.55), 0 0 0 1px var(--mb-line-strong)',
        transition: 'width .34s cubic-bezier(.4,0,.2,1)' }}>
        <Transport playing={playing} onPlay={() => setPlaying((v) => !v)} onStop={() => { setPlaying(false); setPlayhead(0); }}
          bpm={selected.bpm} synced={synced} folder={folder} onToggleSync={() => setSynced((v) => !v)} density={t.density} />
        {body}
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

ReactDOM.createRoot(document.getElementById('root')).render(<App />);
