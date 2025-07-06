import React from 'react';
import { AudioVisualizerConfig } from '../App';

interface StartupSettingsProps {
  config: AudioVisualizerConfig;
  onConfigChange: (config: Partial<AudioVisualizerConfig>) => void;
}

const StartupSettings: React.FC<StartupSettingsProps> = ({
  config,
  onConfigChange,
}) => {
  return (
    <div className="space-y-4">
      <h2 className="text-xl font-semibold">Startup Settings</h2>
      
      <div className="space-y-3">
        <div className="flex items-center">
          <input
            id="start-minimized"
            type="checkbox"
            checked={config.start_minimized_to_tray}
            onChange={(e) => onConfigChange({ start_minimized_to_tray: e.target.checked })}
            className="h-4 w-4 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
          />
          <label htmlFor="start-minimized" className="ml-2 block text-sm text-gray-700">
            Start minimized to tray
          </label>
        </div>
        
        <div className="flex items-center">
          <input
            id="autostart"
            type="checkbox"
            checked={config.autostart_enabled}
            onChange={(e) => onConfigChange({ autostart_enabled: e.target.checked })}
            className="h-4 w-4 text-blue-600 focus:ring-blue-500 border-gray-300 rounded"
          />
          <label htmlFor="autostart" className="ml-2 block text-sm text-gray-700">
            Start with system
          </label>
        </div>
      </div>
      
      <div className="bg-blue-50 text-blue-800 p-3 rounded-md text-sm mt-2">
        <p>
          <strong>Note:</strong> When "Start minimized to tray" is enabled, 
          the application will start hidden in the system tray. 
          Click the tray icon to show the window.
        </p>
      </div>
    </div>
  );
};

export default StartupSettings;
