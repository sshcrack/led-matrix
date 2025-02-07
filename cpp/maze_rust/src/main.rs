#![cfg_attr(
    not(any(debug_assertions, feature = "show_console")),
    windows_subsystem = "windows"
)] // hide console window on Windows in release

use std::{
    path::PathBuf,
    sync::{Arc, RwLock},
};

use eframe::{App, Theme};
use egui::*;
use im_native_dialog::ImNativeFileDialog;
use manager::MazeThread;
use solve::solve::SolveAlgorithm;
use tools::{
    consts::MazeOptions,
    options::{AnimOptions, MazeData},
};

use crate::{
    point::point::Point,
    tools::math::{numb_to_vec2, vec2_to_numb},
};

mod generators;
mod manager;
mod point;
mod solve;
#[cfg(test)]
mod tests;
mod tools;

const ICON: &[u8; 324] = include_bytes!("./assets/icon.png");
fn main() -> Result<(), eframe::Error> {
    let options = eframe::NativeOptions {
        icon_data: Some(eframe::IconData {
            rgba: ICON.to_vec(),
            width: 32,
            height: 32,
        }),
        ..Default::default()
    };
    eframe::run_native(
        "Maze Solver",
        options,
        Box::new(|cc| Box::new(MyApp::new(&cc.egui_ctx))),
    )
}

struct MyApp {
    pixels: Arc<RwLock<Vec<Color32>>>,
    maze_img: Option<TextureHandle>,
    curr: Option<MazeThread>,

    seed_random: bool,
    seed: u64,
    seed_text: String,

    solve_algorithm: SolveAlgorithm,
    size: usize,
    size_text: String,

    show_animation: bool,

    speed: f64,
    speed_text: String,

    show_debug: bool,

    save_path: PathBuf,
    save_file_dialog: ImNativeFileDialog<Option<PathBuf>>,

    decimate: usize,
}

impl MyApp {
    fn new(ctx: &Context) -> Self {
        let pixels = Arc::new(RwLock::new(Vec::new()));
        let rand_seed = rand::random();

        let size = 50;
        let mut e = Self {
            pixels,
            maze_img: None,
            seed: rand_seed,
            seed_text: rand_seed.to_string(),
            seed_random: true,
            curr: None,
            solve_algorithm: SolveAlgorithm::AStar,

            size,
            size_text: size.to_string(),

            show_animation: true,

            speed: 0.975,
            speed_text: "1.0".to_string(),
            show_debug: true,

            save_path: Default::default(),
            save_file_dialog: Default::default(),
            decimate: 2,
        };

        e.curr = Some(e.start_generating(ctx));
        e
    }

    fn start_generating(&self, ctx: &Context) -> MazeThread {
        let data = MazeData::new(
            ctx,
            &self.pixels,
            &MazeOptions::new(self.size, self.seed, self.decimate),
            &AnimOptions::new(
                self.show_debug,
                self.show_animation,
                Self::get_speed_limited(self.size, self.speed),
            ),
        );

        MazeThread::new(&data, self.solve_algorithm)
    }
}

impl MyApp {
    fn add_image(&mut self, ctx: &Context, ui: &mut Ui) {
        let left = ui.available_size_before_wrap();
        let size_left = left.min_elem() as usize;
        if self.curr.is_none() {
            return;
        }

        let thread = self.curr.as_ref().unwrap();
        let maze_dim = thread.get_options().size;
        if maze_dim == 0 || size_left == 0 {
            return;
        }

        let scale = (size_left as f64 / maze_dim as f64).floor() as usize;

        let mut img = ColorImage::new([size_left, size_left], Color32::BLACK);

        let texture = self.maze_img.get_or_insert_with(|| {
            ctx.load_texture("maze-texture", ColorImage::example(), Default::default())
        });

        let maze_pixels = self.pixels.read().unwrap().clone();
        let pixel_len = img.pixels.len();
        let pixel_dim = (pixel_len as f64).sqrt() as usize;

        let min_val = img.pixels.len().min(maze_pixels.len());
        for pos in 0..min_val {
            let Point { x, y } = numb_to_vec2(pos, maze_dim);

            let color = *maze_pixels.get(pos).unwrap();
            let rel_x = ((x as f64) / (maze_dim as f64) * pixel_dim as f64) as usize;
            let rel_y = ((y as f64) / (maze_dim as f64) * pixel_dim as f64) as usize;

            for x_chunk in 0..scale {
                for y_chunk in 0..scale {
                    let one_d = vec2_to_numb(rel_x + x_chunk, rel_y + y_chunk, pixel_dim);
                    if one_d >= img.pixels.len() {
                        break;
                    }
                    img.pixels[one_d] = color;
                }
            }
        }

        texture.set(img, Default::default());
        ui.with_layout(egui::Layout::bottom_up(egui::Align::Center), |ui| {
            ui.image(texture.id(), texture.size_vec2())
        });
    }

    fn regenerate_seed(&mut self) {
        let r = rand::random();
        self.seed = r;
        self.seed_text = r.to_string();
    }

    fn add_seed_selector(&mut self, ui: &mut Ui, frame: &mut eframe::Frame) {
        ui.horizontal(|ui| {
            let checkbox = ui.checkbox(&mut self.seed_random, "Random?");
            if checkbox.changed() {
                if self.seed_random {
                    self.regenerate_seed()
                }
            }

            let seed_label = ui.label("Seed: ");
            ui.add_enabled_ui(!self.seed_random, |ui| {
                if self.seed_random {
                    self.seed_text = self.seed.to_string();
                }

                let valid_seed = self.seed_text.parse::<u64>().is_ok();
                let theme = frame.info().system_theme.unwrap_or(Theme::Dark);

                let mut text_color = if theme == Theme::Dark {
                    Color32::WHITE
                } else {
                    Color32::BLACK
                };
                if !valid_seed {
                    text_color = if theme == Theme::Dark {
                        Color32::LIGHT_RED
                    } else {
                        Color32::DARK_RED
                    };
                }

                let res = TextEdit::singleline(&mut self.seed_text)
                    .text_color(text_color)
                    .ui(ui)
                    .labelled_by(seed_label.id);
                if res.changed() {
                    let parse_res = self.seed_text.parse::<u64>();
                    if parse_res.is_ok() {
                        self.seed = parse_res.unwrap();
                    }
                }
            });
        });
    }

    fn add_size_selector(&mut self, ui: &mut Ui, frame: &mut eframe::Frame) {
        ui.horizontal(|ui| {
            let size_label = ui.label("Size: ");
            let valid_size = self.size_text.parse::<usize>().is_ok();
            let theme = frame.info().system_theme.unwrap_or(Theme::Dark);

            let mut text_color = if theme == Theme::Dark {
                Color32::WHITE
            } else {
                Color32::BLACK
            };
            if !valid_size {
                text_color = if theme == Theme::Dark {
                    Color32::LIGHT_RED
                } else {
                    Color32::DARK_RED
                };
            }

            let res = TextEdit::singleline(&mut self.size_text)
                .text_color(text_color)
                .ui(ui)
                .labelled_by(size_label.id);
            if res.changed() {
                let parse_res = self.size_text.parse::<usize>();
                if parse_res.is_ok() {
                    self.size = parse_res.unwrap();
                }
            }
        });
    }

    fn add_speed_selector(&mut self, ui: &mut Ui, frame: &mut eframe::Frame) {
        ui.horizontal(|ui| {
            let speed_label = ui.label("Speed: ");
            let valid_speed = self.speed_text.parse::<f64>().is_ok();
            let theme = frame.info().system_theme.unwrap_or(Theme::Dark);

            let mut text_color = if theme == Theme::Dark {
                Color32::WHITE
            } else {
                Color32::BLACK
            };
            if !valid_speed {
                text_color = if theme == Theme::Dark {
                    Color32::LIGHT_RED
                } else {
                    Color32::DARK_RED
                };
            }

            let res = TextEdit::singleline(&mut self.speed_text)
                .text_color(text_color)
                .ui(ui)
                .labelled_by(speed_label.id);
            if res.changed() {
                let parse_res = self.speed_text.parse::<f64>();
                if parse_res.is_ok() {
                    self.speed = parse_res.unwrap();
                    if self.curr.is_some() {
                        let c = self.curr.as_mut().unwrap();
                        let size = c.get_data().get_opt().size;

                        c.get_mut_data()
                            .set_speed_anim(Self::get_speed_limited(size, self.speed));
                    }
                }
            }
        });
    }

    fn get_speed_limited(size: usize, speed: f64) -> f64 {
        if size > 1000 {
            (size * size) as f64 * 0.0025
        } else {
            speed
        }
    }

    fn add_show_animation(&mut self, ui: &mut Ui) {
        ui.horizontal(|ui| {
            let size_label = ui.label("Show Animation: ");
            let res = ui
                .checkbox(&mut self.show_animation, "Show Animation")
                .labelled_by(size_label.id);

            if res.changed() && self.curr.is_some() {
                let c = self.curr.as_mut().unwrap();
                c.get_mut_data().set_show_anim(self.show_animation);
            }
        });
    }

    fn add_decimate_slider(&mut self, ui: &mut Ui) {
        ui.horizontal(|ui| {
            ui.label("Decimate (0-100): ");
            ui.add(Slider::new(&mut self.decimate, 0..=100));
        });
    }

    fn add_solve_algorithm(&mut self, ui: &mut Ui) {
        ui.horizontal(|ui| {
            ui.label("Solving Algorithm:");
            ComboBox::from_label("")
            .selected_text(format!("{}", self.solve_algorithm))
            .show_ui(ui, |ui| {
                for s in SolveAlgorithm::all() {
                    ui.selectable_value(&mut self.solve_algorithm, s, format!("{}", s));
                }
            });
        });

    }

    fn add_save_button(&mut self, ui: &mut Ui, ctx: &Context) {
        if let Some(result) = self.save_file_dialog.check() {
            match result {
                Ok(Some(path)) => {
                    self.save_path = path;
                    if self.curr.is_none() {
                        eprintln!("Tried to save without curr");
                    }
                    let thread = self.curr.as_ref().unwrap();
                    thread
                        .get_data()
                        .set_requested(self.save_path.to_string_lossy().to_string());
                }
                Ok(None) => {}
                Err(error) => {
                    eprintln!("Error selecting xplane_path: {}", error)
                }
            }
        }

        ui.add_enabled_ui(
            !self.save_file_dialog.is_open()
                && self.curr.is_some()
                && self.curr.as_ref().unwrap().get_data().is_done(),
            |ui| {
                if ui.button("Save Maze").clicked() {
                    let location = self
                        .save_path
                        .parent()
                        .map(|location| location.to_path_buf());

                    let temp = ctx.clone();
                    self.save_file_dialog
                        .with_callback(move |_| temp.request_repaint())
                        .show(|sender, dialog, callback| {
                            let dialog = match &location {
                                Some(location) => dialog.set_location(location),
                                None => dialog,
                            };
                            let result = dialog
                                .add_filter("PNG Image", &["png"])
                                .show_save_single_file();
                            callback(&result);
                            sender
                                .send(result)
                                .expect("error sending show_save_single_file result to ui");
                            drop(location)
                        })
                        .expect("Unable to open file_path dialog");
                }
            },
        );
    }

    fn add_gen_button(&mut self, ui: &mut Ui, ctx: &Context) {
        let mut text = "Generate";
        if self.curr.is_some() {
            let thread = self.curr.as_ref().unwrap();
            let signal_sent = thread.exit_signal_sent();

            let is_done = thread.get_data().is_done();
            if !is_done {
                if signal_sent {
                    text = "Stopping...";
                } else {
                    text = "Stop";
                }
            }

            if thread.is_finished() && signal_sent {
                println!("Setting curr to None");
                // If was done = User requested new maze
                self.curr = if is_done {
                    Some(self.start_generating(&ctx))
                } else {
                    None
                }
            }
        }

        let res = ui.button(text);
        if res.clicked() {
            if self.curr.is_some() {
                let thread = self.curr.as_ref().unwrap();
                thread.terminate();
                if self.seed_random {
                    self.regenerate_seed();
                }
            } else {
                self.curr = Some(self.start_generating(&ctx));
            }
        }
    }
}

impl App for MyApp {
    fn update(&mut self, ctx: &Context, frame: &mut eframe::Frame) {
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.vertical_centered(|ui| {
                ui.heading("Maze Generator / Solver");
            });
            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    ui.label(RichText::new("Maze").size(25.0));
                    self.add_seed_selector(ui, frame);
                    self.add_size_selector(ui, frame);
                    self.add_solve_algorithm(ui);
                    self.add_decimate_slider(ui);
                });
                ui.add_space(30.0);
                ui.vertical(|ui| {
                    ui.label(RichText::new("Animation").size(25.0));
                    self.add_speed_selector(ui, frame);
                    self.add_show_animation(ui);

                    let check = ui.checkbox(&mut self.show_debug, "Show debug?");
                    if check.changed() && self.curr.is_some() {
                        let c = self.curr.as_mut().unwrap();
                        c.get_mut_data().set_show_debug(self.show_debug);
                    }
                })
            });

            ui.add_space(10.0);
            ui.vertical_centered_justified(|ui| {
                self.add_gen_button(ui, ctx);
                self.add_save_button(ui, ctx);
            });

            ui.vertical_centered_justified(|ui| {
                if self.curr.is_some() {
                    let t = self.curr.as_ref().unwrap();
                    let proc = t.get_data().get_gen_proc();
                    let readable_proc = (proc * 100.0 * 100.0).round() / 100.0;

                    let mut text = format!("Generating: {}%", readable_proc);
                    if proc == 1.0 {
                        text = "Solving...".to_string();
                    }

                    if t.get_data().is_done() {
                        text = format!("{:?} elapsed", t.get_data().get_time_elapsed().unwrap());
                    }

                    ui.label(RichText::new(text));
                }
                self.add_image(ctx, ui);
            });
        });
    }
}
