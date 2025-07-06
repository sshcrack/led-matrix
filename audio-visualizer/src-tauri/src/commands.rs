use crate::audio_processor::{AudioProcessor, AudioProcessorTrait, SAMPLE_RATE};
use crate::config::AudioVisualizerConfig;
use crate::network::{NetworkSender, UdpSender};
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::{Device, SampleRate, StreamConfig};
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex};
use tauri::{Manager, Window};
use tokio::time::Duration;
use std::collections::HashMap;

// Global state to hold audio processing components
static mut AUDIO_PROCESSOR: Option<Arc<Mutex<AudioProcessor>>> = None;
static mut AUDIO_STREAM: Option<cpal::Stream> = None;
static mut UDP_SENDER: Option<UdpSender> = None;
static mut IS_RUNNING: bool = false;

#[tauri::command]
pub fn get_audio_devices() -> Vec<String> {
    let host = cpal::default_host();
    let devices = host.input_devices().unwrap_or_default();
    
    devices
        .filter_map(|device| device.name().ok())
        .collect()
}

#[tauri::command]
pub fn start_visualization(hostname: String, port: u16, config: serde_json::Value) -> Result<String, String> {
    let config_obj: AudioVisualizerConfig = serde_json::from_value(config)
        .map_err(|e| format!("Failed to parse config: {}", e))?;
    
    unsafe {
        // Create audio processor if it doesn't exist
        if AUDIO_PROCESSOR.is_none() {
            AUDIO_PROCESSOR = Some(Arc::new(Mutex::new(AudioProcessor::new(config_obj.clone()))));
        } else if let Some(processor) = &AUDIO_PROCESSOR {
            if let Ok(mut p) = processor.lock() {
                p.update_config(config_obj.clone());
            }
        }
        
        // Set up UDP sender
        match UdpSender::new() {
            Ok(sender) => {
                UDP_SENDER = Some(sender);
            }
            Err(e) => {
                return Err(format!("Failed to create UDP socket: {}", e));
            }
        }
        
        // Set up audio stream if not already running
        if AUDIO_STREAM.is_none() {
            let host = cpal::default_host();
            let device = host.default_input_device().unwrap();
            
            let stream_config = StreamConfig {
                channels: 1,
                sample_rate: SampleRate(SAMPLE_RATE),
                buffer_size: cpal::BufferSize::Default,
            };
            
            let processor_clone = AUDIO_PROCESSOR.as_ref().unwrap().clone();
            
            let stream = device
                .build_input_stream(
                    &stream_config,
                    move |data: &[f32], _: &cpal::InputCallbackInfo| {
                        if let Ok(mut processor) = processor_clone.lock() {
                            processor.process_audio(data);
                        }
                    },
                    |err| eprintln!("Audio stream error: {}", err),
                    None,
                )
                .map_err(|e| format!("Failed to build audio stream: {}", e))?;
            
            AUDIO_STREAM = Some(stream);
        }
        
        // Start the stream
        if let Some(stream) = &AUDIO_STREAM {
            stream.play().map_err(|e| format!("Failed to start audio stream: {}", e))?;
        }
        
        // Start the UDP sender in a separate thread
        if let Some(sender) = &UDP_SENDER {
            let sender_clone = sender.try_clone().unwrap();
            let target_addr = format!("{}:{}", hostname, port);
            let processor_clone = AUDIO_PROCESSOR.as_ref().unwrap().clone();
            
            let interpolated = config_obj.interpolate_bands.clone();
            let frequency_scale = config_obj.frequency_scale.clone();
            
            tokio::spawn(async move {
                let mut interval = tokio::time::interval(Duration::from_millis(16)); // ~60 FPS
                
                loop {
                    interval.tick().await;
                    
                    let interpolated_log = interpolated.load(Ordering::Relaxed)
                        && *frequency_scale.read().unwrap() == "log";
                    
                    if let Ok(processor) = processor_clone.lock() {
                        let bands = processor.get_bands();
                        let _ = sender_clone.send_audio_data(bands, &target_addr, interpolated_log);
                    }
                }
            });
        }
        
        IS_RUNNING = true;
        
        Ok(format!("Connected to {}:{}", hostname, port))
    }
}

#[tauri::command]
pub fn stop_visualization() -> Result<(), String> {
    unsafe {
        if let Some(stream) = &AUDIO_STREAM {
            stream.pause().map_err(|e| format!("Failed to pause audio stream: {}", e))?;
        }
        
        UDP_SENDER = None;
        IS_RUNNING = false;
        
        Ok(())
    }
}

#[tauri::command]
pub fn get_audio_data() -> Vec<f32> {
    unsafe {
        if let Some(processor) = &AUDIO_PROCESSOR {
            if let Ok(p) = processor.lock() {
                return p.get_bands();
            }
        }
        vec![0.0; 64] // Default empty bands
    }
}

#[tauri::command]
pub fn save_config(config: serde_json::Value) -> Result<(), String> {
    let config_obj: AudioVisualizerConfig = serde_json::from_value(config)
        .map_err(|e| format!("Failed to parse config: {}", e))?;
    
    config_obj.save().map_err(|e| format!("Failed to save config: {}", e))
}

#[tauri::command]
pub fn load_config() -> Result<AudioVisualizerConfig, String> {
    AudioVisualizerConfig::load().map_err(|e| format!("Failed to load config: {}", e))
}

#[tauri::command]
pub fn toggle_window_visibility(window: Option<tauri::window::WebviewWindow>) -> Result<(), String> {
    let window = if let Some(win) = window {
        win
    } else {
        // Get the main window if none is provided
        tauri::AppHandle::get_global()
            .and_then(|handle| handle.get_webview_window("main"))
            .ok_or_else(|| "Failed to get main window".to_string())?
    };
    
    if window.is_visible().unwrap_or(true) {
        window.hide().map_err(|e| format!("Failed to hide window: {}", e))
    } else {
        window.show().map_err(|e| format!("Failed to show window: {}", e))?;
        window.set_focus().map_err(|e| format!("Failed to focus window: {}", e))
    }
}
