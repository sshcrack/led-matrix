use crate::state::AppState;
use crate::{audio_processor::AudioProcessorTrait, state::AudioDeviceInfo};
use cpal::traits::{DeviceTrait, HostTrait};
use tauri::State;
use tokio::sync::Mutex;

/// Get a list of available audio output devices
#[tauri::command]
pub fn get_audio_devices() -> Vec<AudioDeviceInfo> {
    println!("[DEBUG] get_audio_devices: Retrieving available audio output devices");
    let host = cpal::default_host();
    let mut devices = Vec::new();

    // Get default output device
    let default_output = host.default_output_device();

    // Get all output devices
    if let Ok(output_devices) = host.output_devices() {
        for device in output_devices {
            if let Ok(device_name) = device.name() {
                let id = device_name.clone();
                let is_default = default_output
                    .as_ref()
                    .map(|d| d.name().map(|n| n == device_name).unwrap_or(false))
                    .unwrap_or(false);

                println!(
                    "[DEBUG] get_audio_devices: Found output device: {} (default: {})",
                    device_name, is_default
                );

                devices.push(AudioDeviceInfo {
                    id,
                    name: format!("{} (Output)", device_name),
                    is_default,
                });
            }
        }
    }

    println!(
        "[DEBUG] get_audio_devices: Found {} output devices total",
        devices.len()
    );
    devices
}

/// Get the most recent audio data from the processor
#[tauri::command]
pub async fn get_audio_data(state: State<'_, Mutex<AppState>>) -> Result<Vec<f32>, String> {
    // Only log occasionally to avoid flooding the console
    static mut COUNTER: u32 = 0;
    let should_log = unsafe {
        COUNTER += 1;
        COUNTER % 300 == 0 // Log every 300th call
    };

    if should_log {
        println!("[DEBUG] get_audio_data: Retrieving audio data");
    }

    let s = state.lock().await;
    if let Some(processor) = &s.audio_processor {
        let p = processor.lock().await;
        let bands = p.get_bands();

        if should_log && !bands.is_empty() {
            println!("[DEBUG] get_audio_data: Retrieved {} bands", bands.len());
            // Log a few band values as sample
            println!(
                "[DEBUG] get_audio_data: Band samples - first: {}, middle: {}, last: {}",
                bands[0],
                bands[bands.len() / 2],
                bands[bands.len() - 1]
            );
        }

        return Ok(bands);
    }

    if should_log {
        println!("[DEBUG] get_audio_data: No audio processor, returning empty bands");
    }

    Ok(vec![0.0; 64]) // Default empty bands
}

/// Set the audio device to use for capturing
#[tauri::command]
pub async fn set_audio_device(
    state: State<'_, Mutex<AppState>>,
    device_id: String,
) -> Result<(), String> {
    println!(
        "[DEBUG] set_audio_device: Setting output device to {}",
        device_id
    );

    let mut state = state.lock().await;

    // Update the selected output device ID
    state.selected_output_device_id = Some(device_id);

    println!("[DEBUG] set_audio_device: Output device selection updated successfully");
    Ok(())
}
