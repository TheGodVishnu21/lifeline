import React from 'react';
import ReactDOM from 'react-dom/client';
import 'leaflet/dist/leaflet.css';
import './index.css';
import App from './App.jsx';

// Note: StrictMode is intentionally omitted — its dev-mode double-mount
// fights with Leaflet's imperative map initialisation. MapView guards
// against re-init anyway, but this keeps the console clean.
ReactDOM.createRoot(document.getElementById('root')).render(<App />);
