import React from 'react';
import { AudioVisualizerConfig } from '../App';

interface ConnectionControlsProps {
  hostname: string;
  port: number;
  isRunning: boolean;
  connectionStatus: string;
  onConfigChange: (config: Partial<AudioVisualizerConfig>) => void;
  onStart: () => void;
  onStop: () => void;
}

const ConnectionControls: React.FC<ConnectionControlsProps> = ({
  hostname,
  port,
  isRunning,
  connectionStatus,
  onConfigChange,
  onStart,
  onStop,
}) => {
  return (
    <div className="space-y-4">
      <h2 className="text-xl font-semibold">Connection Settings</h2>

      <div className="flex flex-wrap gap-4 items-center">
        <div className="w-full sm:w-auto flex-1">
          <label htmlFor="hostname" className="block text-sm font-medium text-gray-700 mb-1">
            Hostname:
          </label>
          <input
            id="hostname"
            type="text"
            value={hostname}
            onChange={(e) => onConfigChange({ hostname: e.target.value })}
            className="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
          />
        </div>

        <div className="w-full sm:w-32">
          <label htmlFor="port" className="block text-sm font-medium text-gray-700 mb-1">
            Port:
          </label>
          <input
            id="port"
            type="number"
            min={1}
            max={65535}
            value={port}
            onChange={(e) => onConfigChange({ port: parseInt(e.target.value) || 0 })}
            className="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
          />
        </div>
      </div>

      <div className="flex items-center gap-4">
        <button
          onClick={isRunning ? () => onStop() : () => onStart()}
          className={`px-4 py-2 rounded-md text-white font-medium cursor-pointer transition-colors ${isRunning
              ? 'bg-red-500 hover:bg-red-600'
              : 'bg-green-500 hover:bg-green-600'
            }`}
        >
          {isRunning ? 'Stop' : 'Start'}
        </button>

        <div className="text-sm">
          Status: <span className={isRunning ? 'text-green-600 font-medium' : 'text-red-600 font-medium'}>
            {connectionStatus}
          </span>
        </div>
      </div>
    </div>
  );
};

export default ConnectionControls;
