// Returns the preset id from the URL, with URL decoding
export function usePresetId(preset_id: string | undefined): string {
  return preset_id ? decodeURIComponent(preset_id) : ''
}
