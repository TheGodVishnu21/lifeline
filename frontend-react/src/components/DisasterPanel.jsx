// components/DisasterPanel.jsx — trigger controls + evacuation results.
// Two small, tightly-related panels kept in one file.

export function DisasterPanel({
  nodes, epicenter, severity, active,
  onEpicenter, onSeverity, onTrigger, onClear,
}) {
  const sorted = [...nodes].sort((a, b) => a.name.localeCompare(b.name));
  return (
    <>
      <div className="divider" />
      <div className="section-tag">DISASTER SIMULATION</div>
      <p className="sub">
        Pick a spread radius, then click a map location (or use the box
        below) to flood it. Roads inside the zone close; routing reroutes
        live.
      </p>
      <div>
        <label>SPREAD RADIUS</label>
        <div className="sev">
          {[1, 2, 3].map((s) => (
            <button
              key={s}
              className={severity === s ? 'active' : ''}
              onClick={() => onSeverity(s)}
            >
              {s} ring{s > 1 ? 's' : ''}
            </button>
          ))}
        </div>
      </div>
      <div>
        <label htmlFor="epi">EPICENTER</label>
        <select id="epi" value={epicenter}
                onChange={(e) => onEpicenter(+e.target.value)}>
          {sorted.map((n) => (
            <option key={n.id} value={n.id}>{n.name}</option>
          ))}
        </select>
      </div>
      <button
        id="dztrigger"
        className={active ? 'armed' : ''}
        onClick={active ? onClear : onTrigger}
      >
        {active
          ? `✕ Clear ${active.type} (${active.blocked_count} roads down)`
          : '⚠ Trigger disaster'}
      </button>
    </>
  );
}

export function EvacPanel({ evac }) {
  if (!evac) return null;
  const cuts = [...evac.bottlenecks].sort((a, b) => a.capacity - b.capacity);
  return (
    <section id="evac" style={{ display: 'flex' }}>
      <div className="section-tag" style={{ color: 'var(--signal)' }}>
        EVACUATION CAPACITY
      </div>
      <div className="flow-big">
        {Math.round(evac.max_flow_per_hour).toLocaleString()}
        <small>people/hour to safe shelters</small>
      </div>
      {cuts.length > 0 && (
        <>
          <div className="race-title">MIN-CUT BOTTLENECKS</div>
          <div className="cut-list">
            {cuts.map((b, i) => (
              <div key={i} className="cut-item">
                <span className="r">{b.road}</span>
                <span className="c">
                  {Math.round(b.capacity).toLocaleString()}/hr
                </span>
              </div>
            ))}
          </div>
        </>
      )}
      <p className="race-note">
        {evac.note ? (
          <>⚠ {evac.note}</>
        ) : (
          <>
            Ford–Fulkerson found this in <b>{evac.augmenting_paths}</b>{' '}
            augmenting paths. The min-cut roads above are the true
            evacuation bottleneck — widening any other road won't help.
          </>
        )}
      </p>
    </section>
  );
}
