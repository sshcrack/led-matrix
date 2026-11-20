/** Matches PanelHardware::to_json() in src_matrix/hardware/panel_options.cpp. */
export interface PanelOptions {
  rows: number
  cols: number
  chain: number
  parallel: number
  slowdown: number
  gpio_mapping: string
  pixel_mapper: string
  multiplexing: number
  row_addr_type: number
  panel_type: string
  pwm_bits: number
  pwm_lsb_nanoseconds: number
  pwm_dither_bits: number
  rgb_sequence: string
}

/** GET /hardware. */
export interface HardwareStatus {
  running: PanelOptions
  flags: string
  supported: boolean
  unsupported_reason?: string
  setup_needed: boolean
  pending: boolean
  result: { state: 'applied' | 'failed'; message: string; flags: string; at: string } | null
  restart?: boolean
}

export type NumberKey = { [K in keyof PanelOptions]: PanelOptions[K] extends number ? K : never }[keyof PanelOptions]

/** Same ranges as PanelHardware::validate() and apply_hardware.sh. */
export const RANGES: Record<NumberKey, [number, number]> = {
  rows: [8, 64],
  cols: [16, 256],
  chain: [1, 16],
  parallel: [1, 3],
  slowdown: [0, 5],
  multiplexing: [0, 32],
  row_addr_type: [0, 5],
  pwm_bits: [1, 11],
  pwm_lsb_nanoseconds: [50, 3000],
  pwm_dither_bits: [0, 2],
}

export const GPIO_MAPPINGS = [
  { value: 'regular', label: 'Direct wiring / Electrodragon (regular)' },
  { value: 'adafruit-hat', label: 'Adafruit HAT / Bonnet' },
  { value: 'adafruit-hat-pwm', label: 'Adafruit HAT with PWM mod' },
  { value: 'regular-pi1', label: 'Regular, Pi 1 header' },
  { value: 'classic', label: 'Classic' },
  { value: 'classic-pi1', label: 'Classic, Pi 1 header' },
  { value: 'compute-module', label: 'Compute Module' },
]
export const PANEL_TYPES = ['', 'FM6126A', 'FM6127']
export const RGB_SEQUENCES = ['RGB', 'RBG', 'GRB', 'GBR', 'BRG', 'BGR']
export const ROTATIONS = ['', 'Rotate:90', 'Rotate:180', 'Rotate:270']

const MAPPER_PART = String.raw`(Rotate:(0|90|180|270)|Mirror:[HV]|U-mapper|V-mapper(:Z)?)`
const MAPPER = new RegExp(`^${MAPPER_PART}(;${MAPPER_PART}){0,3}$`)

/** Common ways to wire panels; `parallel` chains use the other connector outputs. */
export const LAYOUTS = [
  { id: '1', label: 'One panel', chain: 1, parallel: 1 },
  { id: '2x1', label: 'Two panels side by side', chain: 2, parallel: 1 },
  { id: '2x2', label: 'Four panels, 2 chains of 2 (2 outputs)', chain: 2, parallel: 2 },
  { id: '4x1', label: 'Four panels in one chain', chain: 4, parallel: 1 },
  { id: '3x3', label: 'Nine panels, 3 chains of 3 (3 outputs)', chain: 3, parallel: 3 },
] as const

export function layoutId(o: Pick<PanelOptions, 'chain' | 'parallel'>) {
  return LAYOUTS.find(l => l.chain === o.chain && l.parallel === o.parallel)?.id ?? 'custom'
}

/** Canvas size the scenes see, before the pixel mapper. */
export function canvasSize(o: PanelOptions) {
  let width = o.cols * o.chain, height = o.rows * o.parallel
  for (const part of o.pixel_mapper.split(';')) {
    if (part === 'Rotate:90' || part === 'Rotate:270') [width, height] = [height, width]
  }
  return { width, height }
}

export function describe(o: PanelOptions) {
  const panels = o.chain * o.parallel
  const { width, height } = canvasSize(o)
  return `${panels} panel${panels === 1 ? '' : 's'} of ${o.cols}×${o.rows} · ${width}×${height} pixels`
}

/** First problem with these options, or null. The daemon checks again (and asks the LED library). */
export function validate(o: PanelOptions): string | null {
  for (const [key, [low, high]] of Object.entries(RANGES) as [NumberKey, [number, number]][]) {
    const v = o[key]
    if (!Number.isInteger(v) || v < low || v > high) return `${LABELS[key]} must be ${low}–${high}`
  }
  if (!GPIO_MAPPINGS.some(m => m.value === o.gpio_mapping)) return 'Unknown wiring'
  if (!PANEL_TYPES.includes(o.panel_type)) return 'Unknown panel type'
  if (!RGB_SEQUENCES.includes(o.rgb_sequence)) return 'Colour order must be an order of R, G and B'
  if (o.pixel_mapper !== '' && !MAPPER.test(o.pixel_mapper)) return 'Pixel mapper must be up to four of Rotate:<0|90|180|270>, Mirror:<H|V>, U-mapper, V-mapper, separated by ;'
  return null
}

export function sameOptions(a: PanelOptions, b: PanelOptions) {
  return (Object.keys(a) as (keyof PanelOptions)[]).every(k => a[k] === b[k])
}

export const LABELS: Record<NumberKey, string> = {
  rows: 'Rows per panel',
  cols: 'Columns per panel',
  chain: 'Panels per chain',
  parallel: 'Parallel chains',
  slowdown: 'GPIO slowdown',
  multiplexing: 'Multiplexing',
  row_addr_type: 'Row address type',
  pwm_bits: 'PWM bits',
  pwm_lsb_nanoseconds: 'PWM LSB (ns)',
  pwm_dither_bits: 'Dither bits',
}
