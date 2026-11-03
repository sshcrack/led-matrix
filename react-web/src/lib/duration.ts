export function clamp(value: number, min?: number, max?: number) {
  if (min !== undefined) value = Math.max(min, value)
  if (max !== undefined) value = Math.min(max, value)
  return value
}

export function formatDuration(milliseconds: number): string {
  const safe = Math.max(0, Math.round(milliseconds))
  if (safe === 0) return '0s'
  if (safe < 1000) return `${safe}ms`

  const totalSeconds = safe / 1000
  if (totalSeconds < 60) {
    return `${Number(totalSeconds.toFixed(totalSeconds % 1 === 0 ? 0 : 2))}s`
  }

  const minutes = Math.floor(totalSeconds / 60)
  const seconds = totalSeconds - minutes * 60
  if (seconds === 0) return `${minutes}m`
  return `${minutes}m ${Number(seconds.toFixed(seconds % 1 === 0 ? 0 : 2))}s`
}

export function parseDuration(input: string): number | null {
  const normalized = input.trim().toLowerCase().replace(/,/g, '.')
  if (!normalized) return null

  // A plain number is interpreted as seconds because this is the most useful
  // unit for people editing scene and transition durations.
  if (/^\d*\.?\d+$/.test(normalized)) {
    const seconds = Number(normalized)
    return Number.isFinite(seconds) ? Math.round(seconds * 1000) : null
  }

  const token = /(\d*\.?\d+)\s*(ms|milliseconds?|s|sec(?:ond)?s?|m|min(?:ute)?s?)/g
  let total = 0
  let matched = false

  for (const match of normalized.matchAll(token)) {
    matched = true
    const amount = Number(match[1])
    if (!Number.isFinite(amount)) return null

    const unit = match[2]
    if (unit === 'ms' || unit.startsWith('millisecond')) total += amount
    else if (unit === 'm' || unit.startsWith('min')) total += amount * 60_000
    else total += amount * 1000
  }

  if (!matched) return null
  const remainder = normalized.replace(token, '').replace(/[\s+]/g, '')
  if (remainder.length > 0) return null
  return Math.round(total)
}

