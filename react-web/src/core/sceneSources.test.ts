import { describe, expect, it } from 'vitest'
import type { ListProviders, ListScenes } from '~/apiTypes/list_scenes'
import { newSource, readSources, sourceOptions, sourcesProperty, writeSources } from './sceneSources'

const scene = (properties: object[]) => ({ name: 's', properties }) as unknown as ListScenes
const imageDaemon = [
  { name: 'collection', properties: [{ name: 'images', default_value: [], type_id: 'string[]' }] },
  { name: 'pages', properties: [{ name: 'pages_begin', default_value: 0, type_id: 'int' }, { name: 'pages_end', default_value: -1, type_id: 'int' }] },
] as unknown as ListProviders[]

describe('scene sources', () => {
  it('finds the sources property by control id, or by name on older daemons', () => {
    expect(sourcesProperty(scene([{ name: 'x', type_id: 'json', additional: { control: 'shader_sources' } }]))?.kind).toBe('shader')
    expect(sourcesProperty(scene([{ name: 'providers', type_id: 'json', additional: null }]))?.kind).toBe('image')
    expect(sourcesProperty(scene([{ name: 'shader_providers', type_id: 'json' }]))?.kind).toBe('shader')
    expect(sourcesProperty(scene([{ name: 'providers', type_id: 'json', additional: { control: 'video_playlist' } }]))).toBeNull()
    expect(sourcesProperty(scene([{ name: 'speed', type_id: 'int' }]))).toBeNull()
  })

  it('round-trips the daemon format and drops the local keys', () => {
    const stored = [{ type: 'collection', arguments: { images: ['a.png'] } }, { type: 'pages', arguments: { pages_begin: 2, pages_end: 5 } }]
    const read = readSources(stored)
    expect(read.every(s => typeof s.uuid === 'string')).toBe(true)
    expect(writeSources(read)).toEqual(stored)
    expect(readSources(null)).toEqual([])
  })

  it('migrates the begin/end arguments the UI used to write', () => {
    expect(writeSources(readSources([{ type: 'pages', arguments: { begin: 0, end: -1 } }])))
      .toEqual([{ type: 'pages', arguments: { pages_begin: 0, pages_end: -1 } }])
  })

  it('offers each provider type once, with the daemon defaults', () => {
    const options = sourceOptions('image', imageDaemon)
    expect(options.map(o => o.value)).toEqual(['collection', 'pages'])
    expect(options[0].label).toBe('Image collection')
    expect(newSource(options[1]).arguments).toEqual({ pages_begin: 0, pages_end: -1 })
    expect(sourceOptions('shader', null).map(o => o.value)).toEqual(['random', 'shader_collection'])
  })
})
