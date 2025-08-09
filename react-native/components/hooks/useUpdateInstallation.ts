import { useState, useCallback, useRef } from 'react';
import { UpdateStatus, UpdateInstallResponse } from '../apiTypes/update';
import { useApiUrl } from '../apiUrl/ApiUrlProvider';

interface UseUpdateInstallationProps {
  onStatusUpdate?: (status: UpdateStatus) => void;
  onSuccess?: (version: string) => void;
  onError?: (error: string) => void;
}

export function useUpdateInstallation({
  onStatusUpdate,
  onSuccess,
  onError
}: UseUpdateInstallationProps = {}) {
  const [isInstalling, setIsInstalling] = useState(false);
  const [installProgress, setInstallProgress] = useState<string>('');
  const apiUrl = useApiUrl();
  const pollIntervalRef = useRef<number | null>(null);
  const expectedVersionRef = useRef<string>('');

  const stopPolling = useCallback(() => {
    if (pollIntervalRef.current) {
      clearInterval(pollIntervalRef.current);
      pollIntervalRef.current = null;
    }
  }, []);

  const pollUpdateStatus = useCallback(async (expectedVersion: string): Promise<boolean> => {
    try {
      // Create timeout signal manually for better compatibility
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 5000);
      
      const response = await fetch(`${apiUrl}/api/update/status`, {
        signal: controller.signal
      });
      
      clearTimeout(timeoutId);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }
      
      const status: UpdateStatus = await response.json();
      onStatusUpdate?.(status);
      
      // Check if update completed successfully by comparing versions
      if (status.current_version === expectedVersion) {
        setInstallProgress('Update completed successfully');
        onSuccess?.(expectedVersion);
        setIsInstalling(false);
        stopPolling();
        return true;
      }
      
      // Check status codes
      switch (status.status) {
        case 1: // CHECKING
          setInstallProgress('Checking for updates...');
          break;
        case 2: // DOWNLOADING
          setInstallProgress('Downloading update...');
          break;
        case 3: // INSTALLING  
          setInstallProgress('Installing update...');
          break;
        case 4: // ERROR
          const errorMsg = status.error_message || 'Update failed';
          setInstallProgress(`Error: ${errorMsg}`);
          onError?.(errorMsg);
          setIsInstalling(false);
          stopPolling();
          return true;
        case 5: // SUCCESS
          setInstallProgress('Update completed successfully');
          onSuccess?.(status.current_version);
          setIsInstalling(false);
          stopPolling();
          return true;
        case 0: // IDLE
        default:
          setInstallProgress('Waiting for update to start...');
          break;
      }
      
      return false; // Continue polling
    } catch (error) {
      // Handle network errors gracefully - service might be restarting
      if (error instanceof Error) {
        if (error.name === 'AbortError' || error.message.includes('Failed to fetch') || error.message.includes("Error when attempting")) {
          setInstallProgress('Service restarting... Please wait');
          return false; // Continue polling - service is likely restarting
        }
      }
      
      // Other errors should stop polling
      const errorMsg = error instanceof Error ? error.message : 'Network error';
      setInstallProgress(`Connection error: ${errorMsg}`);
      onError?.(errorMsg);
      setIsInstalling(false);
      stopPolling();
      return true;
    }
  }, [apiUrl, onStatusUpdate, onSuccess, onError, stopPolling]);

  const startInstallation = useCallback(async (version?: string): Promise<void> => {
    if (isInstalling) {
      return;
    }
    
    setIsInstalling(true);
    setInstallProgress('Starting update installation...');
    
    try {
      // Get current status to determine expected version
      const statusResponse = await fetch(`${apiUrl}/api/update/status`);
      const currentStatus: UpdateStatus = await statusResponse.json();
      
      const targetVersion = version || currentStatus.latest_version;
      expectedVersionRef.current = targetVersion;
      
      // Start the installation
      const url = version 
        ? `/api/update/install?version=${encodeURIComponent(version)}`
        : '/api/update/install';
      
      // Create timeout signal manually for better compatibility
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 10000);
      
      const response = await fetch(`${apiUrl}${url}`, {
        method: 'POST',
        signal: controller.signal
      });
      
      clearTimeout(timeoutId);
      
      // Handle the initial response
      if (response.ok) {
        const result: UpdateInstallResponse = await response.json();
        setInstallProgress(result.message);
        
        if (result.status === 'already_running') {
          setIsInstalling(false);
          onError?.('Update already in progress');
          return;
        }
      } else {
        const errorText = await response.text().catch(() => 'Unknown error');
        throw new Error(errorText);
      }
      
    } catch (error) {
      // Even if the initial request fails, we should start polling
      // because the update might have started successfully but the response was lost
      const errorMsg = error instanceof Error ? error.message : 'Failed to start update';
      setInstallProgress(`Started update (${errorMsg}) - polling for status...`);
    }
    
    // Start polling regardless of initial request result
    setInstallProgress('Monitoring update progress...');
    pollIntervalRef.current = setInterval(async () => {
      const completed = await pollUpdateStatus(expectedVersionRef.current);
      if (completed) {
        stopPolling();
      }
    }, 3000); // Poll every 3 seconds
    
    // Safety timeout - stop polling after 10 minutes
    setTimeout(() => {
      if (pollIntervalRef.current) {
        stopPolling();
        setInstallProgress('Update monitoring timeout - please check status manually');
        setIsInstalling(false);
        onError?.('Update monitoring timeout');
      }
    }, 600000); // 10 minutes
    
  }, [isInstalling, apiUrl, pollUpdateStatus, stopPolling, onError]);

  const cancelInstallation = useCallback(() => {
    stopPolling();
    setIsInstalling(false);
    setInstallProgress('');
    expectedVersionRef.current = '';
  }, [stopPolling]);

  return {
    isInstalling,
    installProgress,
    startInstallation,
    cancelInstallation
  };
}