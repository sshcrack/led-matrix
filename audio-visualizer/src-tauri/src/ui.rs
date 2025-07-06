use crate::audio_processor::{AudioProcessor, AudioProcessorTrait, SAMPLE_RATE};
use crate::config::AudioVisualizerConfig;
use crate::network::{NetworkSender, UdpSender};
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::{SampleRate, StreamConfig};
use eframe::egui;
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex};
use tokio::time::Duration;
use tray_icon::{Icon, menu::{Menu, MenuEvent, MenuItem}, TrayIcon, TrayIconBuilder};
use std::thread;

pub trait AudioVisualizerUI {
    fn setup_audio(&mut self);
    fn start_visualization(&mut self);
    fn stop_visualization(&mut self);
    fn update_ui(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame);
}

pub struct AudioVisualizerApp {
    config: AudioVisualizerConfig,
    audio_processor: Arc<Mutex<dyn AudioProcessorTrait + Send>>,
    udp_sender: Option<UdpSender>,
    audio_stream: Option<cpal::Stream>,
    is_running: bool,
    connection_status: String,
    bands_display: Vec<f32>,
    has_unsaved_changes: bool,
    tray_icon: Option<TrayIcon>,
    pub window_visible: bool,
}

impl AudioVisualizerApp {
    pub fn new(_cc: &eframe::CreationContext<'_>) -> Self {
        let config = AudioVisualizerConfig::load().unwrap_or_default();
        let audio_processor: Arc<Mutex<dyn AudioProcessorTrait + Send>> =
            Arc::new(Mutex::new(AudioProcessor::new(config.clone())));

        // Check if app should start minimized
        let window_visible = !config.start_minimized_to_tray;
        
        // Create tray icon
        let tray_icon = Self::setup_tray_icon();

        let mut app = Self {
            config,
            audio_processor,
            udp_sender: None,
            audio_stream: None,
            is_running: false,
            connection_status: "Disconnected".to_string(),
            bands_display: vec![0.0; 64],
            has_unsaved_changes: false,
            tray_icon,
            window_visible,
        };

        app.setup_audio();

        // Auto-connect if hostname and port are properly configured
        if !app.config.hostname.is_empty() && app.config.port > 0 {
            app.start_visualization();
        }

        app
    }
    
    fn setup_tray_icon() -> Option<TrayIcon> {
        // Create a simple tray icon
        let icon_path = std::path::Path::new("assets/icon.ico");
        if !icon_path.exists() {
            return None;
        }
        
        let icon_data = std::fs::read(icon_path).ok()?;
        let icon = Icon::from_buffer(&icon_data).ok()?;
        
        // Create menu items
        let mut menu = Menu::new();
        let show_item = MenuItem::new("Show Window", true, None);
        let quit_item = MenuItem::new("Quit", true, None);
        
        menu.append(&show_item).ok()?;
        menu.append(&quit_item).ok()?;
        
        // Create tray icon
        let tray_icon = TrayIconBuilder::new()
            .with_menu(Box::new(menu))
            .with_tooltip("Audio Visualizer")
            .with_icon(icon)
            .build()
            .ok()?;
            
        // Handle menu events in a separate thread
        let show_id = show_item.id();
        let quit_id = quit_item.id();
        
        thread::spawn(move || {
            let event_receiver = MenuEvent::receiver();
            while let Ok(event) = event_receiver.recv() {
                if event.id == show_id {
                    // Request to show the window
                    if let Some(ctx) = egui::Context::get_shared() {
                        ctx.send_viewport_cmd(egui::ViewportCommand::Visible(true));
                        ctx.request_repaint();
                    }
                } else if event.id == quit_id {
                    // Exit the application
                    std::process::exit(0);
                }
            }
        });
        
        Some(tray_icon)
    }

    fn start_udp_sender(&self) {
        if let Some(sender) = &self.udp_sender {
            let sender = sender.try_clone().unwrap();
            let target_addr = format!("{}:{}", self.config.hostname, self.config.port);
            let processor = Arc::clone(&self.audio_processor);

            let interpolated = self.config.interpolate_bands.clone();
            let frequency_scale = self.config.frequency_scale.clone();
            tokio::spawn(async move {
                let mut interval = tokio::time::interval(Duration::from_millis(16)); // ~60 FPS

                loop {
                    interval.tick().await;

                    let interpolated_log = interpolated.load(Ordering::Relaxed)
                        && *frequency_scale.read().unwrap() == "log";
                    if let Ok(processor) = processor.lock() {
                        let bands = processor.get_bands();

                        let _ = sender.send_audio_data(bands, &target_addr, interpolated_log);
                    }
                }
            });
        }
    }

    pub fn toggle_window_visibility(&mut self) {
        self.window_visible = !self.window_visible;
    }
}

impl AudioVisualizerUI for AudioVisualizerApp {
    fn setup_audio(&mut self) {
        let host = cpal::default_host();
        let device = host.default_input_device().unwrap();

        let config = StreamConfig {
            channels: 1,
            sample_rate: SampleRate(SAMPLE_RATE),
            buffer_size: cpal::BufferSize::Default,
        };

        let processor = Arc::clone(&self.audio_processor);

        let stream = device
            .build_input_stream(
                &config,
                move |data: &[f32], _: &cpal::InputCallbackInfo| {
                    if let Ok(mut processor) = processor.lock() {
                        processor.process_audio(data);
                    }
                },
                |err| eprintln!("Audio stream error: {}", err),
                None,
            )
            .unwrap();

        self.audio_stream = Some(stream);
    }

    fn start_visualization(&mut self) {
        if let Ok(sender) = UdpSender::new() {
            self.udp_sender = Some(sender);
            self.is_running = true;
            self.connection_status =
                format!("Connected to {}:{}", self.config.hostname, self.config.port);

            if let Some(stream) = &self.audio_stream {
                stream.play().unwrap();
            }

            self.start_udp_sender();
        } else {
            self.connection_status = "Failed to create UDP socket".to_string();
        }
    }

    fn stop_visualization(&mut self) {
        self.is_running = false;
        self.connection_status = "Disconnected".to_string();
        self.udp_sender = None;

        if let Some(stream) = &self.audio_stream {
            stream.pause().unwrap();
        }
    }

    fn update_ui(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame) {
        // Update bands display
        if let Ok(processor) = self.audio_processor.lock() {
            self.bands_display = processor.get_bands();
        }

        // Handle window visibility
        if !self.window_visible {
            // If window should be hidden, hide it
            ctx.send_viewport_cmd(egui::ViewportCommand::Visible(false));
            // Still need to update the frame to keep running
            ctx.request_repaint();
            return;
        }
        
        // If we're here, the window should be visible
        ctx.send_viewport_cmd(egui::ViewportCommand::Visible(true));

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.heading("Audio Visualizer");
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if ui.button("Minimize to Tray").clicked() {
                        self.window_visible = false;
                    }
                });
            });
            ui.separator();

            self.render_connection_controls(ui);
            ui.separator();
            self.render_audio_settings(ui);
            ui.separator();
            self.render_analysis_settings(ui);
            ui.separator();
            self.render_startup_settings(ui);
            ui.separator();
            self.render_save_button(ui);
            ui.separator();
            self.render_visualizer(ui);
        });

        // Request repaint for smooth animation
        ctx.request_repaint();
    }
}

impl AudioVisualizerApp {
    fn render_connection_controls(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            ui.label("Hostname:");
            if ui.text_edit_singleline(&mut self.config.hostname).changed() {
                self.has_unsaved_changes = true;
            }
            ui.label("Port:");
            if ui.add(egui::DragValue::new(&mut self.config.port).range(1..=65535)).changed() {
                self.has_unsaved_changes = true;
            }
        });

        ui.horizontal(|ui| {
            if ui
                .button(if self.is_running { "Stop" } else { "Start" })
                .clicked()
            {
                if self.is_running {
                    self.stop_visualization();
                } else {
                    self.start_visualization();
                }
            }
            ui.label(format!("Status: {}", self.connection_status));
        });
    }

    fn render_audio_settings(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            ui.label("Bands:");
            if ui
                .add(egui::DragValue::new(&mut self.config.num_bands).range(8..=256))
                .changed()
            {
                self.update_processor_config();
            }
        });

        ui.horizontal(|ui| {
            ui.label("Gain:");
            if ui
                .add(egui::Slider::new(&mut self.config.gain, 0.1..=5.0))
                .changed()
            {
                self.update_processor_config();
            }
        });

        ui.horizontal(|ui| {
            ui.label("Smoothing:");
            if ui
                .add(egui::Slider::new(&mut self.config.smoothing, 0.0..=0.99))
                .changed()
            {
                self.update_processor_config();
            }
        });

        ui.horizontal(|ui| {
            ui.label("Min Frequency:");
            if ui
                .add(egui::DragValue::new(&mut self.config.min_freq).range(20.0..=1000.0))
                .changed()
            {
                self.update_processor_config();
            }
            ui.label("Hz");
        });

        ui.horizontal(|ui| {
            ui.label("Max Frequency:");
            if ui
                .add(egui::DragValue::new(&mut self.config.max_freq).range(1000.0..=22000.0))
                .changed()
            {
                self.update_processor_config();
            }
            ui.label("Hz");
        });
    }

    fn render_analysis_settings(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            ui.label("Mode:");
            let mode_names = [
                "Discrete Frequencies",
                "1/3 Octave Bands",
                "Full Octave Bands",
            ];
            let mut selected_mode = self.config.mode as usize;

            egui::ComboBox::from_id_source("mode_combo")
                .selected_text(
                    mode_names
                        .get(selected_mode)
                        .unwrap_or(&"Unknown")
                        .to_string(),
                )
                .show_ui(ui, |ui| {
                    let mut changed = false;
                    for (i, name) in mode_names.iter().enumerate() {
                        if ui.selectable_value(&mut selected_mode, i, *name).changed() {
                            changed = true;
                        }
                    }
                    if changed {
                        self.config.mode = selected_mode as u8;
                        self.update_processor_config();
                    }
                });
        });

        ui.horizontal(|ui| {
            ui.label("Frequency Scale:");
            let scale_names = ["Logarithmic", "Linear", "Bark", "Mel"];
            let scale_values = ["log", "linear", "bark", "mel"];
            let current_scale_index = scale_values
                .iter()
                .position(|&s| s == self.config.frequency_scale.read().unwrap().as_str())
                .unwrap_or(0);
            let mut selected_scale = current_scale_index;

            egui::ComboBox::from_id_source("freq_scale_combo")
                .selected_text(scale_names[current_scale_index])
                .show_ui(ui, |ui| {
                    let mut changed = false;
                    for (i, name) in scale_names.iter().enumerate() {
                        if ui.selectable_value(&mut selected_scale, i, *name).changed() {
                            changed = true;
                        }
                    }
                    if changed {
                        *self.config.frequency_scale.write().unwrap() =
                            scale_values[selected_scale].to_string();
                        self.update_processor_config();
                    }
                });
        });

        ui.horizontal(|ui| {
            ui.label("Amplitude Scale:");
            let mut amplitude_changed = false;
            if ui
                .checkbox(&mut self.config.linear_amplitude, "Linear")
                .changed()
            {
                amplitude_changed = true;
            }
            ui.label("(unchecked = Logarithmic dB)");

            if amplitude_changed {
                self.update_processor_config();
            }
        });

        ui.horizontal(|ui| {
            ui.label("Band Interpolation:");
            let mut interpolate = self.config.interpolate_bands.load(Ordering::Relaxed);
            if ui
                .checkbox(&mut interpolate, "Interpolate missing bands")
                .changed()
            {
                self.config
                    .interpolate_bands
                    .store(interpolate, Ordering::Relaxed);
                self.update_processor_config();
            }
            ui.label("(for logarithmic analyzer)");
        });

        ui.horizontal(|ui| {
            ui.label("Skip missing bands from output:");
            if ui
                .checkbox(&mut self.config.skip_non_processed, "Skip missing bands")
                .changed()
            {
                self.update_processor_config();
            }
        });
    }

    fn render_startup_settings(&mut self, ui: &mut egui::Ui) {
        ui.heading("Startup Settings");
        
        ui.horizontal(|ui| {
            if ui.checkbox(&mut self.config.start_minimized_to_tray, "Start minimized to tray").changed() {
                self.has_unsaved_changes = true;
            }
            
            ui.tooltip_text("When enabled, the application will start minimized to the system tray");
        });
        
        ui.horizontal(|ui| {
            if ui.checkbox(&mut self.config.autostart_enabled, "Start with system").changed() {
                self.has_unsaved_changes = true;
            }
            
            ui.tooltip_text("When enabled, the application will start automatically when you log in");
        });
    }

    fn render_save_button(&mut self, ui: &mut egui::Ui) {
        let button_text = "Save Settings";
        let button = if self.has_unsaved_changes {
            // Use a different color for the button when there are unsaved changes
            egui::Button::new(button_text)
                .fill(egui::Color32::from_rgb(100, 200, 100))
        } else {
            egui::Button::new(button_text)
        };

        if ui.add(button).clicked() {
            if let Err(e) = self.config.save() {
                eprintln!("Failed to save config: {}", e);
            } else {
                // Reset the flag when successfully saved
                self.has_unsaved_changes = false;
            }
        }
    }

    fn render_visualizer(&mut self, ui: &mut egui::Ui) {
        ui.label("Audio Spectrum:");
        ui.add_space(10.0);

        let available_width = ui.available_width();
        let bar_width = available_width / self.bands_display.len() as f32;
        ui.add_space(180.0);

        ui.allocate_ui_with_layout(
            egui::Vec2::new(available_width, 200.0),
            egui::Layout::left_to_right(egui::Align::Min),
            |ui| {
                for (i, &amplitude) in self.bands_display.iter().enumerate() {
                    let bar_height = amplitude * 180.0;
                    let rect = egui::Rect::from_min_size(
                        egui::Pos2::new(i as f32 * bar_width, 580.0 - bar_height),
                        egui::Vec2::new(bar_width, bar_height),
                    );

                    let color = if amplitude > 0.8 {
                        egui::Color32::RED
                    } else if amplitude > 0.5 {
                        egui::Color32::YELLOW
                    } else {
                        egui::Color32::GREEN
                    };

                    ui.painter().rect_filled(rect, 0.0, color);
                }
            },
        );
    }

    fn update_processor_config(&mut self) {
        if let Ok(mut processor) = self.audio_processor.lock() {
            processor.update_config(self.config.clone());
            self.has_unsaved_changes = true;
        }
    }
}

impl eframe::App for AudioVisualizerApp {
    fn update(&mut self, ctx: &egui::Context, frame: &mut eframe::Frame) {
        self.update_ui(ctx, frame);
    }
    
    fn on_close_event(&mut self) -> bool {
        // If we have a tray icon, just hide the window instead of closing the app
        if self.tray_icon.is_some() {
            self.window_visible = false;
            return false; // Don't close the app
        }
        true // Close the app if no tray icon
    }
}
