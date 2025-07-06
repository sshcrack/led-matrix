import React from 'react';
import { AudioVisualizerConfig } from '../App';

interface AnalysisSettingsProps {
  config: AudioVisualizerConfig;
  onConfigChange: (config: Partial<AudioVisualizerConfig>) => void;
}

const AnalysisSettings: React.FC<AnalysisSettingsProps> = ({
  config,
  onConfigChange,
}) => {
  const modeOptions = [
    { value: 0, label: 'Discrete Frequencies' },
    { value: 1, label: '1/3 Octave Bands' },
    { value: 2, label: 'Full Octave Bands' }
  ];

  const scaleOptions = [
    { value: 'log', label: 'Logarithmic' },
    { value: 'linear', label: 'Linear' },
    { value: 'bark', label: 'Bark' },
    { value: 'mel', label: 'Mel' }
  ];

  return (
    <div className="space-y-4">
      <h2 className="text-xl font-semibold">Analysis Settings</h2>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <div>
          <label htmlFor="mode" className="block text-sm font-medium text-gray-700 mb-1">
            Mode:
          </label>
          <select
            id="mode"
            value={config.mode}
            onChange={(e) => onConfigChange({ mode: parseInt(e.target.value) })}
            className="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
          >
            {modeOptions.map((option) => (
              <option key={option.value} value={option.value}>
                {option.label}
              </option>
            ))}
          </select>
        </div>

        <div>
          <label htmlFor="frequency-scale" className="block text-sm font-medium text-gray-700 mb-1">
            Frequency Scale:
          </label>
          <select
            id="frequency-scale"
            value={config.frequency_scale}
            onChange={(e) => onConfigChange({ frequency_scale: e.target.value })}
            className="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
          >
            {scaleOptions.map((option) => (
              <option key={option.value} value={option.value}>
                {option.label}
              </option>
            ))}
          </select>
        </div>
      </div>

      <div className="space-y-3 mt-2">
        <div className="flex items-center">
          <input
            id="linear-amplitude"
            type="checkbox"
            checked={config.linear_amplitude}
            onChange={(e) => onConfigChange({ linear_amplitude: e.target.checked })}
            className="h-4 w-4 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
          />
          <label htmlFor="linear-amplitude" className="ml-2 block text-sm text-gray-700">
            Linear Amplitude (unchecked = Logarithmic dB)
          </label>
        </div>

        <div className="flex items-center">
          <input
            id="interpolate-bands"
            type="checkbox"
            checked={config.interpolate_bands}
            onChange={(e) => onConfigChange({ interpolate_bands: e.target.checked })}
            className="h-4 w-4 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
          />
          <label htmlFor="interpolate-bands" className="ml-2 block text-sm text-gray-700">
            Interpolate missing bands (for logarithmic analyzer)
          </label>
        </div>

        <div className="flex items-center">
          <input
            id="skip-non-processed"
            type="checkbox"
            checked={config.skip_non_processed}
            onChange={(e) => onConfigChange({ skip_non_processed: e.target.checked })}
            className="h-4 w-4 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
          />
          <label htmlFor="skip-non-processed" className="ml-2 block text-sm text-gray-700">
            Skip missing bands from output
          </label>
        </div>
      </div>
    </div>
  );
};

export default AnalysisSettings;
