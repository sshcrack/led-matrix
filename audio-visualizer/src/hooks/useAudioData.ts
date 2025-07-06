import { useEffect, useState } from 'react';
import { invoke } from '@tauri-apps/api/core';

export function useAudioData(isRunning: boolean, fps: number = 60) {
  const [bands, setBands] = useState<number[]>(Array(64).fill(0));
  
  useEffect(() => {
    if (!isRunning) return;
    
    const interval = 1000 / fps; // Convert FPS to milliseconds
    const intervalId = setInterval(async () => {
      try {
        const data = await invoke<number[]>('get_audio_data');
        setBands(data);
      } catch (error) {
        console.error('Failed to get audio data:', error);
      }
    }, interval);
    
    return () => clearInterval(intervalId);
  }, [isRunning, fps]);
  
  return bands;
}

export default useAudioData;
