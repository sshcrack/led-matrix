import { describe, expect, it } from 'vitest'
import { parseLiveFrame } from './liveFrame'

function makeFrame(width: number, height: number, sequence: number, rgb: number[], magic = 'LMF1') {
  const buffer = new ArrayBuffer(12 + rgb.length)
  const view = new DataView(buffer)
  for (let i = 0; i < 4; i++) view.setUint8(i, magic.charCodeAt(i))
  view.setUint16(4, width, true)
  view.setUint16(6, height, true)
  view.setUint32(8, sequence, true)
  new Uint8Array(buffer, 12).set(rgb)
  return buffer
}

describe('parseLiveFrame', () => {
  it('decodes header fields and expands RGB to opaque RGBA', () => {
    const frame = parseLiveFrame(makeFrame(2, 1, 70000, [1, 2, 3, 4, 5, 6]))
    expect(frame.width).toBe(2)
    expect(frame.height).toBe(1)
    expect(frame.sequence).toBe(70000)
    expect(Array.from(frame.rgba)).toEqual([1, 2, 3, 255, 4, 5, 6, 255])
  })

  it('rejects a wrong magic', () => {
    expect(() => parseLiveFrame(makeFrame(1, 1, 0, [0, 0, 0], 'XXXX'))).toThrow('Unsupported')
  })

  it('rejects payloads shorter than the header', () => {
    expect(() => parseLiveFrame(new ArrayBuffer(4))).toThrow('Unsupported')
  })

  it('rejects truncated or oversized pixel data', () => {
    expect(() => parseLiveFrame(makeFrame(2, 2, 0, [1, 2, 3]))).toThrow('Incomplete')
    expect(() => parseLiveFrame(makeFrame(1, 1, 0, [1, 2, 3, 4]))).toThrow('Incomplete')
  })

  it('rejects zero dimensions', () => {
    expect(() => parseLiveFrame(makeFrame(0, 1, 0, []))).toThrow('Incomplete')
  })
})
