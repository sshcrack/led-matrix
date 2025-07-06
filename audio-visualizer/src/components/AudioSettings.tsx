import React from 'react';
import { AudioVisualizerConfig } from '../App';

interface AudioSettingsProps {
  config: AudioVisualizerConfig;
  onConfigChange: (config: Partial<AudioVisualizerConfig>) => void;
}

const AudioSettings: React.FC<AudioSettingsProps> = ({
  config,
  onConfigChange,
}) => {
  return (
    <div className="space-y-4">
      <h2 className="text-xl font-semibold">Audio Settings</h2>
      
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        <div>
          <label htmlFor="num-bands" className="block text-sm font-medium text-gray-700 mb-1">
            Bands:
          </label>
          <div className="flex items-center">
            <input
              id="num-bands"
              type="range"
              min={8}
              max={256}
              step={1}
              value={config.num_bands}
              onChange={(e) => onConfigChange({ num_bands: parseInt(e.target.value) })}
              className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
            />
            <span className="ml-2 text-sm w-12 text-right">{config.num_bands}</span>
          </div>
        </div>
        
        <div>
          <label htmlFor="gain" className="block text-sm font-medium text-gray-700 mb-1">
            Gain:
          </label>
          <div className="flex items-center">
            <input
              id="gain"
              type="range"
              min={0.1}
              max={5.0}
              step={0.1}
              value={config.gain}
              onChange={(e) => onConfigChange({ gain: parseFloat(e.target.value) })}
              className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
            />
            <span className="ml-2 text-sm w-12 text-right">{config.gain.toFixed(1)}</span>
          </div>
        </div>
        
        <div>
          <label htmlFor="smoothing" className="block text-sm font-medium text-gray-700 mb-1">
            Smoothing:
          </label>
          <div className="flex items-center">
            <input
              id="smoothing"
              type="range"
              min={0}
              max={0.99}
              step={0.01}
              value={config.smoothing}
              onChange={(e) => onConfigChange({ smoothing: parseFloat(e.target.value) })}
              className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
            />
            <span className="ml-2 text-sm w-12 text-right">{config.smoothing.toFixed(2)}</span>
          </div>
        </div>
        
        <div>
          <label htmlFor="min-freq" className="block text-sm font-medium text-gray-700 mb-1">
            Min Frequency (Hz):
          </label>
          <div className="flex items-center">
            <input
              id="min-freq"
              type="range"
              min={20}
              max={1000}
              step={10}
              value={config.min_freq}
              onChange={(e) => onConfigChange({ min_freq: parseFloat(e.target.value) })}
              className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
            />
            <span className="ml-2 text-sm w-16 text-right">{config.min_freq.toFixed(0)} Hz</span>
          </div>
        </div>
        
        <div>
          <label htmlFor="max-freq" className="block text-sm font-medium text-gray-700 mb-1">
            Max Frequency (Hz):
          </label>
          <div className="flex items-center">
            <input
              id="max-freq"
              type="range"
              min={1000}
              max={22000}
              step={100}
              value={config.max_freq}
              onChange={(e) => onConfigChange({ max_freq: parseFloat(e.target.value) })}
              className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
            />
            <span className="ml-2 text-sm w-16 text-right">{config.max_freq.toFixed(0)} Hz</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default AudioSettings;
