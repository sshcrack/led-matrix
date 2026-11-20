import { describe as suite, expect, it } from 'vitest'
import { canvasSize, describe, layoutId, sameOptions, validate, type PanelOptions } from './panelHardwareModel'

const base: PanelOptions = {
  rows: 64, cols: 64, chain: 2, parallel: 2, slowdown: 3, gpio_mapping: 'regular', pixel_mapper: 'Rotate:270',
  multiplexing: 0, row_addr_type: 0, panel_type: '', pwm_bits: 11, pwm_lsb_nanoseconds: 130, pwm_dither_bits: 0, rgb_sequence: 'RGB',
}

suite('panel hardware model', () => {
  it('describes the layout and canvas', () => {
    expect(describe(base)).toBe('4 panels of 64×64 · 128×128 pixels')
    expect(canvasSize({ ...base, rows: 32, chain: 2, parallel: 1, pixel_mapper: 'Rotate:90' })).toEqual({ width: 32, height: 128 })
    expect(layoutId(base)).toBe('2x2')
    expect(layoutId({ chain: 5, parallel: 1 })).toBe('custom')
  })

  it('accepts what the daemon accepts and rejects the rest', () => {
    expect(validate(base)).toBeNull()
    expect(validate({ ...base, pixel_mapper: 'U-mapper;Rotate:90;Mirror:H' })).toBeNull()
    expect(validate({ ...base, rows: 7 })).toMatch(/Rows per panel/)
    expect(validate({ ...base, chain: 1.5 })).toMatch(/Panels per chain/)
    expect(validate({ ...base, pixel_mapper: 'Rotate:45' })).toMatch(/Pixel mapper/)
    expect(validate({ ...base, pixel_mapper: 'Rotate:90;$(reboot)' })).toMatch(/Pixel mapper/)
    expect(validate({ ...base, gpio_mapping: 'regular --led-daemon=1' })).toBe('Unknown wiring')
  })

  it('compares every option', () => {
    expect(sameOptions(base, { ...base })).toBe(true)
    expect(sameOptions(base, { ...base, rgb_sequence: 'RBG' })).toBe(false)
  })
})
