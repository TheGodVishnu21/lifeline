import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// base './' -> built assets use relative paths, so the C++ server can
// mount dist/ at any root. Dev proxy forwards /api to the backend.
export default defineConfig({
  plugins: [react()],
  base: './',
  server: {
    proxy: { '/api': 'http://localhost:8080' }
  }
})
