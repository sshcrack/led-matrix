use std::sync::{Arc, RwLock};

use egui::Context;

use crate::{generators::generate::generate, tools::{options::{MazeData, AnimOptions}, consts::MazeOptions}};


#[test]
pub fn bench_large() {
    println!("Generating...");
    let data = MazeData::new(
        &Context::default(),
        &Arc::new(RwLock::new(Vec::new())),
        &MazeOptions::new(10000, rand::random(), 30),
        &AnimOptions::new(false, false, 5000.0)
    );
    let _e = generate(&data).unwrap();
    println!("Done.");
}