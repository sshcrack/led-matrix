import { afterEach, describe, expect, it, vi } from 'vitest'
import { cleanup, render, screen } from '@testing-library/react'
import Diagnostics from './Diagnostics'

const fixture = (worker_available: boolean, worker_compatible?: boolean) => ({
  uptime_seconds: 60, desktop_connections: 1,
  renderer: { active_scene: 'audio_marina_sunset', frames: 60, fps: 60, render_ms_average: 10, render_ms_max: 20, slow_frames: 0, scene_errors: {}, scene_performance: {} },
  udp: {}, audio_transport: {}, audio: { bands: {} }, registry: { ok: true, errors: [], warnings: [] }, plugins: [],
  remote_render: { worker_available, worker_compatible, worker_scene_count: 71, local_protocol: 4, worker_protocol: 3, worker_compatible_scene_count: worker_compatible ? 70 : 0 },
})

afterEach(() => { cleanup(); vi.unstubAllGlobals() })

it('explains a connected incompatible desktop and offers matching releases', async () => {
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue(new Response(JSON.stringify(fixture(true, false)))))
  render(<Diagnostics />)
  expect(await screen.findByRole('alert')).toBeTruthy()
  expect(screen.getByText('Desktop app is incompatible')).toBeTruthy()
  expect(screen.getByText('Incompatible')).toBeTruthy()
  expect(screen.queryByText('Ready')).toBeNull()
  expect(screen.getByRole('link', { name: 'Download desktop releases' }).getAttribute('href')).toBe('https://github.com/sshcrack/led-matrix/releases')
  expect(screen.getByText('4')).toBeTruthy()
  expect(screen.getByText('3')).toBeTruthy()
  expect(screen.getByText('0 / 71')).toBeTruthy()
})

describe.each([
  [true, true, 'Ready'],
  [false, false, 'Unavailable'],
  [true, undefined, 'Ready'],
] as const)('worker availability %s, compatibility %s', (available, compatible, label) => {
  it('does not report an incompatibility without an explicit live mismatch', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(new Response(JSON.stringify(fixture(available, compatible)))))
    render(<Diagnostics />)
    expect(await screen.findByText(label)).toBeTruthy()
    expect(screen.queryByRole('alert')).toBeNull()
  })
})

it('shows refresh health and turns steady brightness on', async () => {
  const refresh = {
    refresh_cpu: 3, timing_available: true,
    windows: { '60s': { seconds: 60, refreshes: 5682, refresh_hz: 94.7, median_us: 10565, p99_us: 11025, max_us: 12705, late_per_1000: 0, interrupts_per_second: { device_and_timer: [1, 2, 3, 8459.4] } } },
    steady_brightness: { enabled: false, state: 'off' },
  }
  const fetchMock = vi.fn((url: string) => Promise.resolve(new Response(JSON.stringify(
    url.endsWith('/steady_brightness') ? { enabled: true } : { ...fixture(true, true), refresh }))))
  vi.stubGlobal('fetch', fetchMock)
  render(<Diagnostics />)
  expect(await screen.findByText('94.7 Hz')).toBeTruthy()
  expect(screen.getByText('8459/s')).toBeTruthy()
  screen.getByRole('switch', { name: 'Steady brightness' }).click()
  await vi.waitFor(() => expect(fetchMock.mock.calls.some(([url]) => String(url).endsWith('/steady_brightness'))).toBe(true))
  const call = fetchMock.mock.calls.find(([url]) => String(url).endsWith('/steady_brightness')) as unknown as [string, RequestInit]
  expect(call[1].body).toBe(JSON.stringify({ enabled: true }))
})
