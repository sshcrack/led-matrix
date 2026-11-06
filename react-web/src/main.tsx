import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App.tsx'
import './index.css'
import { installFetchAuth } from './lib/apiToken'
import { applyColorScheme } from './core/colorScheme'

installFetchAuth()
applyColorScheme()

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
