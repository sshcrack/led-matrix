import { useCallback, useReducer, useRef } from 'react'

/**
 * Editable draft with save identity.
 *
 * Every edit bumps `revision`. A save records the revision it submitted and,
 * on success, marks only that revision clean, so edits made while the save
 * is in flight stay dirty instead of being silently reported as saved.
 * Loading server data sets a clean baseline without counting as an edit.
 */
export type DraftState = {
  revision: number
  savedRevision: number
  /** Revision currently being submitted, or null when idle. */
  saving: number | null
}

export type DraftEvent =
  | { type: 'loaded' }
  | { type: 'edited' }
  | { type: 'saveStarted'; revision: number }
  | { type: 'saveSucceeded'; revision: number }
  | { type: 'saveFailed' }

export const initialDraft: DraftState = { revision: 0, savedRevision: 0, saving: null }

export function draftReducer(state: DraftState, event: DraftEvent): DraftState {
  switch (event.type) {
    case 'loaded':
      return { ...state, savedRevision: state.revision }
    case 'edited':
      return { ...state, revision: state.revision + 1 }
    case 'saveStarted':
      return { ...state, saving: event.revision }
    case 'saveSucceeded':
      return { ...state, saving: null, savedRevision: Math.max(state.savedRevision, event.revision) }
    case 'saveFailed':
      return { ...state, saving: null }
  }
}

export const isDraftDirty = (state: DraftState) => state.revision !== state.savedRevision

export function useDraftSave() {
  const [state, dispatch] = useReducer(draftReducer, initialDraft)
  // Mirrors the reducer synchronously so a save started right after an edit
  // submits (and later acknowledges) the right revision.
  const revision = useRef(0)
  const saving = useRef(false)

  const edited = useCallback(() => {
    revision.current += 1
    dispatch({ type: 'edited' })
  }, [])
  const loaded = useCallback(() => dispatch({ type: 'loaded' }), [])

  /** Runs `submit` unless a save is already in flight. Resolves to true on success. */
  const save = useCallback(async (submit: () => Promise<void>) => {
    if (saving.current) return false
    saving.current = true
    const submitted = revision.current
    dispatch({ type: 'saveStarted', revision: submitted })
    try {
      await submit()
      dispatch({ type: 'saveSucceeded', revision: submitted })
      return true
    } catch (error) {
      dispatch({ type: 'saveFailed' })
      throw error
    } finally {
      saving.current = false
    }
  }, [])

  return { isDirty: isDraftDirty(state), isSaving: state.saving !== null, edited, loaded, save }
}
