import { BrowserRouter, Routes, Route } from 'react-router-dom'
import { Toaster } from 'sonner'
import { ApiUrlProvider } from './components/apiUrl/ApiUrlProvider'
import Layout from './components/Layout'
import Home from './pages/Home'
import Schedules from './pages/Schedules'
import Updates from './pages/Updates'
import ModifyPreset from './pages/ModifyPreset'
import ModifyProviders from './pages/ModifyProviders'
import ModifyShaderProviders from './pages/ModifyShaderProviders'

export default function App() {
  return (
    <ApiUrlProvider>
      <BrowserRouter basename="/web">
        <Layout>
          <Routes>
            <Route path="/" element={<Home />} />
            <Route path="/schedules" element={<Schedules />} />
            <Route path="/updates" element={<Updates />} />
            <Route path="/modify-preset/:preset_id" element={<ModifyPreset />} />
            <Route path="/modify-providers/:preset_id/:scene_id" element={<ModifyProviders />} />
            <Route path="/modify-shader-providers/:preset_id/:scene_id" element={<ModifyShaderProviders />} />
          </Routes>
        </Layout>
      </BrowserRouter>
      <Toaster richColors position="top-right" />
    </ApiUrlProvider>
  )
}
