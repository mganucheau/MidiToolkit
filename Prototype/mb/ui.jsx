// ui.jsx — MidiToolkit control atoms. All visuals via CSS vars (see app theme).

function Icon({ name, weight = '', size = 16, style = {}, className = '' }) {
  const w = weight ? `-${weight}` : '';
  return <i className={`ph${w} ph-${name} ${className}`} style={{ fontSize: size, lineHeight: 1, ...style }} aria-hidden="true" />;
}

// Generic button. variant: 'solid' | 'ghost' | 'soft' | 'accent'
function Btn({ children, onClick, variant = 'ghost', active = false, title, disabled, mono = false, style = {}, icon, iconWeight }) {
  const base = {
    display: 'inline-flex', alignItems: 'center', justifyContent: 'center', gap: 6,
    height: 28, padding: icon && !children ? 0 : '0 11px', width: icon && !children ? 28 : undefined,
    borderRadius: 7, border: '1px solid transparent', cursor: disabled ? 'default' : 'pointer',
    font: 'inherit', fontSize: 12.5, fontWeight: 500, letterSpacing: '-0.01em',
    fontFamily: mono ? 'var(--mb-mono)' : 'inherit',
    whiteSpace: 'nowrap', userSelect: 'none', transition: 'background .13s, border-color .13s, color .13s, transform .06s',
    opacity: disabled ? 0.4 : 1,
  };
  const variants = {
    ghost: { background: active ? 'var(--mb-elev)' : 'transparent', color: active ? 'var(--mb-text)' : 'var(--mb-text2)', borderColor: active ? 'var(--mb-line)' : 'transparent' },
    soft: { background: 'var(--mb-elev)', color: 'var(--mb-text)', borderColor: 'var(--mb-line)' },
    solid: { background: 'var(--mb-text)', color: 'var(--mb-bg)' },
    accent: { background: active ? 'var(--mb-accent)' : 'var(--mb-accent-soft)', color: active ? 'var(--mb-accent-ink)' : 'var(--mb-accent)', borderColor: active ? 'transparent' : 'var(--mb-accent-line)' },
  };
  return (
    <button type="button" title={title} disabled={disabled} onClick={onClick}
      onMouseDown={(e) => { if (!disabled) e.currentTarget.style.transform = 'translateY(0.5px)'; }}
      onMouseUp={(e) => { e.currentTarget.style.transform = ''; }}
      onMouseLeave={(e) => { e.currentTarget.style.transform = ''; }}
      className="mb-btn"
      style={{ ...base, ...variants[variant] }}>
      {icon && <Icon name={icon} weight={iconWeight} size={children ? 14 : 16} />}
      {children}
    </button>
  );
}

// Small status / edit badge.
function Badge({ children, tone = 'edit', onClear, title }) {
  const tones = {
    edit: { bg: 'var(--mb-accent-soft)', fg: 'var(--mb-accent)', bd: 'var(--mb-accent-line)' },
    mute: { bg: 'var(--mb-elev)', fg: 'var(--mb-text3)', bd: 'var(--mb-line)' },
  };
  const c = tones[tone];
  return (
    <span title={title} style={{
      display: 'inline-flex', alignItems: 'center', gap: 5, height: 20, padding: onClear ? '0 4px 0 8px' : '0 8px',
      borderRadius: 999, background: c.bg, color: c.fg, border: `1px solid ${c.bd}`,
      fontSize: 11, fontWeight: 600, fontFamily: 'var(--mb-mono)', letterSpacing: '-0.01em', whiteSpace: 'nowrap',
    }}>
      {children}
      {onClear && (
        <button type="button" onClick={onClear} title="Revert this" style={{
          display: 'inline-flex', alignItems: 'center', justifyContent: 'center', width: 14, height: 14,
          borderRadius: 999, border: 0, background: 'transparent', color: 'inherit', cursor: 'pointer', opacity: 0.7, padding: 0,
        }}><Icon name="x" weight="bold" size={9} /></button>
      )}
    </span>
  );
}

// Stepper: − value + with a label.
function Stepper({ label, value, display, min = -99, max = 99, onChange }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
      {label && <span style={{ fontSize: 10.5, fontWeight: 600, letterSpacing: '0.04em', textTransform: 'uppercase', color: 'var(--mb-text3)' }}>{label}</span>}
      <div style={{ display: 'flex', alignItems: 'center', height: 28, borderRadius: 7, border: '1px solid var(--mb-line)', background: 'var(--mb-elev)', overflow: 'hidden' }}>
        <button type="button" onClick={() => onChange(Math.max(min, value - 1))} disabled={value <= min}
          style={stepBtn(value <= min)}><Icon name="minus" weight="bold" size={11} /></button>
        <div style={{ minWidth: 44, textAlign: 'center', fontFamily: 'var(--mb-mono)', fontSize: 12.5, fontWeight: 600, color: 'var(--mb-text)', fontVariantNumeric: 'tabular-nums' }}>{display != null ? display : value}</div>
        <button type="button" onClick={() => onChange(Math.min(max, value + 1))} disabled={value >= max}
          style={stepBtn(value >= max)}><Icon name="plus" weight="bold" size={11} /></button>
      </div>
    </div>
  );
}
function stepBtn(dim) {
  return { width: 26, height: 26, border: 0, background: 'transparent', color: dim ? 'var(--mb-text3)' : 'var(--mb-text2)', cursor: dim ? 'default' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', opacity: dim ? 0.4 : 1 };
}

// Compact labeled native select styled to the theme.
function Picker({ label, value, options, onChange, w }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
      {label && <span style={{ fontSize: 10.5, fontWeight: 600, letterSpacing: '0.04em', textTransform: 'uppercase', color: 'var(--mb-text3)' }}>{label}</span>}
      <div style={{ position: 'relative', width: w }}>
        <select value={value} onChange={(e) => onChange(e.target.value)} className="mb-select"
          style={{
            appearance: 'none', WebkitAppearance: 'none', width: w || '100%', height: 28, padding: '0 26px 0 10px',
            borderRadius: 7, border: '1px solid var(--mb-line)', background: 'var(--mb-elev)', color: 'var(--mb-text)',
            font: 'inherit', fontSize: 12.5, fontWeight: 500, fontFamily: 'var(--mb-mono)', cursor: 'pointer', outline: 'none',
          }}>
          {options.map((o) => {
            const v = typeof o === 'object' ? o.value : o;
            const l = typeof o === 'object' ? o.label : o;
            return <option key={v} value={v}>{l}</option>;
          })}
        </select>
        <Icon name="caret-down" weight="bold" size={10} style={{ position: 'absolute', right: 9, top: '50%', transform: 'translateY(-50%)', color: 'var(--mb-text3)', pointerEvents: 'none' }} />
      </div>
    </div>
  );
}

// Toggle pill switch.
function Switch({ value, onChange }) {
  return (
    <button type="button" role="switch" aria-checked={!!value} onClick={() => onChange(!value)}
      style={{
        position: 'relative', width: 36, height: 20, borderRadius: 999, border: 0, padding: 0, cursor: 'pointer',
        background: value ? 'var(--mb-accent)' : 'var(--mb-line-strong)', transition: 'background .15s', flex: '0 0 auto',
      }}>
      <span style={{ position: 'absolute', top: 2, left: value ? 18 : 2, width: 16, height: 16, borderRadius: 999, background: '#fff', boxShadow: '0 1px 3px rgba(0,0,0,.4)', transition: 'left .15s' }} />
    </button>
  );
}

Object.assign(window, { MBIcon: Icon, Btn, Badge, Stepper, Picker, Switch });
