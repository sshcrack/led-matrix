import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App.tsx'
import './index.css'
import { installFetchAuth } from './lib/apiToken'
import { loadPluginUis } from './core/pluginRegistry'
import { applyColorScheme } from './core/colorScheme'

installFetchAuth()
void loadPluginUis()
applyColorScheme()

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
