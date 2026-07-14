// src/api.js — every backend call in one place (docs/api_spec.md is
// the contract). Same-origin in production (the C++ server serves this
// build); the Vite dev server proxies /api to :8080.

async function request(path, options = {}) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), 5000);
  try {
    const res = await fetch(path, { ...options, signal: ctrl.signal });
    if (!res.ok) {
      const body = await res.json().catch(() => ({}));
      throw new Error(body.error || `HTTP ${res.status}`);
    }
    return await res.json();
  } finally {
    clearTimeout(t);
  }
}

export const apiGet = (path) => request(path);

export const apiPost = (path, body) =>
  request(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body ?? {}),
  });

export const api = {
  health: () => apiGet('/api/health'),
  city: () => apiGet('/api/city'),
  route: (from, to, algo) =>
    apiGet(`/api/route?from=${from}&to=${to}&algo=${algo}`),
  suggest: (q) => apiGet(`/api/suggest?q=${encodeURIComponent(q)}`),
  disaster: () => apiGet('/api/disaster'),
  triggerDisaster: (epicenter, severity) =>
    apiPost('/api/disaster', { epicenter, type: 'flood', severity }),
  reset: () => apiPost('/api/reset', {}),
  evacuation: () => apiGet('/api/evacuation'),
  dispatch: (body) => apiPost('/api/dispatch', body ?? {}),
  supply: (capacity) => apiPost('/api/supply', { capacity }),
  history: (limit = 8) => apiGet(`/api/history?limit=${limit}`),
  resilience: () => apiGet('/api/resilience'),
  mst: () => apiGet('/api/mst'),
  compare: (from, to) => apiGet(`/api/compare?from=${from}&to=${to}`),
  cityStats: () => apiGet('/api/citystats'),
};
