// components/ResponsePanel.jsx — dispatch units (triage + greedy) and
// pack the relief truck (knapsack).

export default function ResponsePanel({
  dispatchResult, supply, busyDispatch, busySupply,
  onDispatch, onSupply,
}) {
  const d = dispatchResult;
  const fleet =
    d && d.assignments_by_type
      ? Object.entries(d.assignments_by_type)
          .map(([k, v]) => `${v} ${k.replace('_', ' ')}`)
          .join(' · ')
      : '';

  return (
    <>
      <div className="divider" />
      <div className="section-tag" style={{ color: 'var(--route)' }}>
        RESPONSE COORDINATION
      </div>
      <p className="sub">
        Dispatch responders to the disaster zone (triage + greedy nearest
        vehicle), or pack a relief truck (knapsack).
      </p>
      <div className="row2">
        <button className="btn-ghost" disabled={busyDispatch} onClick={onDispatch}>
          🚑 Dispatch units
        </button>
        <button className="btn-ghost" disabled={busySupply} onClick={onSupply}>
          📦 Load truck
        </button>
      </div>

      {d && (
        <section id="dispatchOut" style={{ display: 'flex' }}>
          {d.assignments.map((a) => (
            <div key={a.incident_id} className="asg">
              <span className={`sev-badge sev-${a.severity}`}>{a.severity}</span>
              <span className="to">
                {a.incident_name}
                <small>
                  {a.vehicle_type} #{a.vehicle_id} from {a.vehicle_name}
                </small>
              </span>
              <span className="km">
                {a.distance_km.toFixed(1)} km<br />
                {Math.round(a.eta_min)} min
              </span>
            </div>
          ))}
          {d.unassigned.length > 0 && (
            <div className="unassigned">
              ⚠ {d.unassigned.length} incident(s) unreachable — roads cut off
            </div>
          )}
          <div className="supply-summary">
            {d.assignments.length} units dispatched
            {fleet ? ` (${fleet})` : ''} · {d.total_response_km.toFixed(1)} km
            total
          </div>
        </section>
      )}

      {supply && (
        <section id="supplyOut" style={{ display: 'flex' }}>
          <div className="supply-summary">
            {supply.capacity} kg truck → packed {supply.total_weight} kg,
            relief value {supply.total_value}
          </div>
          {supply.chosen.map((it, i) => (
            <div key={i} className="supply-item">
              <span>{it.name}</span>
              <span className="w">{it.weight} kg · v{it.value}</span>
            </div>
          ))}
        </section>
      )}
    </>
  );
}
