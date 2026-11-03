// Parser for the binary "LMF1" live-frame protocol sent by the matrix:
// "LMF1" magic, u16 width, u16 height, u32 sequence (all little endian),
// followed by width * height RGB triplets.

export const LIVE_FRAME_HEADER_BYTES = 12

export interface LiveFrame {
  width: number
  height: number
  sequence: number
  /** RGBA pixel data, alpha always 255. */
  rgba: Uint8ClampedArray<ArrayBuffer>
}

export function parseLiveFrame(buffer: ArrayBuffer): LiveFrame {
  const bytes = new Uint8Array(buffer)
  if (bytes.length < LIVE_FRAME_HEADER_BYTES || bytes[0] !== 0x4c || bytes[1] !== 0x4d || bytes[2] !== 0x46 || bytes[3] !== 0x31)
    throw new Error('Unsupported live frame payload')

  const view = new DataView(buffer)
  const width = view.getUint16(4, true)
  const height = view.getUint16(6, true)
  const sequence = view.getUint32(8, true)
  const expected = LIVE_FRAME_HEADER_BYTES + width * height * 3
  if (!width || !height || bytes.length !== expected)
    throw new Error('Incomplete live frame payload')

  const rgba = new Uint8ClampedArray(width * height * 4)
  for (let source = LIVE_FRAME_HEADER_BYTES, target = 0; source < bytes.length; source += 3, target += 4) {
    rgba[target] = bytes[source]
    rgba[target + 1] = bytes[source + 1]
    rgba[target + 2] = bytes[source + 2]
    rgba[target + 3] = 255
  }
  return { width, height, sequence, rgba }
}
