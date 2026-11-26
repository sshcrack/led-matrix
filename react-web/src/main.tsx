import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App.tsx'
import './index.css'
import { installFetchAuth } from './lib/apiToken'
import { applyColorScheme } from './core/colorScheme'
import { registerSW } from 'virtual:pwa-register'
import { installChunkErrorReload, startUiUpdates } from './core/uiUpdate'

installFetchAuth()
applyColorScheme()
installChunkErrorReload()
registerSW({ immediate: true, onRegisteredSW: (_url, reg) => startUiUpdates(reg) })

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
