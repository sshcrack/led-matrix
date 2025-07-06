import React, { useEffect, useRef } from 'react';

interface VisualizerProps {
  bands: number[];
}

const Visualizer: React.FC<VisualizerProps> = ({ bands }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  
  // Draw the visualizer on canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    
    // Clear the canvas
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    // Calculate bar width based on the number of bands
    const barWidth = canvas.width / bands.length;
    
    // Draw each band
    bands.forEach((amplitude, i) => {
      const barHeight = amplitude * canvas.height;
      
      // Determine color based on amplitude (similar to original egui app)
      let color;
      if (amplitude > 0.8) {
        color = '#ef4444'; // red-500
      } else if (amplitude > 0.5) {
        color = '#eab308'; // yellow-500
      } else {
        color = '#22c55e'; // green-500
      }
      
      // Draw the bar
      ctx.fillStyle = color;
      ctx.fillRect(
        i * barWidth,
        canvas.height - barHeight,
        barWidth - 1, // Leave a small gap between bars
        barHeight
      );
    });
  }, [bands]);
  
  return (
    <div className="space-y-2">
      <h2 className="text-xl font-semibold">Audio Spectrum</h2>
      
      <div className="border border-gray-300 rounded-md bg-black p-2 h-48">
        <canvas 
          ref={canvasRef}
          width={800}
          height={180}
          className="w-full h-full"
        />
      </div>
    </div>
  );
};

export default Visualizer;
