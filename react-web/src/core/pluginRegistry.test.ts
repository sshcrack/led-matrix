import { describe, expect, it } from 'vitest'
import { collectSceneActions, collectTemplates, findPropertyControl, registerPluginUi } from './pluginRegistry'
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
