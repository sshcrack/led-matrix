/**
 * The only host API a plugin web module may import (as `~/plugin-sdk`), besides
 * `react` and `lucide-react`.
 *
 * In dev and in the bundled build this file is imported directly. For runtime
 * plugin bundles (scripts/build-plugin-uis.mjs) every name exported below is
 * replaced by a binding to `globalThis.__LED_MATRIX_SDK__['plugin-sdk']`, which
 * `installPluginSdk()` fills from this very module — so a plugin built once
 * keeps working against any host that still provides these names.
 *
 * Keep this a plain list of `export { … } from` lines: the build script reads
 * the exported names from it with a regex.
 */
export { definePluginUi } from '~/core/pluginUi'
export type { PluginUi, PropertyControlProps, SceneAction, SceneActionContext, PresetTemplate } from '~/core/pluginUi'
export { useApiUrl } from '~/components/apiUrl/apiUrlContext'
export { Button } from '~/components/ui/button'
export { Input } from '~/components/ui/input'
export { Label } from '~/components/ui/label'
export { propertyLabel, propertyDescription } from '~/components/modify-preset/propertyUi'
export type { Property } from '~/apiTypes/list_scenes'
