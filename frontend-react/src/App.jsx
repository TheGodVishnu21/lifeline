// App.jsx — all shared state lives here; panels are presentational.
// Data flow: user action -> api call -> setState -> MapView effects
// redraw the matching Leaflet layer. See docs/api_spec.md for shapes.
import { useEffect, useState } from 'react';
import { api } from './api.js';
import MapView from './components/MapView.jsx';
import SearchBar from './components/SearchBar.jsx';
import RoutePanel from './components/RoutePanel.jsx';
import { DisasterPanel, EvacPanel } from './components/DisasterPanel.jsx';
import ResponsePanel from './components/ResponsePanel.jsx';
import ResiliencePanel from './components/ResiliencePanel.jsx';
import AnalyticsPanel from './components/AnalyticsPanel.jsx';
import HistoryLog from './components/HistoryLog.jsx';

export default function App() {
  // connection + base data
  const [health, setHealth] = useState(null);
  const [city, setCity] = useState(null);
  const [error, setError] = useState('');

  // routing
  const [from, setFrom] = useState(15);           // Riverside Colony
  const [to, setTo] = useState(1);                // Sunrise Hospital
  const [algo, setAlgo] = useState('dijkstra');
  const [routeResult, setRouteResult] = useState(null);
  const [routeCoords, setRouteCoords] = useState(null);
  const [busyRoute, setBusyRoute] = useState(false);

  // disaster + evacuation
  const [epicenter, setEpicenter] = useState(26); // River Bridge South
  const [severity, setSeverity] = useState(1);
  const [disaster, setDisaster] = useState(null);
  const [evac, setEvac] = useState(null);

  // response / resilience / analytics / history
  const [dispatchResult, setDispatchResult] = useState(null);
  const [supply, setSupply] = useState(null);
  const [resilience, setResilience] = useState(null);
  const [mst, setMst] = useState(null);
  const [compare, setCompare] = useState(null);
  const [stats, setStats] = useState(null);
  const [history, setHistory] = useState([]);
  const [busy, setBusy] = useState({});
  const [focusNode, setFocusNode] = useState(null);

  const setBusyKey = (k, v) => setBusy((b) => ({ ...b, [k]: v }));
  const friendly = (err) =>
    err.name === 'AbortError' || `${err.message}`.includes('fetch')
      ? 'Backend offline — start it with ./lifeline inside backend/ and reload.'
      : err.message;

  // ---- boot: health, city, history, restore active disaster ---------
  useEffect(() => {
    (async () => {
      try {
        const h = await api.health();
        setHealth(h);
        setCity(await api.city());
        loadHistory();
        const ds = await api.disaster();
        if (ds.active) {
          setDisaster(ds);
          setSeverity(ds.severity);
          setEpicenter(ds.epicenter);
          loadEvac();
        }
      } catch (err) {
        setError(friendly(err));
      }
    })();
  }, []);

  async function loadHistory() {
    try { setHistory((await api.history(8)).entries); } catch { /* ok */ }
  }
  async function loadEvac() {
    try { setEvac(await api.evacuation()); } catch { setEvac(null); }
  }

  // ---- routing -------------------------------------------------------
  async function findRoute(f = from, t = to, a = algo) {
    setBusyRoute(true);
    setError('');
    try {
      if (a === 'compare') {
        const [d, aa] = await Promise.all([
          api.route(f, t, 'dijkstra'),
          api.route(f, t, 'astar'),
        ]);
        if (!d.found) throw new Error('No route found between these points.');
        setRouteResult({ compare: { d, a: aa } });
        setRouteCoords(aa.coords);
      } else {
        const r = await api.route(f, t, a);
        if (!r.found)
          throw new Error(
            'No route — roads blocked by the disaster. Try Reset.'
          );
        setRouteResult({ single: r });
        setRouteCoords(r.coords);
      }
    } catch (err) {
      setRouteResult(null);
      setRouteCoords(null);
      setError(friendly(err));
    } finally {
      setBusyRoute(false);
    }
  }

  // ---- disaster ------------------------------------------------------
  async function triggerDisaster(epi = epicenter, sev = severity) {
    setError('');
    try {
      const d = await api.triggerDisaster(epi, sev);
      setDisaster(d);
      setEpicenter(epi);
      setResilience(null);            // stale against new road state
      setDispatchResult(null);
      loadEvac();
      loadHistory();
      if (routeResult) findRoute();   // recompute the shown route
    } catch (err) {
      setError(friendly(err));
    }
  }

  async function clearDisaster() {
    try {
      await api.reset();
      setDisaster(null);
      setEvac(null);
      setDispatchResult(null);
      setResilience(null);
      if (routeResult) findRoute();
    } catch (err) {
      setError(friendly(err));
    }
  }

  function changeSeverity(s) {
    setSeverity(s);
    if (disaster) triggerDisaster(epicenter, s);  // live re-run
  }

  // ---- response ------------------------------------------------------
  async function runDispatch() {
    setBusyKey('dispatch', true);
    setError('');
    try {
      setDispatchResult(await api.dispatch());
      loadHistory();
    } catch (err) {
      setError(
        `${err.message}`.includes('disaster')
          ? 'Trigger a disaster first, then dispatch responders to the zone.'
          : friendly(err)
      );
    } finally {
      setBusyKey('dispatch', false);
    }
  }

  async function runSupply() {
    setBusyKey('supply', true);
    try { setSupply(await api.supply(50)); }
    catch (err) { setError(friendly(err)); }
    finally { setBusyKey('supply', false); }
  }

  // ---- resilience / analytics ----------------------------------------
  async function runResilience() {
    setBusyKey('resil', true);
    try { setResilience(await api.resilience()); }
    catch (err) { setError(friendly(err)); }
    finally { setBusyKey('resil', false); }
  }
  async function runMst() {
    setBusyKey('mst', true);
    try { setMst(await api.mst()); }
    catch (err) { setError(friendly(err)); }
    finally { setBusyKey('mst', false); }
  }
  async function runCompare() {
    setBusyKey('compare', true);
    try { setCompare(await api.compare(from, to)); }
    catch (err) { setError(friendly(err)); }
    finally { setBusyKey('compare', false); }
  }
  async function runStats() {
    setBusyKey('stats', true);
    try { setStats(await api.cityStats()); }
    catch (err) { setError(friendly(err)); }
    finally { setBusyKey('stats', false); }
  }

  return (
    <>
      <aside id="panel">
        <div>
          <div className="eyebrow">LIFELINE // COMMAND CENTER</div>
          <h1>Disaster Response Simulator</h1>
          <p className="sub">
            A weighted city graph under a C++ DSA engine — routing, max-flow
            evacuation, triage dispatch, and network resilience, live.
          </p>
        </div>

        <span className={'pill ' + (health ? 'on' : 'off')}>
          <span className="dot" />
          <span>
            {health
              ? `${health.city} · ${health.nodes} nodes · ${health.edges} roads · P${health.phase}`
              : 'backend offline'}
          </span>
        </span>

        {error && (
          <div id="error" style={{ display: 'block' }}>{error}</div>
        )}

        {city && (
          <>
            <SearchBar onPick={(r) => setFocusNode(r)} />

            <RoutePanel
              nodes={city.nodes}
              from={from} to={to} algo={algo} busy={busyRoute}
              onFrom={setFrom} onTo={setTo} onAlgo={setAlgo}
              onSwap={() => { setFrom(to); setTo(from); }}
              onGo={() => findRoute()}
              result={routeResult}
            />

            <DisasterPanel
              nodes={city.nodes}
              epicenter={epicenter} severity={severity} active={disaster}
              onEpicenter={setEpicenter}
              onSeverity={changeSeverity}
              onTrigger={() => triggerDisaster()}
              onClear={clearDisaster}
            />
            <EvacPanel evac={disaster ? evac : null} />

            <ResponsePanel
              dispatchResult={dispatchResult} supply={supply}
              busyDispatch={busy.dispatch} busySupply={busy.supply}
              onDispatch={runDispatch} onSupply={runSupply}
            />

            <ResiliencePanel
              resilience={resilience} mst={mst}
              disasterActive={!!disaster}
              busyResil={busy.resil} busyMst={busy.mst}
              onResil={runResilience} onMst={runMst}
            />

            <AnalyticsPanel
              compare={compare} stats={stats}
              busyCompare={busy.compare} busyStats={busy.stats}
              onCompare={runCompare} onStats={runStats}
            />

            <HistoryLog entries={history} onRefresh={loadHistory} />
          </>
        )}
      </aside>

      <MapView
        city={city}
        disaster={disaster}
        routeCoords={routeCoords}
        dispatchResult={dispatchResult}
        resilience={resilience}
        mstEdges={mst ? mst.kruskal.edges : null}
        focusNode={focusNode}
        onNodeClick={(id) => triggerDisaster(id)}
      />
    </>
  );
}
