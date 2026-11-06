import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App.tsx'
import './index.css'
import { installFetchAuth } from './lib/apiToken'
import { loadPluginUis } from './core/pluginRegistry'
import { applyVariant } from './core/variants'

installFetchAuth()
void loadPluginUis()
applyVariant()

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
