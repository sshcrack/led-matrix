use std::sync::{Arc, RwLock};

use egui::Context;

use image::io::Reader as ImageReader;
use crate::{tools::{options::{MazeData, AnimOptions}, image::img_to_maze}, solve::solve::{solve, SolveOptions}};


#[test]
pub fn bench_solve() {
    println!("Reading img...");
    let img = ImageReader::open("large.png").unwrap().decode().unwrap();

    println!("Translating img to maze...");
    let (mut maze, opt) = img_to_maze(img).unwrap();

    let data = MazeData::new(
        &Context::default(),
        &Arc::new(RwLock::new(Vec::new())),
        &opt,
        &AnimOptions::new(false, false, 5000.0)
    );
    println!("Solving...");
    let _e = solve(&mut maze, &data, &SolveOptions::new(opt.size)).unwrap();
    println!("Done.");
}