import { definePluginUi } from '~/plugin-sdk'

/**
 * Web UI extension for AmbientScenes. Discovered automatically by react-web
 * (plugins/<Name>/web/index.ts) — no registration step and no CMake change.
 * Only scenes that exist on the connected matrix are used, so this stays safe
 * if a scene is renamed or the plugin is not installed.
 */
export default definePluginUi({
  plugin: 'AmbientScenes',
  presetTemplates: [
    { id: 'night-sky', name: 'Night sky', description: 'Slow starfield with a drifting blob', scenes: ['starfield', 'metablob'], variants: { starfield: 'calm' } },
    { id: 'digital-calm', name: 'Digital calm', description: 'Matrix rain and a neon tunnel', scenes: ['digitalrain', 'neontunnel'] },
    { id: 'desk-clock', name: 'Desk clock', description: 'Clock, with a quiet starfield between', scenes: ['clock', 'starfield'] },
  ],
})
