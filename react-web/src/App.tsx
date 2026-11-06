import { lazy, Suspense } from 'react'
import { BrowserRouter, Routes, Route } from 'react-router-dom'
import { Toaster } from 'sonner'
import { ApiUrlProvider } from './components/apiUrl/ApiUrlProvider'
import { MatrixProvider } from './core/MatrixProvider'
import Shell from './shell/Shell'
import ErrorBoundary from './components/ErrorBoundary'
import ApiTokenDialog from './components/apiUrl/ApiTokenDialog'

const Schedules = lazy(() => import('./pages/Schedules'))
const Updates = lazy(() => import('./pages/Updates'))
const ModifyProviders = lazy(() => import('./pages/ModifyProviders'))
const ModifyShaderProviders = lazy(() => import('./pages/ModifyShaderProviders'))
const SceneGallery = lazy(() => import('./pages/SceneGallery'))
const SceneLab = lazy(() => import('./pages/SceneLab'))
const AssetManager = lazy(() => import('./pages/AssetManager'))
const Diagnostics = lazy(() => import('./pages/Diagnostics'))

const Home = lazy(() => import('./shell/Home'))
const Editor = lazy(() => import('./shell/Editor'))

function Routed() {
  return (
      <Shell>
            <ErrorBoundary>
              <Suspense fallback={<div style={{ padding: '2rem', textAlign: 'center', color: '#888' }}>Loading...</div>}>
                <Routes>
                  <Route path="/" element={<Home />} />
                  <Route path="/gallery" element={<SceneGallery />} />
                  <Route path="/scene-lab" element={<SceneLab />} />
                  <Route path="/assets" element={<AssetManager />} />
                  <Route path="/schedules" element={<Schedules />} />
                  <Route path="/updates" element={<Updates />} />
                  <Route path="/diagnostics" element={<Diagnostics />} />
                  <Route path="/modify-preset/:preset_id" element={<Editor />} />
                  <Route path="/modify-providers/:preset_id/:scene_id" element={<ModifyProviders />} />
                  <Route path="/modify-shader-providers/:preset_id/:scene_id" element={<ModifyShaderProviders />} />
                </Routes>
              </Suspense>
            </ErrorBoundary>
      </Shell>
  )
}

export default function App() {
  return (
    <ApiUrlProvider>
      <ApiTokenDialog />
      <BrowserRouter basename="/web">
        <MatrixProvider>
          <Routed />
        </MatrixProvider>
      </BrowserRouter>
      <Toaster richColors position="bottom-center" offset={20} />
    </ApiUrlProvider>
  )
}
