// components/RoutePanel.jsx — route controls, result card, and the
// Dijkstra-vs-A* "search effort" race.
import { useEffect, useRef, useState } from 'react';

const fmtUs = (us) =>
  us >= 1000 ? (us / 1000).toFixed(2) + ' ms' : Math.round(us) + ' µs';

export default function RoutePanel({
  nodes, from, to, algo, busy,
  onFrom, onTo, onAlgo, onSwap, onGo, result,
}) {
  const sorted = [...nodes].sort((a, b) => a.name.localeCompare(b.name));
  const single = result && result.single;
  const cmp = result && result.compare;

  // animate the race bars after render
  const [widths, setWidths] = useState([0, 0]);
  const raf = useRef(null);
  useEffect(() => {
    if (!cmp) return;
    const max = Math.max(cmp.d.nodes_explored, cmp.a.nodes_explored, 1);
    setWidths([0, 0]);
    raf.current = requestAnimationFrame(() =>
      setWidths([
        (cmp.d.nodes_explored / max) * 100,
        (cmp.a.nodes_explored / max) * 100,
      ])
    );
    return () => cancelAnimationFrame(raf.current);
  }, [result]);

  const shown = single || (cmp && cmp.a);
  const saved = cmp ? cmp.d.nodes_explored - cmp.a.nodes_explored : 0;

  return (
    <>
      <div>
        <label htmlFor="from">FROM</label>
        <select id="from" value={from} onChange={(e) => onFrom(+e.target.value)}>
          {sorted.map((n) => (
            <option key={n.id} value={n.id}>{n.name}</option>
          ))}
        </select>
      </div>
      <button id="swap" title="Swap from and to" onClick={onSwap}>⇅ swap</button>
      <div>
        <label htmlFor="to">TO</label>
        <select id="to" value={to} onChange={(e) => onTo(+e.target.value)}>
          {sorted.map((n) => (
            <option key={n.id} value={n.id}>{n.name}</option>
          ))}
        </select>
      </div>

      <div>
        <label>ALGORITHM</label>
        <div className="seg">
          {['dijkstra', 'astar', 'compare'].map((a) => (
            <button
              key={a}
              className={algo === a ? 'active' : ''}
              onClick={() => onAlgo(a)}
            >
              {a === 'dijkstra' ? 'Dijkstra' : a === 'astar' ? 'A*' : 'Compare'}
            </button>
          ))}
        </div>
      </div>

      <button id="go" disabled={busy} onClick={onGo}>Find route</button>

      {shown && (
        <section id="result" style={{ display: 'flex' }}>
          <div className="stats">
            <div className="stat">
              <div className="k">DISTANCE</div>
              <div className="v">{shown.distance_km.toFixed(2)} <small>km</small></div>
            </div>
            <div className="stat">
              <div className="k">ETA @30 KM/H</div>
              <div className="v">{Math.round(shown.eta_min)} <small>min</small></div>
            </div>
            <div className="stat">
              <div className="k">NODES EXPLORED</div>
              <div className="v">
                {cmp
                  ? `${cmp.a.nodes_explored} vs ${cmp.d.nodes_explored}`
                  : shown.nodes_explored}
              </div>
            </div>
            <div className="stat">
              <div className="k">RUNTIME</div>
              <div className="v">{fmtUs(shown.runtime_us)}</div>
            </div>
          </div>
          <div id="pathline">
            {shown.path_names.map((n, i) => (
              <span key={i}>
                <b>{n}</b>{i + 1 < shown.path_names.length ? ' → ' : ''}
              </span>
            ))}
          </div>
        </section>
      )}

      {cmp && (
        <section id="race" style={{ display: 'flex' }}>
          <div className="race-title">SEARCH EFFORT — SAME OPTIMAL PATH</div>
          <div className="race-row">
            <div className="race-label">
              <span className="n">Dijkstra</span>
              <span className="c">
                {cmp.d.nodes_explored} nodes · {fmtUs(cmp.d.runtime_us)}
              </span>
            </div>
            <div className="bar dijkstra">
              <i style={{ width: widths[0] + '%' }} />
            </div>
          </div>
          <div className="race-row">
            <div className="race-label">
              <span className="n">A* (haversine h)</span>
              <span className="c">
                {cmp.a.nodes_explored} nodes · {fmtUs(cmp.a.runtime_us)}
              </span>
            </div>
            <div className="bar astar">
              <i style={{ width: widths[1] + '%' }} />
            </div>
          </div>
          <p className="race-note">
            {saved > 0 ? (
              <>Same {cmp.a.distance_km.toFixed(2)} km path — the haversine
              heuristic let A* skip <b>{saved} nodes</b> that Dijkstra had
              to settle.</>
            ) : (
              <>Both settled the same nodes here — try a longer route to
              see A* pull ahead.</>
            )}
          </p>
        </section>
      )}
    </>
  );
}
