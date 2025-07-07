import { invoke } from "@tauri-apps/api/core";
import "./App.css";
// Components
import ConnectionControls from "./components/ConnectionControls";
import AudioSettings from "./components/AudioSettings";
import AnalysisSettings from "./components/AnalysisSettings";
import StartupSettings from "./components/StartupSettings";
import Visualizer from "./components/Visualizer";
import AudioDeviceSettings from "./components/AudioDeviceSettings";

// Hooks
import useAudioData from "./hooks/useAudioData";
import useConfigWithAutoUpdate from "./hooks/useConfigWithAutoUpdate";

// Types
export interface AudioVisualizerConfig {
  hostname: string;
  port: number;
  num_bands: number;
  gain: number;
  smoothing: number;
  min_freq: number;
  max_freq: number;
  mode: number; // 0 = discrete frequencies, 1 = 1/3 octave bands, 2 = full octave bands
  linear_amplitude: boolean; // true = linear amplitude, false = logarithmic (dB) amplitude
  frequency_scale: string; // "log", "linear", "bark", "mel"
  interpolate_bands: boolean; // Whether to interpolate frequency bands that cannot be directly calculated
  skip_non_processed: boolean; // true = skip bands that haven't been processed and omit them from the output
  start_minimized_to_tray: boolean; // true = start application hidden in system tray
  autostart_enabled: boolean; // true = add application to autostart
  selected_output_device: string | null; // ID of the selected output device
}

function App() {
  const {
    config,
    hasUnsavedChanges,
    isRunning,
    connectionStatus,
    handleConfigChange,
    saveConfig,
    startVisualization,
    stopVisualization
  } = useConfigWithAutoUpdate({
    hostname: "",
    port: 0,
    num_bands: 64,
    gain: 2.0,
    smoothing: 0.8,
    min_freq: 20.0,
    max_freq: 20000.0,
    mode: 0,
    linear_amplitude: false,
    frequency_scale: "log",
    interpolate_bands: true,
    skip_non_processed: true,
    start_minimized_to_tray: false,
    autostart_enabled: false,
    selected_output_device: null
  });

  // Use our custom hook to get audio data
  const bands = useAudioData(isRunning);

  const minimizeToTray = async () => {
    try {
      await invoke("toggle_window_visibility");
    } catch (error) {
      console.error("Failed to minimize to tray:", error);
    }
  };

  return (
    <div className="min-h-screen bg-gray-100 text-gray-900 p-4">
      <div className="p-5 mx-auto bg-white shadow-md rounded-lg overflow-hidden">
        <div className="p-6">
          <div className="flex justify-between items-center mb-6">
            <h1 className="text-2xl font-bold">Audio Visualizer</h1>
            <button
              onClick={minimizeToTray}
              className="bg-gray-200 hover:bg-gray-300 text-gray-800 px-4 py-2 rounded transition-colors cursor-pointer"
            >
              Minimize to Tray
            </button>
          </div>

          <div className="space-y-6">
            <ConnectionControls
              hostname={config.hostname}
              port={config.port}
              isRunning={isRunning}
              connectionStatus={connectionStatus}
              onConfigChange={handleConfigChange}
              onStart={startVisualization}
              onStop={stopVisualization}
            />

            <div className="border-t border-gray-200 pt-4"></div>

            <AudioSettings
              config={config}
              onConfigChange={handleConfigChange}
            />

            <div className="border-t border-gray-200 pt-4"></div>

            <AnalysisSettings
              config={config}
              onConfigChange={handleConfigChange}
            />

            <div className="border-t border-gray-200 pt-4"></div>

            <AudioDeviceSettings
              config={config}
              onConfigChange={handleConfigChange}
              isRunning={isRunning}
            />

            <div className="border-t border-gray-200 pt-4"></div>

            <StartupSettings
              config={config}
              onConfigChange={handleConfigChange}
            />

            <div className="border-t border-gray-200 pt-4"></div>

            <div className="flex justify-center">
              <button
                onClick={saveConfig}
                className={`
                    px-6 py-2
                    rounded text-white
                    font-medium transition-colors cursor-pointer
                    ${hasUnsavedChanges ? 'bg-green-500 hover:bg-green-600' : 'bg-blue-500 hover:bg-blue-600'}
                  `}
              >
                Save Settings
              </button>
            </div>

            <div className="border-t border-gray-200 pt-4"></div>

            <Visualizer bands={bands} />
          </div>
        </div>
      </div>
    </div>
  );
}

export default App;
