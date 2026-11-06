import * as React from 'react'
import * as jsxRuntime from 'react/jsx-runtime'
import * as sdk from '~/plugin-sdk'

declare global {
  var __LED_MATRIX_SDK__: Record<string, unknown> | undefined
}

/**
 * Publish the shared modules runtime plugin bundles import (they are built with
 * these as externals, see scripts/build-plugin-uis.mjs). Must run before any
 * runtime bundle is evaluated.
 */
export function installPluginSdk() {
  globalThis.__LED_MATRIX_SDK__ = { 'react': React, 'react/jsx-runtime': jsxRuntime, 'plugin-sdk': sdk }
}
