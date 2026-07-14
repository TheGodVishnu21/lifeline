// components/ResiliencePanel.jsx — Tarjan critical analysis + MST
// restoration plan.

export default function ResiliencePanel({
  resilience, mst, disasterActive, busyResil, busyMst, onResil, onMst,
}) {
  const r = resilience;
  return (
    <>
      <div className="divider" />
      <div className="section-tag" style={{ color: '#b78bff' }}>
        NETWORK RESILIENCE
      </div>
      <p className="sub">
        Tarjan's DFS finds critical roads (bridges) and junctions
        (articulation points) in the current network. MST = minimum
        backbone to keep the city connected.
      </p>
      <div className="row2">
        <button className="btn-ghost" disabled={busyResil} onClick={onResil}>
          🕸 Analyze network
        </button>
        <button className="btn-ghost" disabled={busyMst} onClick={onMst}>
          🌿 Restoration plan
        </button>
      </div>

      {r && (
        <section id="resilOut" style={{ display: 'flex' }}>
          <div className="resil-head">
            {r.components} connected component{r.components > 1 ? 's' : ''}
          </div>
          {r.isolated.length > 0 && (
            <div className="warn-line">
              ⚠ isolated: {r.isolated.map((i) => i.name).join(', ')}
            </div>
          )}
          {r.cut_off_from_hospital.length > 0 && (
            <div className="warn-line">
              ⚠ no hospital reachable:{' '}
              {r.cut_off_from_hospital.map((i) => i.name).join(', ')}
            </div>
          )}
          {r.bridges.length === 0 && r.articulation_points.length === 0 ? (
            <div className="ok-line">
              ✓ No single road or junction failure can split this network —
              healthy mesh (every node has degree ≥ 3).
            </div>
          ) : (
            <>
              {r.bridges.map((b, i) => (
                <div key={i} className="crit-item">
                  <span>{b.road}</span>
                  <span className="c">bridge</span>
                </div>
              ))}
              {r.articulation_points.length > 0 && (
                <div className="chips">
                  {r.articulation_points.map((a) => (
                    <span key={a.id} className="chip">{a.name}</span>
                  ))}
                </div>
              )}
              <p className="race-note">
                Violet = single points of failure found by one Tarjan DFS
                (O(V+E)).{' '}
                {disasterActive
                  ? 'The disaster created these — the healthy mesh has none.'
                  : ''}
              </p>
            </>
          )}
        </section>
      )}

      {mst && (
        <section id="mstOut" style={{ display: 'flex' }}>
          <div className="resil-head">
            {mst.kruskal.total_km.toFixed(2)} km minimum backbone (
            {mst.kruskal.edge_count} roads)
          </div>
          <div className="ok-line">
            ✓ Prim {mst.prim.total_km.toFixed(2)} km = Kruskal{' '}
            {mst.kruskal.total_km.toFixed(2)} km — two algorithms, one
            optimum
          </div>
          <p className="race-note">
            Green = the cheapest set of roads that keeps every zone
            connected (planning view, ignores blockages). Prim{' '}
            {Math.round(mst.prim.runtime_us)}µs vs Kruskal{' '}
            {Math.round(mst.kruskal.runtime_us)}µs.
          </p>
        </section>
      )}
    </>
  );
}
