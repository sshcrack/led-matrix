import { defineConfig } from 'vitest/config'
import react from '@vitejs/plugin-react'
import { VitePWA } from 'vite-plugin-pwa'
import path from 'path'
import fs from 'fs'
import { fileURLToPath } from 'url'

const __filename = fileURLToPath(import.meta.url)
const __dirname = path.dirname(__filename)

function pwaAssetCheckPlugin(): import('vite').Plugin {
  const requiredAssets = ['icon-192.png', 'icon-512.png', 'screenshot-wide.png', 'screenshot-mobile.png']
  return {
    name: 'pwa-asset-check',
    buildStart() {
      for (const asset of requiredAssets) {
        const fullPath = path.resolve(__dirname, 'public', asset)
        if (!fs.existsSync(fullPath)) {
          console.warn(`[PWA] Missing asset: ${asset} — expected at ${fullPath}`)
        }
      }
    },
  }
}

export default defineConfig({
  plugins: [
    pwaAssetCheckPlugin(),
    react(),
    VitePWA({
      registerType: 'autoUpdate',
      includeAssets: ['favicon.svg'],
      manifest: {
        name: 'LED Matrix Controller',
        short_name: 'LED Matrix',
        description: 'Control your LED matrix display',
        theme_color: '#6366f1',
        background_color: '#f8fafc',
        display: 'standalone',
        icons: [
          { src: 'icon-192.png', sizes: '192x192', type: 'image/png' },
          { src: 'icon-512.png', sizes: '512x512', type: 'image/png' }
        ],
        screenshots: [
          {
            src: 'screenshot-wide.png',
            sizes: '1280x720',
            type: 'image/png',
            form_factor: 'wide'
          },
          {
            src: 'screenshot-mobile.png',
            sizes: '390x844',
            type: 'image/png'
          }
        ]
      },
      workbox: {
        globPatterns: ['**/*.{js,css,html,ico,png,svg}'],
        runtimeCaching: [
          {
            urlPattern: /^https?.*\/api\/.*/i,
            handler: 'NetworkFirst',
            options: {
              cacheName: 'api-cache',
              expiration: { maxEntries: 50, maxAgeSeconds: 60 }
            }
          }
        ]
      }
    })
  ],
  resolve: {
    alias: {
      '~': path.resolve(__dirname, './src'),
    },
    // Plugin UI modules live outside react-web/ (plugins/<Name>/web); make them share our React.
    dedupe: ['react', 'react-dom', 'react-router-dom', 'lucide-react'],
  },
  server: {
    fs: { allow: ['..'] },
    // The dev UI talks to `/__api` (apiUrlContext.ts), proxied to the daemon: LED_MATRIX_DEV_API, default the
    // local emulator. Same-origin, so the daemon's cross-origin rejection never applies (incl. /live_frame WebSocket).
    proxy: {
      '/__api': { target: process.env.LED_MATRIX_DEV_API ?? 'http://localhost:8080', changeOrigin: false, ws: true, rewrite: (p: string) => p.replace(/^\/__api/, '') },
    },
  },
  test: {
    environment: 'jsdom',
    include: ['src/**/*.test.{ts,tsx}'],
  },
  base: '/web/',
  build: {
    outDir: 'dist'
  }
})
