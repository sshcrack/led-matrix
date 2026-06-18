import { lazy, Suspense } from 'react'
import { BrowserRouter, Routes, Route } from 'react-router-dom'
import { Toaster } from 'sonner'
import { ApiUrlProvider } from './components/apiUrl/ApiUrlProvider'
import Layout from './components/Layout'
import ErrorBoundary from './components/ErrorBoundary'

const Home = lazy(() => import('./pages/Home'))
const Schedules = lazy(() => import('./pages/Schedules'))
const Updates = lazy(() => import('./pages/Updates'))
const ModifyPreset = lazy(() => import('./pages/ModifyPreset'))
const ModifyProviders = lazy(() => import('./pages/ModifyProviders'))
const ModifyShaderProviders = lazy(() => import('./pages/ModifyShaderProviders'))
const SceneGallery = lazy(() => import('./pages/SceneGallery'))
const AssetManager = lazy(() => import('./pages/AssetManager'))

export default function App() {
  return (
    <ApiUrlProvider>
      <BrowserRouter basename="/web">
          <Layout>
            <ErrorBoundary>
              <Suspense fallback={<div style={{ padding: '2rem', textAlign: 'center', color: '#888' }}>Loading...</div>}>
                <Routes>
                  <Route path="/" element={<Home />} />
                  <Route path="/gallery" element={<SceneGallery />} />
                  <Route path="/assets" element={<AssetManager />} />
                  <Route path="/schedules" element={<Schedules />} />
                  <Route path="/updates" element={<Updates />} />
                  <Route path="/modify-preset/:preset_id" element={<ModifyPreset />} />
                  <Route path="/modify-providers/:preset_id/:scene_id" element={<ModifyProviders />} />
                  <Route path="/modify-shader-providers/:preset_id/:scene_id" element={<ModifyShaderProviders />} />
                </Routes>
              </Suspense>
            </ErrorBoundary>
          </Layout>
      </BrowserRouter>
      <Toaster richColors position="top-right" />
    </ApiUrlProvider>
  )
}
