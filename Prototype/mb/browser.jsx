// browser.jsx — transport bar, collapsible favorites sidebar, file list.
const { MODE_NAMES } = window.MB_DATA;

// ── Transport / header ───────────────────────────────────────────────────────
function Transport({ playing, onPlay, onStop, bpm, synced, folder, onToggleSync, density, compact, editorFolded, onToggleEditor }) {
  const pad = density === 'comfortable' ? '0 16px' : '0 12px';
  return (
    <header style={{
      display: 'flex', alignItems: 'center', gap: 12, height: density === 'comfortable' ? 52 : 46,
      padding: pad, borderBottom: '1px solid var(--mb-line)', background: 'var(--mb-panel)', flex: '0 0 auto',
      minWidth: 0, overflow: 'hidden',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <div style={{ width: 22, height: 22, borderRadius: 6, background: 'var(--mb-accent)', display: 'flex', alignItems: 'center', justifyContent: 'center', flex: '0 0 auto' }}>
          <MBIcon name="waveform" weight="bold" size={13} style={{ color: 'var(--mb-accent-ink)' }} />
        </div>
        {!compact && <span style={{ fontSize: 13.5, fontWeight: 700, letterSpacing: '-0.02em', color: 'var(--mb-text)' }}>MidiToolkit</span>}
      </div>

      <div style={{ width: 1, height: 20, background: 'var(--mb-line)' }} />

      <div style={{ display: 'flex', alignItems: 'center', gap: 2 }}>
        <Btn icon={playing ? 'pause' : 'play'} iconWeight="fill" variant={playing ? 'accent' : 'soft'} active={playing} onClick={onPlay} title={playing ? 'Pause' : 'Play (synced to DAW)'} />
        <Btn icon="stop" iconWeight="fill" variant="ghost" onClick={onStop} title="Stop" />
      </div>

      {!compact && <button type="button" onClick={onToggleSync} title="Toggle DAW transport sync" style={{
        display: 'flex', alignItems: 'center', gap: 7, height: 28, padding: '0 11px', borderRadius: 7, cursor: 'pointer',
        border: '1px solid var(--mb-line)', background: synced ? 'var(--mb-accent-soft)' : 'var(--mb-elev)', font: 'inherit',
      }}>
        <span style={{ width: 7, height: 7, borderRadius: 999, background: synced ? 'var(--mb-accent)' : 'var(--mb-text3)', boxShadow: synced && playing ? '0 0 0 3px var(--mb-accent-soft)' : 'none' }} />
        <span style={{ fontSize: 11.5, fontWeight: 600, color: synced ? 'var(--mb-accent)' : 'var(--mb-text3)' }}>{synced ? 'Synced to DAW' : 'Free-run'}</span>
      </button>}

      {!compact && <div style={{ display: 'flex', alignItems: 'baseline', gap: 4, marginLeft: 2 }}>
        <span style={{ fontFamily: 'var(--mb-mono)', fontSize: 15, fontWeight: 600, color: 'var(--mb-text)', fontVariantNumeric: 'tabular-nums' }}>{bpm.toFixed(1)}</span>
        <span style={{ fontSize: 10, color: 'var(--mb-text3)', letterSpacing: '0.06em', fontWeight: 600 }}>BPM</span>
      </div>}

      <div style={{ flex: 1 }} />

      {!compact && <span style={{ fontSize: 12, color: 'var(--mb-text3)', fontFamily: 'var(--mb-mono)', maxWidth: 240, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{folder.path}</span>}
      {onToggleEditor && <Btn icon={editorFolded ? 'arrows-out-line-horizontal' : 'arrows-in-line-horizontal'} iconWeight="bold" variant="accent" active={!editorFolded} onClick={onToggleEditor} title={editorFolded ? 'Open editor' : 'Fold editor — browse only'}>Editor</Btn>}
    </header>
  );
}

// ── Favorites sidebar (saved directories) ─────────────────────────────────────
function Sidebar({ collapsed, onToggle, dirs, folders, activeId, onPick, onAdd, onRemove }) {
  if (collapsed) {
    return (
      <nav style={{ width: 48, flex: '0 0 auto', borderRight: '1px solid var(--mb-line)', background: 'var(--mb-panel)', display: 'flex', flexDirection: 'column', alignItems: 'center', padding: '8px 0', gap: 4 }}>
        <Btn icon="sidebar-simple" variant="ghost" onClick={onToggle} title="Show saved folders" />
        <div style={{ width: 24, height: 1, background: 'var(--mb-line)', margin: '4px 0' }} />
        {dirs.map((d) => {
          const active = d.id === activeId;
          return (
            <button key={d.id} type="button" onClick={() => onPick(d.id)} title={d.label} style={{
              width: 34, height: 34, borderRadius: 8, border: '1px solid ' + (active ? 'var(--mb-accent-line)' : 'transparent'),
              background: active ? 'var(--mb-accent-soft)' : 'transparent', color: active ? 'var(--mb-accent)' : 'var(--mb-text2)',
              cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center',
            }}><MBIcon name={active ? 'folder-open' : 'folder'} weight={active ? 'fill' : 'regular'} size={16} /></button>
          );
        })}
      </nav>
    );
  }
  return (
    <nav style={{ width: 196, flex: '0 0 auto', borderRight: '1px solid var(--mb-line)', background: 'var(--mb-panel)', display: 'flex', flexDirection: 'column' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 6, padding: '10px 8px 8px 14px' }}>
        <MBIcon name="push-pin" weight="fill" size={11} style={{ color: 'var(--mb-text3)' }} />
        <span style={{ flex: 1, fontSize: 10.5, fontWeight: 700, letterSpacing: '0.06em', textTransform: 'uppercase', color: 'var(--mb-text3)' }}>Saved folders</span>
        <Btn icon="sidebar-simple" variant="ghost" onClick={onToggle} title="Collapse" />
      </div>
      <div style={{ display: 'flex', flexDirection: 'column', gap: 1, padding: '0 6px', overflowY: 'auto', flex: 1 }}>
        {dirs.map((d) => {
          const active = d.id === activeId;
          const folder = folders.find((f) => f.id === d.id);
          return (
            <div key={d.id} className="mb-fav" onClick={() => onPick(d.id)} style={{
              display: 'flex', alignItems: 'center', gap: 9, padding: '8px 8px', borderRadius: 8, cursor: 'pointer',
              background: active ? 'var(--mb-accent-soft)' : 'transparent', position: 'relative',
            }}>
              <MBIcon name={active ? 'folder-open' : 'folder'} weight={active ? 'fill' : 'regular'} size={16} style={{ color: active ? 'var(--mb-accent)' : 'var(--mb-text3)', flex: '0 0 auto' }} />
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontSize: 12.5, fontWeight: active ? 600 : 500, color: active ? 'var(--mb-text)' : 'var(--mb-text2)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{d.label}</div>
                <div style={{ fontSize: 10.5, color: 'var(--mb-text3)', fontFamily: 'var(--mb-mono)' }}>{folder ? folder.files.length : 0} files</div>
              </div>
              <button type="button" className="mb-fav-x" onClick={(e) => { e.stopPropagation(); onRemove(d.id); }} title="Remove from saved"
                style={{ width: 20, height: 20, borderRadius: 6, border: 0, background: 'transparent', color: 'var(--mb-text3)', cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', opacity: 0, flex: '0 0 auto' }}>
                <MBIcon name="x" weight="bold" size={10} />
              </button>
            </div>
          );
        })}
      </div>
      <div style={{ padding: 10, borderTop: '1px solid var(--mb-line)' }}>
        <Btn icon="folder-plus" variant="soft" onClick={onAdd} style={{ width: '100%' }}>Add current folder</Btn>
      </div>
    </nav>
  );
}

// ── File list ─────────────────────────────────────────────────────────────────
function FileList({ folder, files, selectedId, onSelect, edits, playing, playingId, density, onPreviewPlay }) {
  const rowH = density === 'comfortable' ? 38 : 32;
  const { editIsClean } = window.MB_DATA;
  const scrollRef = React.useRef(null);
  const selRef = React.useRef(null);
  // keep the selected row in view when navigating with the keyboard (no scrollIntoView)
  React.useEffect(() => {
    const c = scrollRef.current, el = selRef.current;
    if (!c || !el) return;
    const top = el.offsetTop, bot = top + el.offsetHeight;
    if (top < c.scrollTop) c.scrollTop = top - 4;
    else if (bot > c.scrollTop + c.clientHeight) c.scrollTop = bot - c.clientHeight + 4;
  }, [selectedId]);
  return (
    <section style={{ flex: 1, minWidth: 0, minHeight: 0, display: 'flex', flexDirection: 'column', background: 'var(--mb-panel2)' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '0 12px', height: 36, borderBottom: '1px solid var(--mb-line)', flex: '0 0 auto' }}>
        <MBIcon name="folder-open" weight="fill" size={14} style={{ color: 'var(--mb-text2)' }} />
        <span style={{ fontSize: 12.5, fontWeight: 600, color: 'var(--mb-text)' }}>{folder.name}</span>
        <span style={{ fontSize: 11, color: 'var(--mb-text3)', fontFamily: 'var(--mb-mono)' }}>{files.length}</span>
        <div style={{ flex: 1 }} />
      </div>
      <div ref={scrollRef} style={{ flex: 1, overflowY: 'auto', position: 'relative', padding: '4px 6px' }}>
        {files.map((f) => {
          const sel = f.id === selectedId;
          const edited = !editIsClean(edits[f.id]);
          const isPlaying = playing && playingId === f.id;
          return (
            <div key={f.id} ref={sel ? selRef : null} className="mb-row" onClick={() => onSelect(f.id)} style={{
              display: 'flex', alignItems: 'center', gap: 9, height: rowH, padding: '0 8px', borderRadius: 7, cursor: 'pointer',
              background: sel ? 'var(--mb-accent)' : 'transparent', color: sel ? 'var(--mb-accent-ink)' : 'var(--mb-text)',
            }}>
              <button type="button" className="mb-row-play" onClick={(e) => { e.stopPropagation(); onPreviewPlay(f.id); }} title="Preview"
                style={{ width: 20, height: 20, flex: '0 0 auto', borderRadius: 5, border: 0, cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center',
                  background: sel ? 'rgba(0,0,0,.14)' : 'transparent', color: sel ? 'var(--mb-accent-ink)' : 'var(--mb-text3)' }}>
                <MBIcon name={isPlaying ? 'pause' : 'play'} weight="fill" size={10} />
              </button>
              <MBIcon name="file" size={14} style={{ color: sel ? 'var(--mb-accent-ink)' : 'var(--mb-text3)', flex: '0 0 auto', opacity: sel ? 0.85 : 1 }} />
              <span style={{ flex: 1, minWidth: 0, fontSize: 12.5, fontWeight: sel ? 600 : 450, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{f.name}</span>
              {edited && <span title="Edited (non-destructive)" style={{ display: 'inline-flex', alignItems: 'center', gap: 4, flex: '0 0 auto' }}>
                <span style={{ width: 6, height: 6, borderRadius: 999, background: sel ? 'var(--mb-accent-ink)' : 'var(--mb-accent)' }} />
              </span>}
              {isPlaying && <span style={{ display: 'flex', gap: 1.5, alignItems: 'flex-end', height: 12, flex: '0 0 auto' }}>
                {[0, 1, 2].map((i) => <span key={i} className="mb-eqbar" style={{ width: 2, background: sel ? 'var(--mb-accent-ink)' : 'var(--mb-accent)', animationDelay: `${i * 0.18}s` }} />)}
              </span>}
            </div>
          );
        })}
      </div>
    </section>
  );
}

Object.assign(window, { Transport, Sidebar, FileList });
