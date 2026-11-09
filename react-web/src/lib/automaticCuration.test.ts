import { describe, expect, it } from 'vitest'
import type { ListScenes } from '~/apiTypes/list_scenes'
import { automaticLooks } from './automaticCuration'

const scene = (tags: string[], variants: { id: string; automatic_default?: boolean }[]) =>
  ({
    name: 'lake',
    descriptor: {
      family: 'hiking',
      tags,
      intensity: 0.2,
      motion: 0.2,
      music_affinity: 0,
      performance_cost: 0.3,
      automatic_eligible: true,
      variants: variants.map(v => ({ ...v, label: v.id, description: '', properties: {}, tags: [] })),
    },
  }) as unknown as ListScenes

describe('automaticLooks', () => {
  it('lists only the variants of a plain scene with variants', () => {
    const looks = automaticLooks(scene([], [{ id: 'calm' }]), {})
    expect(looks.map(l => l.key)).toEqual(['lake/calm'])
  })

  it('adds the default look for scenes that follow the real world', () => {
    const looks = automaticLooks(
      scene(['environment-adaptive'], [{ id: 'night', automatic_default: false }]),
      {},
    )
    expect(looks.map(l => [l.key, l.state])).toEqual([
      ['lake', 'on'],
      ['lake/night', 'off'],
    ])
  })

  it('does not play ineligible scenes', () => {
    const s = scene([], [])
    s.descriptor!.automatic_eligible = false
    expect(automaticLooks(s, {})).toEqual([])
  })
})
