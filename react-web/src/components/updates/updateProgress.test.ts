import { describe, expect, it } from 'vitest'
import type { UpdateStatus } from '~/apiTypes/update'
import { describeInstallation, installReducer, serverPhase, type InstallEvent, type Installation } from './updateProgress'

const status = (code: number, extra: Partial<UpdateStatus> = {}): InstallEvent => ({
  type: 'statusLoaded',
  status: { auto_update_enabled: true, check_interval_hours: 24, current_version: '1.0.0', latest_version: '1.1.0', update_available: true, status: code, error_message: '', ...extra },
})
const run = (events: InstallEvent[], from: Installation = { kind: 'idle' }) => events.reduce(installReducer, from)
const accepted = run([{ type: 'requested', target: '1.1.0' }, { type: 'accepted' }])

describe('update installation state machine', () => {
  it('names the Pi status codes', () => {
    expect([0, 1, 2, 3, 4, 5, 6].map(code => serverPhase({ status: code })))
      .toEqual(['idle', 'checking', 'downloading', 'installing', 'error', 'success', 'disabled'])
    expect(serverPhase({ status: 42 })).toBeNull()
    expect(serverPhase(null)).toBeNull()
  })

  it('follows a successful install through the restart', () => {
    expect(accepted).toMatchObject({ kind: 'running', phase: 'checking' })
    const downloading = run([status(2)], accepted)
    expect(downloading).toMatchObject({ kind: 'running', phase: 'downloading' })
    const restarting = run([status(3), { type: 'unreachable' }, { type: 'unreachable' }], downloading)
    expect(restarting.kind).toBe('restarting')
    // The new daemon answers while update_service.sh is still health-checking it: not done yet.
    const checking = run([status(0, { current_version: '1.1.0' })], restarting)
    expect(checking.kind).toBe('restarting')
    expect(run([status(5, { current_version: '1.1.0' })], checking)).toEqual({ kind: 'done', version: '1.1.0' })
  })

  it('reports a rollback with the Pi message', () => {
    const rolledBack = run([status(2), { type: 'unreachable' }, status(0), status(4, { error_message: 'Update failed: health checks failed' })], accepted)
    expect(rolledBack).toEqual({ kind: 'failed', message: 'Update failed: health checks failed' })
  })

  it('reports a download failure before any restart', () => {
    expect(run([status(4, { error_message: 'Download failed: HTTP 404' })], accepted)).toEqual({ kind: 'failed', message: 'Download failed: HTTP 404' })
  })

  it('reports a rejected request and a timeout', () => {
    expect(run([{ type: 'requested', target: '1.1.0' }, { type: 'rejected', message: 'nope' }])).toEqual({ kind: 'failed', message: 'nope' })
    expect(run([{ type: 'unreachable' }, { type: 'timedOut' }], accepted).kind).toBe('failed')
  })

  it('ignores status and repeat clicks that do not belong to a followed install', () => {
    const idle: Installation = { kind: 'idle' }
    expect(run([status(4, { error_message: 'old failure' }), { type: 'unreachable' }, { type: 'timedOut' }], idle)).toBe(idle)
    expect(run([{ type: 'requested', target: '2.0.0' }], accepted)).toBe(accepted)
    expect(run([{ type: 'requested', target: '1.1.0' }], { kind: 'failed', message: 'x' }).kind).toBe('requesting')
  })

  it('describes every phase in words', () => {
    expect(describeInstallation({ kind: 'idle' })).toBeNull()
    expect(describeInstallation(accepted)?.text).toBe('Preparing 1.1.0…')
    expect(describeInstallation({ kind: 'restarting', target: '1.1.0' })?.tone).toBe('progress')
    expect(describeInstallation({ kind: 'done', version: '1.1.0' })?.text).toMatch(/^Updated to 1\.1\.0\./)
  })
})
