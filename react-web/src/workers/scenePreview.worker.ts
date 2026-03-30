/**
 * Scene Preview Web Worker
 *
 * Loads the Emscripten WASM module and drives a 15-FPS render loop for a
 * single scene. Property updates can be pushed at any time via messages.
 *
 * Message protocol (main thread → worker):
 *   { type: 'init',           sceneName: string, width?: number, height?: number }
 *   { type: 'set_properties', json: string }
 *   { type: 'stop' }
 *
 * Message protocol (worker → main thread):
 *   { type: 'frame',   imageData: ImageData }   – transferable
 *   { type: 'error',   message: string }
 *   { type: 'stopped' }
 */

// -------------------------------------------------------------------------
// Types mirroring the Emscripten module interface
// -------------------------------------------------------------------------
interface WasmModule {
  // Exported functions
  _wasm_list_scenes: () => number;
  _wasm_scene_create: (namePtr: number, width: number, height: number) => number;
  _wasm_scene_set_properties: (jsonPtr: number) => void;
  _wasm_scene_render_frame: () => number;
  _wasm_scene_destroy: () => void;
  _wasm_get_frame_buffer: () => number;
  _wasm_get_buffer_size: () => number;
  // Emscripten helpers
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  UTF8ToString: (ptr: number) => string;
  stringToUTF8: (str: string, outPtr: number, maxBytes: number) => void;
  lengthBytesUTF8: (str: string) => number;
  // WASM heap views
  HEAPU8: Uint8Array;
}

// -------------------------------------------------------------------------
// Module-level state
// -------------------------------------------------------------------------
let mod: WasmModule | null = null;
let loopHandle: ReturnType<typeof setInterval> | null = null;
let currentWidth  = 128;
let currentHeight = 128;

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

/** Allocate a null-terminated UTF-8 string in WASM heap memory. Caller must _free() it. */
function allocString(m: WasmModule, s: string): number {
  const len = m.lengthBytesUTF8(s) + 1;
  const ptr = m._malloc(len);
  m.stringToUTF8(s, ptr, len);
  return ptr;
}

/** Dynamically import the Emscripten factory and instantiate the WASM module. */
async function loadModule(): Promise<WasmModule> {
  // The compiled artefacts are served from /web/wasm/ (react-web public dir).
  // Vite's base is '/web/', so the asset URL is /web/wasm/scene_preview.js.
  // Use a variable URL so that TypeScript does not attempt to statically resolve
  // the path (the file only exists after the WASM build step).
  const url: string = '/web/wasm/scene_preview.js';
  // eslint-disable-next-line no-new-func
  const factoryModule = await new Function('u', 'return import(u)')(url) as {
    default: (opts: { locateFile: (f: string) => string }) => Promise<WasmModule>;
  };
  return factoryModule.default({
    locateFile: (file: string) => `/web/wasm/${file}`,
  });
}

function stopLoop(): void {
  if (loopHandle !== null) {
    clearInterval(loopHandle);
    loopHandle = null;
  }
}

function destroyScene(m: WasmModule): void {
  try { m._wasm_scene_destroy(); } catch { /* ignore */ }
}

// -------------------------------------------------------------------------
// Message handler
// -------------------------------------------------------------------------
self.onmessage = async (event: MessageEvent) => {
  const msg = event.data as
    | { type: 'init';           sceneName: string; width?: number; height?: number }
    | { type: 'set_properties'; json: string }
    | { type: 'stop' };

  if (msg.type === 'init') {
    stopLoop();

    try {
      if (!mod) {
        mod = await loadModule();
      }

      currentWidth  = msg.width  ?? 128;
      currentHeight = msg.height ?? 128;

      if (mod) {
        destroyScene(mod);
      }

      const namePtr = allocString(mod, msg.sceneName);
      const rc = mod._wasm_scene_create(namePtr, currentWidth, currentHeight);
      mod._free(namePtr);

      if (rc !== 0) {
        self.postMessage({ type: 'error', message: `Scene '${msg.sceneName}' not found in WASM module` });
        return;
      }

      // ~15 FPS render loop
      loopHandle = setInterval(() => {
        if (!mod) return;

        const cont = mod._wasm_scene_render_frame();

        const bufPtr  = mod._wasm_get_frame_buffer();
        const bufSize = mod._wasm_get_buffer_size();

        if (bufPtr !== 0 && bufSize > 0) {
          // Copy out of WASM heap before transferring (heap may be resized)
          const raw  = mod.HEAPU8.slice(bufPtr, bufPtr + bufSize);
          const imgData = new ImageData(
            new Uint8ClampedArray(raw.buffer, raw.byteOffset, raw.byteLength),
            currentWidth,
            currentHeight,
          );
          // Use the Transferable array overload supported by DedicatedWorkerGlobalScope
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          ;(self as any).postMessage(
            { type: 'frame', imageData: imgData },
            [imgData.data.buffer],
          )
        }

        if (!cont) {
          stopLoop();
          self.postMessage({ type: 'stopped' });
        }
      }, Math.round(1000 / 15));

    } catch (err) {
      self.postMessage({ type: 'error', message: String(err) });
    }

  } else if (msg.type === 'set_properties') {
    if (!mod) return;
    try {
      const ptr = allocString(mod, msg.json);
      mod._wasm_scene_set_properties(ptr);
      mod._free(ptr);
    } catch (err) {
      console.warn('[WasmScenePreview] failed to set properties:', err);
    }

  } else if (msg.type === 'stop') {
    stopLoop();
    if (mod) destroyScene(mod);
    self.postMessage({ type: 'stopped' });
  }
};
