import { describe, expect, it } from 'vitest'
import { draftReducer, initialDraft, isDraftDirty, type DraftEvent } from './draftSave'

const run = (...events: DraftEvent[]) => events.reduce(draftReducer, initialDraft)

describe('draftReducer', () => {
  it('loading sets a clean baseline', () => {
    expect(isDraftDirty(run({ type: 'loaded' }))).toBe(false)
    expect(isDraftDirty(run({ type: 'edited' }, { type: 'loaded' }))).toBe(false)
  })

  it('a successful save marks the submitted revision clean', () => {
    const state = run({ type: 'edited' }, { type: 'saveStarted', revision: 1 }, { type: 'saveSucceeded', revision: 1 })
    expect(isDraftDirty(state)).toBe(false)
    expect(state.saving).toBeNull()
  })

  it('edits made during a save stay dirty after it succeeds', () => {
    const state = run(
      { type: 'edited' },
      { type: 'saveStarted', revision: 1 },
      { type: 'edited' },
      { type: 'saveSucceeded', revision: 1 },
    )
    expect(isDraftDirty(state)).toBe(true)
  })

  it('a failed save keeps the draft dirty', () => {
    const state = run({ type: 'edited' }, { type: 'saveStarted', revision: 1 }, { type: 'saveFailed' })
    expect(isDraftDirty(state)).toBe(true)
    expect(state.saving).toBeNull()
  })
})
