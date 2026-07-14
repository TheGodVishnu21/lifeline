// components/SearchBar.jsx — Trie prefix autocomplete (GET /api/suggest)
import { useState } from 'react';
import { api } from '../api.js';

export default function SearchBar({ onPick }) {
  const [q, setQ] = useState('');
  const [results, setResults] = useState(null);   // null = hidden

  async function onInput(value) {
    setQ(value);
    if (!value.trim()) return setResults(null);
    try {
      const s = await api.suggest(value.trim());
      setResults(s.results);
    } catch {
      setResults(null);
    }
  }

  function pick(r) {
    setQ(r.name);
    setResults(null);
    onPick(r);
  }

  return (
    <div id="searchWrap">
      <label htmlFor="q">FIND LOCATION — TRIE PREFIX SEARCH</label>
      <input
        id="q"
        value={q}
        placeholder="Type a prefix… e.g. riv"
        autoComplete="off"
        onChange={(e) => onInput(e.target.value)}
        onBlur={() => setTimeout(() => setResults(null), 180)}
      />
      {results && (
        <div id="suggestions" style={{ display: 'block' }}>
          {results.length === 0 ? (
            <div className="sugg">
              <span style={{ color: 'var(--muted)' }}>
                no matches (prefix search)
              </span>
            </div>
          ) : (
            results.map((r) => (
              <div key={r.id} className="sugg" onMouseDown={() => pick(r)}>
                <span>{r.name}</span>
                <small>{r.type.replace('_', ' ')}</small>
              </div>
            ))
          )}
        </div>
      )}
    </div>
  );
}
