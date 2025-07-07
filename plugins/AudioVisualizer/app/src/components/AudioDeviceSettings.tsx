import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { AudioVisualizerConfig } from '../App';

interface AudioDeviceInfo {
    id: string;
    name: string;
    is_default: boolean;
}

interface AudioDeviceSettingsProps {
    config: AudioVisualizerConfig;
    onConfigChange: (config: Partial<AudioVisualizerConfig>) => void;
    isRunning: boolean;
}

const AudioDeviceSettings: React.FC<AudioDeviceSettingsProps> = ({
    config,
    onConfigChange,
    isRunning
}) => {
    const [devices, setDevices] = useState<AudioDeviceInfo[]>([]);
    const [loading, setLoading] = useState(true);

    // Load available audio devices
    useEffect(() => {
        const loadDevices = async () => {
            try {
                setLoading(true);
                console.log("[DEBUG] AudioDeviceSettings: Loading audio output devices");
                const availableDevices = await invoke<AudioDeviceInfo[]>('get_audio_devices');
                console.log("[DEBUG] AudioDeviceSettings: Devices loaded", availableDevices);
                setDevices(availableDevices);
            } catch (error) {
                console.error("Failed to load audio devices:", error);
            } finally {
                setLoading(false);
            }
        };

        loadDevices();
    }, []);

    // Handle output device change
    const handleOutputDeviceChange = async (deviceId: string) => {
        console.log("[DEBUG] AudioDeviceSettings: Output device changed to", deviceId);

        // Update config
        onConfigChange({ selected_output_device: deviceId });

        // If we're running, also update the backend
        if (isRunning) {
            try {
                await invoke('set_audio_device', { deviceId });
                console.log("[DEBUG] AudioDeviceSettings: Backend updated with new output device");
            } catch (error) {
                console.error("Failed to update output device:", error);
            }
        }
    };

    return (
        <div className="space-y-4">
            <h2 className="text-xl font-semibold">Audio Device Settings</h2>

            {loading ? (
                <div className="text-gray-500">Loading audio devices...</div>
            ) : (
                <>
                    {/* Output Device Selection */}
                    <div>
                        <label htmlFor="output-device" className="block text-sm font-medium text-gray-700 mb-1">
                            Output Device:
                        </label>
                        <select
                            id="output-device"
                            value={config.selected_output_device || ''}
                            onChange={(e) => handleOutputDeviceChange(e.target.value)}
                            className="w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                            disabled={loading}
                        >
                            <option value="">Use System Default</option>
                            {devices.map((device) => (
                                <option key={device.id} value={device.id}>
                                    {device.name} {device.is_default ? " (Default)" : ""}
                                </option>
                            ))}
                        </select>
                        <p className="mt-1 text-sm text-gray-500">
                            Select the output device you want to visualize.
                        </p>
                    </div>
                </>
            )}
        </div>
    );
};

export default AudioDeviceSettings;
