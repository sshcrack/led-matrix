mod audio_processor;
mod config;
mod frequency_analyzer;
mod network;
mod ui;
mod generate_icon;

use anyhow::Result;
use eframe::egui;
use ui::AudioVisualizerApp;
use std::path::Path;
use std::env;

#[tokio::main]
async fn main() -> Result<()> {
    // Check for command line arguments
    let args: Vec<String> = env::args().collect();
    let start_minimized = args.iter().any(|arg| arg == "--minimized");
    
    // Generate icon if it doesn't exist
    let icon_path = Path::new("assets/icon.ico");
    if !icon_path.exists() {
        generate_icon::generate_icon()?;
    }
    
    // Create the viewport builder with the desired settings
    let viewport = egui::ViewportBuilder::default()
        .with_inner_size([800.0, 600.0])
        .with_title("Audio Visualizer")
        .with_visible(!start_minimized);
    
    let options = eframe::NativeOptions {
        viewport,
        ..Default::default()
    };

    eframe::run_native(
        "Audio Visualizer",
        options,
        Box::new(|cc| {
            // Pass command line minimized flag to app
            let mut app = AudioVisualizerApp::new(cc);
            if start_minimized {
                app.window_visible = false;
            }
            Ok(Box::new(app))
        }),
    )
    .unwrap();

    Ok(())
}
