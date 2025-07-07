use std::sync::Arc;

use tokio::sync::Mutex;

use crate::audio_processor::AudioProcessor;


#[derive(Default)]
pub struct AppState {
    pub is_running: bool,
    pub udp_sender: Option<crate::network::UdpSender>,
    pub audio_stream: Option<cpal::Stream>,
    pub audio_processor: Option<Arc<Mutex<AudioProcessor>>>,
    pub selected_output_device_id: Option<String>,
}

#[derive(serde::Serialize)]
pub struct AudioDeviceInfo {
    pub id: String,
    pub name: String,
    pub is_default: bool,
}