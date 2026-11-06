import { describe, expect, it } from 'vitest'
import { selectPluginModules, selectRuntimeUis, planPluginUis, pluginNameFromPath, UI_API_VERSION, collectSceneActions, collectTemplates, findPropertyControl, registerPluginUi } from './pluginRegistry'
import { definePluginUi } from './pluginUi'
import { suggestPresetName, newSceneInstance, presetThumbNames } from './scenes'
import type { ListScenes } from '~/apiTypes/list_scenes'

const def = (name: string, extra: Partial<ListScenes> = {}): ListScenes => ({
  name, category: 'Ambient', properties: [{ name: 'speed', type_id: 'float', default_value: 1 }], has_preview: true, ...extra,
})

describe('plugin UI registry', () => {
  it('resolves property controls and templates from registered plugins', () => {
    const Ctl = () => null
    const ui = definePluginUi({ plugin: 'T', propertyControls: { my_control: Ctl }, presetTemplates: [{ id: 't', name: 'T', description: '', scenes: ['a'] }] })
    registerPluginUi(ui)
    expect(findPropertyControl([ui], 'my_control')).toBe(Ctl)
    expect(findPropertyControl([ui], 'other')).toBeUndefined()
    expect(findPropertyControl([ui], undefined)).toBeUndefined()
    expect(collectTemplates([ui]).map(t => t.id)).toEqual(['t'])
  })

  it('ships the built-in "configure sources" action through the same API', async () => {
    const { usePluginUis } = await import('./pluginRegistry')
    expect(usePluginUis).toBeTypeOf('function')
    const ui = definePluginUi({ plugin: 'A', sceneActions: [{ id: 'x', label: 'X', run: () => {} }] })
    expect(collectSceneActions([ui, definePluginUi({ plugin: 'B' })])).toHaveLength(1)
  })
})

describe('scene helpers', () => {
  it('suggests readable preset names', () => {
    expect(suggestPresetName([])).toBe('New preset')
    expect(suggestPresetName(['starfield'])).toBe('Starfield')
    expect(suggestPresetName(['starfield', 'metablob'])).toBe('Starfield + Metablob')
    expect(suggestPresetName(['Shader:Scenic_Aurora', 'a', 'b'])).toBe('Scenic Aurora + 2 more')
  })

  it('uses the automatic default look for new scenes', () => {
    const d = def('s', { descriptor: { family: 'f', tags: [], intensity: 0, motion: 0, music_affinity: 0, performance_cost: 0, automatic_eligible: true, variants: [{ id: 'calm', label: 'Calm', description: '', tags: [], properties: { speed: 0.2 }, automatic_default: true }] } })
    const scene = newSceneInstance(d)
    expect(scene.variant).toBe('calm')
    expect(scene.arguments.speed).toBe(0.2)
    expect(newSceneInstance(d, '').variant).toBeUndefined()
  })

  it('only thumbnails scenes that have previews', () => {
    const names = presetThumbNames({ scenes: [{ uuid: '1', type: 'a', arguments: {} }, { uuid: '2', type: 'b', arguments: {} }] }, [def('a', { has_preview: false }), def('b')])
    expect(names).toEqual(['b'])
  })
})

describe('plugin UI loading', () => {
  const paths = ['../../../plugins/Video/web/index.tsx', '../../../plugins/AmbientScenes/web/index.ts']

  it('derives the plugin name from the module path', () => {
    expect(pluginNameFromPath(paths[0])).toBe('Video')
  })

  it('loads only modules for plugins the daemon reports', () => {
    expect(selectPluginModules(paths, { ui_api_version: 1, plugins: ['Video'] })).toEqual([paths[0]])
  })

  it('loads everything when the daemon cannot say (old daemon)', () => {
    expect(selectPluginModules(paths, null)).toEqual(paths)
  })

  it('loads nothing when the daemon speaks a newer UI API', () => {
    expect(selectPluginModules(paths, { ui_api_version: UI_API_VERSION + 1, plugins: ['Video'] })).toEqual([])
  })

  it('the Video plugin provides the video_playlist control', async () => {
    const ui = (await import('../../../plugins/Video/web/index')).default
    expect(findPropertyControl([ui], 'video_playlist')).toBeTypeOf('function')
  })

  it('uses runtime bundles only for installed, compatible plugins', () => {
    const info = {
      ui_api_version: 1, plugins: ['Video', 'Other'],
      web_ui: { Video: { api_version: 1, js: 'ui.js' }, Other: { api_version: 2, js: 'ui.js' }, Gone: { js: 'ui.js' } },
    }
    expect(selectRuntimeUis(info).map(([n]) => n)).toEqual(['Video'])
    expect(selectRuntimeUis(null)).toEqual([])
    expect(selectRuntimeUis({ ...info, ui_api_version: UI_API_VERSION + 1 })).toEqual([])
  })

  it('skips a plugin entirely when its installed bundle needs a newer UI API', () => {
    const paths = ['../../../plugins/Video/web/index.tsx', '../../../plugins/AmbientScenes/web/index.ts']
    const plan = planPluginUis(paths, {
      ui_api_version: 1, plugins: ['Video', 'AmbientScenes'],
      web_ui: { Video: { api_version: UI_API_VERSION + 1, js: 'ui.js' } },
    })
    expect(plan.runtime).toEqual([])
    expect(plan.bundled).toEqual([paths[1]])
  })
})
