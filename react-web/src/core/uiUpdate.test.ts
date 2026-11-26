import { describe, expect, it, vi } from 'vitest'
import { createUpdateWatcher } from './uiUpdate'

describe('createUpdateWatcher', () => {
  it('ignores the first observation and unchanged state', () => {
    const check = vi.fn()
    const observe = createUpdateWatcher(check)
    observe({ online: true, version: '1.0.0' })
    observe({ online: true, version: '1.0.0' })
    expect(check).not.toHaveBeenCalled()
  })

  it('checks when the daemon reports a new version', () => {
    const check = vi.fn()
    const observe = createUpdateWatcher(check)
    observe({ online: true, version: '1.0.0' })
    observe({ online: true, version: '1.0.1' })
    expect(check).toHaveBeenCalledTimes(1)
  })

  it('checks when the daemon comes back after being unreachable', () => {
    const check = vi.fn()
    const observe = createUpdateWatcher(check)
    observe({ online: true, version: '1.0.0' })
    observe({ online: false, version: '1.0.0' })
    expect(check).not.toHaveBeenCalled()
    observe({ online: true, version: '1.0.0' })
    expect(check).toHaveBeenCalledTimes(1)
  })

  it('does not treat a version appearing (older daemon, first load) as a change', () => {
    const check = vi.fn()
    const observe = createUpdateWatcher(check)
    observe({ online: true, version: undefined })
    observe({ online: true, version: '1.0.0' })
    expect(check).not.toHaveBeenCalled()
  })
})
