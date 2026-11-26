export type DirectorMode = 'mixed' | 'environment' | 'party' | 'chill'

export type Status = {
  current: string
  turned_off: boolean
  operation_mode: 'automatic' | 'manual'
  /** What Automatic Mode is for (older daemons omit it). */
  director_mode?: DirectorMode
  automatic_active: boolean
  /** Daemon version (older daemons omit it); a change makes the UI look for a new build. */
  version?: string
}
