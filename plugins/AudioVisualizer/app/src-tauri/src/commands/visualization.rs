use crate::audio_processor::{AudioProcessor, AudioProcessorTrait};
use crate::config::AudioVisualizerConfig;
use crate::network::{NetworkSender, UdpSender};
use crate::state::AppState;
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::Device;
use std::sync::Arc;
use tauri::State;
use tokio::sync::Mutex;
use tokio::time::Duration;

/// Update the audio processor with new configuration
async fn update_audio_processor(
    processor: &Arc<Mutex<AudioProcessor>>,
    config: AudioVisualizerConfig,
) {
    println!("[DEBUG] Updating existing audio processor");
    let mut p = processor.lock().await;
    p.update_config(config);
}

/// Create a new audio processor if needed
fn create_audio_processor(state: &mut AppState, config: AudioVisualizerConfig, sample_rate: u32) {
    if state.audio_processor.is_none() {
        println!("[DEBUG] Creating new audio processor");
        state.audio_processor = Some(Arc::new(Mutex::new(AudioProcessor::new(
            config,
            sample_rate,
        ))));
    }
}

/// Set up the UDP sender thread
fn setup_udp_sender_thread(
    sender: &UdpSender,
    hostname: &str,
    port: u16,
    processor: Arc<Mutex<AudioProcessor>>,
    output_device_id: Option<String>,
) {
    let sender_clone = sender.try_clone().unwrap();
    let target_addr = format!("{}:{}", hostname, port);
    let processor_clone = processor.clone();
    let output_device_id_clone = output_device_id.clone();

    tokio::spawn(async move {
        println!(
            "[DEBUG] UDP sender thread: Started with target {} for output device {:?}",
            target_addr, output_device_id_clone
        );
        let mut interval = tokio::time::interval(Duration::from_millis(16)); // ~60 FPS

        loop {
            interval.tick().await;

            let processor = processor_clone.lock().await;
            let bands = processor.get_bands();

            // Get current settings from processor
            let interpolated_log = processor.get_interpolated_log();

            match sender_clone.send_audio_data(bands, &target_addr, interpolated_log) {
                Ok(_) => {}
                Err(e) => println!("[ERROR] UDP sender thread: Failed to send data: {}", e),
            }
        }
    });
    println!("[DEBUG] UDP sender thread spawned");
}

fn get_selected_device(state: &AppState) -> Result<Device, String> {
    let host = cpal::default_host();
    let mut device = host.default_output_device();

    // Determine which device to use for audio capture
    if let Some(output_device_id) = &state.selected_output_device_id {
        println!(
            "[DEBUG] Looking for selected output device: {}",
            output_device_id
        );

        // Find the device with the matching name
        let output_devices = host.output_devices().map_err(|e| {
            println!("[ERROR] Failed to get output devices: {}", e);
            format!("Failed to get output devices: {}", e)
        })?;

        device.replace(
            output_devices
                .filter(|d| d.name().map(|n| &n == output_device_id).unwrap_or(false))
                .next()
                .or_else(|| host.default_output_device())
                .ok_or_else(|| {
                    println!(
                        "[ERROR] No output device found with name {}",
                        output_device_id
                    );
                    format!("No output device found with name {}", output_device_id)
                })?,
        );
    }

    let device = device.ok_or_else(|| {
        println!("[ERROR] No output device found");
        format!("No output device found")
    })?;

    println!(
        "[DEBUG] Using output device: {}",
        device.name().unwrap_or_default()
    );

    Ok(device)
}

/// Set up the audio capture stream
fn setup_audio_stream(
    state: &mut AppState,
    device: Device,
    processor: Arc<Mutex<AudioProcessor>>,
) -> Result<(), String> {
    let config = device.default_output_config().map_err(|e| {
        println!("[ERROR] Failed to get default output config: {}", e);
        format!("Failed to get default output config: {}", e)
    })?;

    let processor_clone = processor.clone();

    let stream = device
        .build_input_stream(
            &config.config(),
            move |data: &[f32], _: &cpal::InputCallbackInfo| {
                let mut processor = processor_clone.blocking_lock();
                processor.process_audio(data);
            },
            |err| eprintln!("[ERROR] Audio stream error: {}", err),
            None,
        )
        .map_err(|e| {
            println!("[ERROR] Failed to build audio stream: {}", e);
            format!("Failed to build audio stream: {}", e)
        })?;

    state.audio_stream = Some(stream);
    println!("[DEBUG] Audio stream created successfully");
    Ok(())
}

/// Start the audio visualization process
#[tauri::command]
pub async fn start_visualization(
    state: State<'_, Mutex<AppState>>,
    hostname: String,
    port: u16,
    config: AudioVisualizerConfig,
) -> Result<String, String> {
    println!(
        "[DEBUG] start_visualization: Starting with host={}, port={}",
        hostname, port
    );
    let mut state = state.lock().await;

    // Don't recreate the sender if we're already running
    let create_sender = state.udp_sender.is_none();
    println!(
        "[DEBUG] start_visualization: create_sender={}",
        create_sender
    );

    let device = get_selected_device(&state)?;
    let stream_cfg = device.default_output_config().map_err(|e| {
        println!(
            "[ERROR] start_visualization: Failed to get default output config: {}",
            e
        );
        format!("Failed to get default output config: {}", e)
    })?;

    if let Some(processor) = state.audio_processor.as_ref() {
        update_audio_processor(processor, config.clone()).await;
    } else {
        create_audio_processor(&mut state, config.clone(), stream_cfg.sample_rate().0);
    }

    // Update selected output device ID from config
    if let Some(output_device) = &config.selected_output_device {
        println!(
            "[DEBUG] start_visualization: Setting output device from config: {}",
            output_device
        );
        state.selected_output_device_id = Some(output_device.clone());
    }

    // Set up UDP sender if it doesn't exist
    if create_sender {
        println!("[DEBUG] start_visualization: Creating new UDP sender");
        match UdpSender::new() {
            Ok(sender) => {
                state.udp_sender = Some(sender);
                println!("[DEBUG] start_visualization: UDP sender created successfully");
            }
            Err(e) => {
                println!(
                    "[ERROR] start_visualization: Failed to create UDP socket: {}",
                    e
                );
                return Err(format!("Failed to create UDP socket: {}", e));
            }
        }
    } else {
        println!("[DEBUG] start_visualization: Reusing existing UDP sender");
    }

    // Set up audio stream if not already running
    if state.audio_stream.is_none() {
        println!("[DEBUG] start_visualization: Creating new audio stream");
        let processor = state.audio_processor.as_ref().unwrap().clone();
        setup_audio_stream(&mut state, device, processor)?;
    } else {
        println!("[DEBUG] start_visualization: Reusing existing audio stream");
    }

    // Start the stream
    if let Some(stream) = &state.audio_stream {
        println!("[DEBUG] start_visualization: Starting audio stream");
        stream.play().map_err(|e| {
            println!(
                "[ERROR] start_visualization: Failed to start audio stream: {}",
                e
            );
            format!("Failed to start audio stream: {}", e)
        })?;
        println!("[DEBUG] start_visualization: Audio stream started successfully");
    }

    // Start the UDP sender in a separate thread if it's newly created
    if create_sender {
        if let Some(sender) = &state.udp_sender {
            println!("[DEBUG] start_visualization: Setting up UDP sender thread");
            let processor = state.audio_processor.as_ref().unwrap().clone();
            setup_udp_sender_thread(
                sender,
                &hostname,
                port,
                processor,
                state.selected_output_device_id.clone(),
            );

            // Log the output device being used for visualization
            if let Some(device_id) = &state.selected_output_device_id {
                println!(
                    "[DEBUG] start_visualization: Visualization configured for output device: {}",
                    device_id
                );
            } else {
                println!("[DEBUG] start_visualization: Visualization using default output device");
            }
        }
    }

    state.is_running = true;
    println!("[DEBUG] start_visualization: Visualization started successfully");

    Ok(format!("Connected to {}:{}", hostname, port))
}

/// Stop the audio visualization process
#[tauri::command]
pub async fn stop_visualization(state: State<'_, Mutex<AppState>>) -> Result<(), String> {
    println!("[DEBUG] stop_visualization: Stopping visualization");
    let mut state = state.lock().await;

    if let Some(stream) = &state.audio_stream {
        println!("[DEBUG] stop_visualization: Pausing audio stream");
        stream.pause().map_err(|e| {
            println!(
                "[ERROR] stop_visualization: Failed to pause audio stream: {}",
                e
            );
            format!("Failed to pause audio stream: {}", e)
        })?;
        println!("[DEBUG] stop_visualization: Audio stream paused successfully");
    }

    println!("[DEBUG] stop_visualization: Clearing UDP sender");
    state.udp_sender = None;
    state.is_running = false;
    println!("[DEBUG] stop_visualization: Visualization stopped successfully");

    Ok(())
}

/// Update the configuration while the visualization is running
#[tauri::command]
pub async fn update_config(
    state: State<'_, Mutex<AppState>>,
    config: AudioVisualizerConfig,
) -> Result<(), String> {
    println!("[DEBUG] update_config: Received config update");
    let mut state = state.lock().await;

    // Check if output device has changed
    let output_device_changed = match (
        &state.selected_output_device_id,
        &config.selected_output_device,
    ) {
        (Some(current), Some(new)) => current != new,
        (None, Some(_)) => true,
        (Some(_), None) => true,
        _ => false,
    };

    if output_device_changed {
        println!(
            "[DEBUG] update_config: Output device changed to {:?}",
            config.selected_output_device
        );
        if let Some(device_id) = &config.selected_output_device {
            state.selected_output_device_id = Some(device_id.clone());
        } else {
            state.selected_output_device_id = None;
        }
    }

    // Update audio processor config if it exists
    if let Some(processor) = state.audio_processor.as_ref() {
        println!("[DEBUG] update_config: Updating audio processor config");
        update_audio_processor(processor, config.clone()).await;
        println!("[DEBUG] update_config: Audio processor config updated successfully");
    } else {
        println!("[DEBUG] update_config: No audio processor found to update");
    }

    Ok(())
}
