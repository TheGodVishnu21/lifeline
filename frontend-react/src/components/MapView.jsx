// components/MapView.jsx
// Leaflet is imperative, React is declarative — this component is the
// bridge. The map and its layer groups are created ONCE (refs); a
// useEffect per data slice keeps each layer in sync with App state.
import { useEffect, useRef } from 'react';
import L from 'leaflet';

const TYPE_COLORS = {
  hospital: '#ff5d73', fire_station: '#ffa02e', police: '#4ea8ff',
  shelter: '#58d68d', transit: '#c39bff', infrastructure: '#9d7bff',
  bridge: '#ffd166', market: '#f2a1c0', office: '#7dd3fc',
  education: '#7dd3fc', residential: '#8fa0b8', industrial: '#b8a58f',
  junction: '#5d6b82',
};
const WAVE_COLORS = ['#ff5d73', '#ff8a4c', '#ffb020'];

const edgeKey = (u, v) => `${Math.min(u, v)}-${Math.max(u, v)}`;

export default function MapView({
  city, disaster, routeCoords, dispatchResult, resilience, mstEdges,
  focusNode, onNodeClick,
}) {
  const divRef = useRef(null);
  const mapRef = useRef(null);
  const layers = useRef({});          // disaster, mst, resil, dispatch, route
  const edgeLines = useRef({});       // "u-v" -> polyline
  const nodeMarkers = useRef({});     // id -> circleMarker
  const clickHandler = useRef(onNodeClick);
  clickHandler.current = onNodeClick; // always call the latest handler

  // ---- init once --------------------------------------------------
  useEffect(() => {
    if (mapRef.current) return;
    const map = L.map(divRef.current, { zoomControl: true })
      .setView([19.075, 72.882], 13);
    L.tileLayer(
      'https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png',
      {
        attribution:
          '&copy; <a href="https://www.openstreetmap.org/copyright">OSM</a> &copy; <a href="https://carto.com/">CARTO</a>',
        maxZoom: 19,
      }
    ).addTo(map);
    layers.current = {
      disaster: L.layerGroup().addTo(map),
      mst: L.layerGroup().addTo(map),
      resil: L.layerGroup().addTo(map),
      dispatch: L.layerGroup().addTo(map),
      route: L.layerGroup().addTo(map),
    };
    mapRef.current = map;
    return () => { map.remove(); mapRef.current = null; };
  }, []);

  // ---- base city (nodes + roads) ----------------------------------
  useEffect(() => {
    const map = mapRef.current;
    if (!map || !city) return;

    city.edges.forEach((e) => {
      const a = city.nodes[e.from], b = city.nodes[e.to];
      const line = L.polyline([[a.lat, a.lon], [b.lat, b.lon]], {
        color: '#223048', weight: 1.6, opacity: 0.85,
      }).bindTooltip(`${e.road} · ${e.length_km} km`, { sticky: true })
        .addTo(map);
      edgeLines.current[edgeKey(e.from, e.to)] = line;
    });

    city.nodes.forEach((n) => {
      const c = TYPE_COLORS[n.type] || '#8fa0b8';
      const m = L.circleMarker([n.lat, n.lon], {
        radius: ['hospital', 'fire_station', 'shelter'].includes(n.type) ? 7 : 5,
        color: c, weight: 1.5, fillColor: c, fillOpacity: 0.55,
      }).bindTooltip(`[${n.id}] ${n.name}`).addTo(map);
      m.on('click', () => clickHandler.current && clickHandler.current(n.id));
      nodeMarkers.current[n.id] = m;
    });

    map.fitBounds(city.nodes.map((n) => [n.lat, n.lon]), { padding: [30, 30] });
  }, [city]);

  // ---- disaster overlay (waves + blocked roads) --------------------
  useEffect(() => {
    if (!mapRef.current || !city) return;
    const lg = layers.current.disaster;
    lg.clearLayers();

    const blocked = new Set(
      disaster ? disaster.blocked_edges.map((b) => edgeKey(b.from, b.to)) : []
    );
    Object.entries(edgeLines.current).forEach(([key, line]) => {
      line.setStyle(blocked.has(key)
        ? { color: '#ff5d73', weight: 2.2, opacity: 0.9, dashArray: '4 5' }
        : { color: '#223048', weight: 1.6, opacity: 0.85, dashArray: null });
    });

    if (!disaster) return;
    disaster.waves.forEach((wave, ring) => {
      const color = WAVE_COLORS[Math.min(ring, WAVE_COLORS.length - 1)];
      wave.forEach((id) => {
        const n = city.nodes[id];
        L.circleMarker([n.lat, n.lon], {
          radius: 13 - ring * 2, color, weight: 1.5,
          fillColor: color, fillOpacity: ring === 0 ? 0.5 : 0.28,
        }).addTo(lg);
      });
    });
    const e = city.nodes[disaster.epicenter];
    L.circleMarker([e.lat, e.lon], {
      radius: 9, color: '#fff', weight: 2,
      fillColor: '#ff5d73', fillOpacity: 0.9,
    }).bindTooltip(`Epicenter: ${disaster.epicenter_name}`).addTo(lg);
  }, [disaster, city]);

  // ---- computed route ----------------------------------------------
  useEffect(() => {
    if (!mapRef.current) return;
    const lg = layers.current.route;
    lg.clearLayers();
    if (!routeCoords || routeCoords.length < 2) return;
    L.polyline(routeCoords, {
      color: '#2dd4bf', weight: 4.5, opacity: 0.95,
      dashArray: '10 12', className: 'route-anim',
    }).addTo(lg);
    mapRef.current.fitBounds(routeCoords, { padding: [60, 60] });
  }, [routeCoords]);

  // ---- dispatch assignments ----------------------------------------
  useEffect(() => {
    if (!mapRef.current || !city) return;
    const lg = layers.current.dispatch;
    lg.clearLayers();
    if (!dispatchResult) return;
    dispatchResult.assignments.forEach((a) => {
      if (a.coords && a.coords.length > 1)
        L.polyline(a.coords, {
          color: '#ffb020', weight: 3, opacity: 0.8, dashArray: '6 6',
        }).addTo(lg);
      const inc = city.nodes[a.incident_node];
      L.circleMarker([inc.lat, inc.lon], {
        radius: 6, color: '#ffb020', weight: 2,
        fillColor: '#ff5d73', fillOpacity: 0.8,
      }).addTo(lg);
    });
  }, [dispatchResult, city]);

  // ---- resilience overlay (bridges violet + AP rings) ---------------
  useEffect(() => {
    if (!mapRef.current) return;
    const lg = layers.current.resil;
    lg.clearLayers();
    if (!resilience) return;
    resilience.bridges.forEach((b) => {
      L.polyline(b.coords, { color: '#b78bff', weight: 5, opacity: 0.9 })
        .bindTooltip(`CRITICAL: ${b.road} — losing this splits the network`,
                     { sticky: true })
        .addTo(lg);
    });
    resilience.articulation_points.forEach((a) => {
      L.circleMarker([a.lat, a.lon], {
        radius: 11, color: '#b78bff', weight: 2.5,
        fillOpacity: 0, dashArray: '3 3',
      }).bindTooltip(`CRITICAL JUNCTION: ${a.name}`).addTo(lg);
    });
  }, [resilience]);

  // ---- MST overlay ---------------------------------------------------
  useEffect(() => {
    if (!mapRef.current) return;
    const lg = layers.current.mst;
    lg.clearLayers();
    if (!mstEdges) return;
    mstEdges.forEach((e) => {
      L.polyline(e.coords, { color: '#58d68d', weight: 3.5, opacity: 0.75 })
        .bindTooltip(`MST: ${e.road} · ${e.length_km} km`, { sticky: true })
        .addTo(lg);
    });
  }, [mstEdges]);

  // ---- fly to a searched node ----------------------------------------
  useEffect(() => {
    if (!mapRef.current || !focusNode) return;
    mapRef.current.setView([focusNode.lat, focusNode.lon], 15);
    const m = nodeMarkers.current[focusNode.id];
    if (m) m.openTooltip();
  }, [focusNode]);

  return <div id="map" ref={divRef} />;
}
