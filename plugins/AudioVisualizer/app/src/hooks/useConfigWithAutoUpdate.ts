import { useEffect, useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import { AudioVisualizerConfig } from '../App';
import useDebounce from './useDebounce';

// Debug logging helper with timestamp
const debug = (message: string, data?: any) => {
    const timestamp = new Date().toISOString().split('T')[1].slice(0, -1);
    console.log(`[${timestamp}] ${message}`, data ? data : '');
};

/**
 * Hook to manage configuration with debouncing for automatic updates
 * @param initialConfig The initial configuration
 * @param autoUpdateDelay Delay in ms before auto-updating the backend
 * @returns The configuration state and management functions
 */
export function useConfigWithAutoUpdate(
    initialConfig: AudioVisualizerConfig,
    autoUpdateDelay: number = 500
) {
    const [config, setConfig] = useState<AudioVisualizerConfig>(initialConfig);
    const [hasUnsavedChanges, setHasUnsavedChanges] = useState(false);
    const [isRunning, setIsRunning] = useState(false);
    const [connectionStatus, setConnectionStatus] = useState('Disconnected');

    // Create debounced version of the config
    const debouncedConfig = useDebounce(config, autoUpdateDelay);

    // Track whether the initial load has happened
    const [initialLoadDone, setInitialLoadDone] = useState(false);

    // Handle config changes
    const handleConfigChange = (updatedConfig: Partial<AudioVisualizerConfig>) => {
        debug('Config change requested', updatedConfig);
        setConfig((prev) => ({ ...prev, ...updatedConfig }));
        setHasUnsavedChanges(true);
    };

    // Save config to disk
    const saveConfig = async () => {
        debug('Saving configuration to disk');
        try {
            await invoke('save_config', { config });
            debug('Configuration saved successfully');
            setHasUnsavedChanges(false);
        } catch (error) {
            console.error("Failed to save config:", error);
        }
    };

    // Start visualization
    const startVisualization = async (configToUse = config) => {
        debug('Starting visualization with config', configToUse);
        try {
            const response = await invoke<string>('start_visualization', {
                hostname: configToUse.hostname,
                port: configToUse.port,
                config: configToUse,
            });

            debug('Visualization started successfully', response);
            setConnectionStatus(response);
            setIsRunning(true);
        } catch (error) {
            console.error("Failed to start visualization:", error);
            setConnectionStatus(`Error: ${error}`);
        }
    };

    // Stop visualization
    const stopVisualization = async () => {
        debug('Stopping visualization');
        try {
            await invoke('stop_visualization');
            debug('Visualization stopped successfully');
            setConnectionStatus('Disconnected');
            setIsRunning(false);
        } catch (error) {
            console.error("Failed to stop visualization:", error);
        }
    };

    // Listen for UDP errors
    useEffect(() => {
        const setupErrorListener = async () => {
            const unlisten = await listen<string>('udp-error', (event) => {
                console.error('UDP Error received:', event.payload);
                setConnectionStatus(`Error: ${event.payload}`);
                setIsRunning(false);
            });
            
            return unlisten;
        };

        let unlistenFn: (() => void) | null = null;
        
        setupErrorListener().then((unlisten) => {
            unlistenFn = unlisten;
        }).catch((error) => {
            console.error('Failed to setup UDP error listener:', error);
        });

        return () => {
            if (unlistenFn) {
                unlistenFn();
            }
        };
    }, []);

    // Load config on first mount
    useEffect(() => {
        debug('Loading initial configuration');
        invoke<AudioVisualizerConfig>('load_config')
            .then((loadedConfig) => {
                debug('Configuration loaded successfully', loadedConfig);
                setConfig(loadedConfig);
                setInitialLoadDone(true);

                // Auto-connect if hostname and port are properly configured
                if (loadedConfig.hostname && loadedConfig.port > 0) {
                    debug('Auto-connecting with loaded config');
                    startVisualization(loadedConfig);
                }
            })
            .catch((error) => {
                console.error("Failed to load config:", error);
                setInitialLoadDone(true);
            });
    }, []);

    // Update backend config when debounced config changes
    useEffect(() => {
        // Skip the first update which happens on mount
        if (!initialLoadDone) return;

        // Only update if already running
        if (isRunning) {
            debug('Sending debounced config update to backend', debouncedConfig);
            invoke('update_config', { config: debouncedConfig })
                .then(() => {
                    debug('Backend config updated successfully');

                    // If output device selection has changed, update the device
                    if (debouncedConfig.selected_output_device !== config.selected_output_device) {
                        debug('Updating output device selection in backend', debouncedConfig.selected_output_device);
                        invoke('set_audio_device', {
                            deviceId: debouncedConfig.selected_output_device || ''
                        }).catch(e => console.error("Failed to update output device:", e));
                    }
                })
                .catch((error) => {
                    console.error("Failed to update config:", error);
                });
        }
    }, [debouncedConfig, isRunning, initialLoadDone, config.selected_output_device]);

    return {
        config,
        hasUnsavedChanges,
        isRunning,
        connectionStatus,
        handleConfigChange,
        saveConfig,
        startVisualization,
        stopVisualization
    };
}

export default useConfigWithAutoUpdate;
