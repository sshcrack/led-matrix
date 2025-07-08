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

    useEffect(() => {
        loadDevices();
    }, []);

    // Handle refresh devices
    const handleRefreshDevices = () => {
        loadDevices();
    };

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
            <div className="flex items-center justify-between">
                <h2 className="text-xl font-semibold">Audio Device Settings</h2>
                <button
                    onClick={handleRefreshDevices}
                    disabled={loading}
                    className="px-3 py-1 text-sm bg-blue-500 text-white rounded hover:bg-blue-600 disabled:opacity-50 disabled:cursor-not-allowed"
                >
                    {loading ? 'Refreshing...' : 'Refresh Devices'}
                </button>
            </div>

            {loading ? (
                <div className="text-gray-500">Loading audio devices...</div>
            ) : (
                <>
                    {/* Important notice about stopping/starting */}
                    <div className="bg-yellow-50 border border-yellow-200 rounded-md p-3 mb-4">
                        <div className="flex">
                            <div className="flex-shrink-0">
                                <svg className="h-5 w-5 text-yellow-400" viewBox="0 0 20 20" fill="currentColor">
                                    <path fillRule="evenodd" d="M8.257 3.099c.765-1.36 2.722-1.36 3.486 0l5.58 9.92c.75 1.334-.213 2.98-1.742 2.98H4.42c-1.53 0-2.493-1.646-1.743-2.98l5.58-9.92zM11 13a1 1 0 11-2 0 1 1 0 012 0zm-1-8a1 1 0 00-1 1v3a1 1 0 002 0V6a1 1 0 00-1-1z" clipRule="evenodd" />
                                </svg>
                            </div>
                            <div className="ml-3">
                                <p className="text-sm text-yellow-800">
                                    <strong>Note:</strong> After changing devices or refreshing the device list, you need to press <strong>Stop</strong> and then <strong>Start</strong> at the top to apply the new device selection.
                                </p>
                            </div>
                        </div>
                    </div>

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
