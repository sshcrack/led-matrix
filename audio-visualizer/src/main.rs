mod audio_processor;
mod config;
mod frequency_analysis;
mod network;
mod ui;

use anyhow::Result;
use eframe::egui;
use ui::AudioVisualizerApp;

#[tokio::main]
async fn main() -> Result<()> {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([800.0, 600.0])
            .with_title("Audio Visualizer"),
        ..Default::default()
    };

    eframe::run_native(
        "Audio Visualizer",
        options,
        Box::new(|cc| Ok(Box::new(AudioVisualizerApp::new(cc)))),
    )
    .unwrap();

    Ok(())
}
