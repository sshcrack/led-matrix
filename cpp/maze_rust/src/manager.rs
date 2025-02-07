use std::{
    thread::{self, JoinHandle}, time::Instant,
};

use anyhow::Result;
use image::ImageFormat;

use crate::{
    generators::{generate::generate, decimate::decimate_maze},
    point::{point::Point, point_state::VisualIndicator},
    solve::solve::{solve, SolveAlgorithm, SolveOptions},
    tools::{
        consts::{get_size, check_size, MazeOptions},
        math::{set_point, set_point_mult, points_to_dir},
        matrix::get_pos_between,
        window::update_maze_debug_overwrite, options::MazeData, image::maze_to_img,
    },
};

pub struct MazeThread {
    data: MazeData,
    thread: JoinHandle<Result<()>>
}

impl MazeThread {
    pub fn is_finished(&self) -> bool {
        self.thread.is_finished()
    }

    pub fn exit_signal_sent(&self) -> bool {
        self.data.should_exit()
    }

    pub fn get_options(&self) -> MazeOptions {
        return self.data.get_opt()
    }

    #[allow(dead_code)]
    pub fn get_data(&self) -> &MazeData {
        &self.data
    }

    pub fn get_mut_data(&mut self) -> &mut MazeData {
        &mut self.data
    }

    pub fn terminate(&self) {
        if self.exit_signal_sent() {
            return;
        }

        self.data.set_should_exit(true);
    }

    pub fn new(data: &MazeData, algorithm: SolveAlgorithm) -> Self {
        data.should_exit();

        let temp = data.clone();
        Self {
            data: data.clone(),
            thread: thread::spawn(move || MazeThread::main_run(temp, algorithm)),
        }
    }

    pub fn main_run(data: MazeData, algorithm: SolveAlgorithm) -> Result<()> {
        check_size(&data);
        let end_coords = get_size(&data)? - 2;

        println!("Generating...");
        let start_time = Instant::now();
        let mut maze = generate(&data)?;
        let size = get_size(&data)?;

        decimate_maze(&data, &mut maze, size);

        let start = Point { x: 1, y: 1 };
        let end = Point {
            x: end_coords,
            y: end_coords,
        };

        let options = SolveOptions {
            algorithm,
            start,
            end,
        };

        println!("Solving...");
        let (path, mut visual_overwrites) = solve(&mut maze, &data, &options)?;

        println!("Drawing...");
        for i in 0..path.len() {
            let next_index = i + 1;
            if next_index == path.len() {
                continue;
            }

            let p = path[i];
            let n = path[next_index];

            let dir = points_to_dir(&n, &p);
            if dir.is_none() {
                eprintln!("Could not find direction for {:?} to {:?}6", p, n);
                continue;
            }

            let between = get_pos_between(&size, &p, &dir.unwrap())?.unwrap();
            set_point_mult(
                &mut visual_overwrites,
                &vec![p, between, n],
                Some(VisualIndicator::SolvePath),
            );

            if data.show_debug() {
                print!("{}", p);
                if i != path.len() -2 { print!(" -> "); }
            }
        }
        println!("");

        set_point(&mut visual_overwrites, &start, Some(VisualIndicator::Start));
        set_point(&mut visual_overwrites, &end, Some(VisualIndicator::End));

        data.request_repaint();
        data.set_time_elapsed(start_time.elapsed());
        data.set_done(true);
        while !data.should_exit() {
            update_maze_debug_overwrite(&data, &maze, &visual_overwrites, true, true)?;

            let save_path = data.take_requested();
            if save_path.is_some() {
                let save_path = save_path.unwrap();
                let out = maze_to_img(&data, &maze, &visual_overwrites)?;

                out.save_with_format(save_path, ImageFormat::Png).unwrap();
            }
        }

        println!("Done.");
        Ok(())
    }
}