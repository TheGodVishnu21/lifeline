// components/AnalyticsPanel.jsx — the Phase 5 ALGORITHM LAB.
// Runs Dijkstra vs A* vs Bellman-Ford on the current route pair, and
// Floyd-Warshall city-wide analytics. Three shortest-path strategies,
// one optimal answer — the differences are in the WORK.

const fmtUs = (us) =>
  us >= 1000 ? (us / 1000).toFixed(2) + ' ms' : Math.round(us) + ' µs';

export default function AnalyticsPanel({
  compare, stats, busyCompare, busyStats, onCompare, onStats,
}) {
  return (
    <>
      <div className="divider" />
      <div className="section-tag" style={{ color: '#7dd3fc' }}>
        ALGORITHM LAB
      </div>
      <p className="sub">
        Race all three shortest-path algorithms on the FROM → TO pair
        above, or run Floyd-Warshall over every pair at once.
      </p>
      <div className="row2">
        <button className="btn-ghost" disabled={busyCompare} onClick={onCompare}>
          ⚗ Race 3 algorithms
        </button>
        <button className="btn-ghost" disabled={busyStats} onClick={onStats}>
          🌐 City stats (FW)
        </button>
      </div>

      {compare && (
        <section id="compareOut" style={{ display: 'flex', flexDirection: 'column', gap: 5 }}>
          <div className="resil-head">
            {compare.from_name} → {compare.to_name}
          </div>
          {[
            ['Dijkstra', compare.dijkstra],
            ['A*', compare.astar],
            ['Bellman-Ford', compare.bellman_ford],
          ].map(([name, r]) => (
            <div key={name} className="cut-item" style={{ borderLeftColor: '#7dd3fc' }}>
              <span className="r">
                {name}{' '}
                <small style={{ color: 'var(--muted)' }}>
                  {r.effort} {r.effort_name}
                </small>
              </span>
              <span className="c" style={{ color: '#7dd3fc' }}>
                {r.found ? r.distance_km.toFixed(2) + ' km' : '—'} · {fmtUs(r.runtime_us)}
              </span>
            </div>
          ))}
          <p className="race-note">
            {compare.all_distances_equal
              ? '✓ Three different strategies, one optimal distance — greedy settling (Dijkstra), goal-directed search (A*), and pure edge relaxation (Bellman-Ford) all agree.'
              : '⚠ distances differ — this should never happen!'}
          </p>
        </section>
      )}

      {stats && (
        <section id="statsOut" style={{ display: 'flex', flexDirection: 'column', gap: 5 }}>
          <div className="resil-head">
            Floyd-Warshall: {stats.fw_operations.toLocaleString()} ops in{' '}
            {fmtUs(stats.fw_runtime_us)}
          </div>
          <div className="cut-item" style={{ borderLeftColor: '#7dd3fc' }}>
            <span className="r">City diameter</span>
            <span className="c" style={{ color: '#7dd3fc' }}>
              {stats.diameter_km.toFixed(2)} km
            </span>
          </div>
          <p className="race-note" style={{ margin: 0 }}>
            longest unavoidable trip: {stats.diameter_from} ↔ {stats.diameter_to}
          </p>
          <div className="cut-item" style={{ borderLeftColor: '#7dd3fc' }}>
            <span className="r">Average trip</span>
            <span className="c" style={{ color: '#7dd3fc' }}>
              {stats.avg_distance_km.toFixed(2)} km
            </span>
          </div>
          <div className="cut-item" style={{ borderLeftColor: '#7dd3fc' }}>
            <span className="r">Most central: {stats.most_central}</span>
            <span className="c" style={{ color: '#7dd3fc' }}>
              ecc {stats.central_ecc_km.toFixed(2)} km
            </span>
          </div>
          {stats.disconnected_pairs > 0 && (
            <div className="warn-line">
              ⚠ {stats.disconnected_pairs} location pairs currently
              unreachable (disaster)
            </div>
          )}
        </section>
      )}
    </>
  );
}
