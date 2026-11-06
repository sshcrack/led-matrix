/**
 * Presets deleted in this session. A still-mounted editor (or its unmount flush)
 * must not save a draft for one of these: POST /preset would re-create it.
 */
const deleted = new Set<string>()
export const markPresetDeleted = (id: string) => { deleted.add(id) }
export const unmarkPresetDeleted = (id: string) => { deleted.delete(id) }
export const isPresetDeleted = (id: string) => deleted.has(id)
