use std::time::Duration;

use anyhow::anyhow;
use egui::Color32;

use crate::{point::{point_state::{PointState, VisualIndicator}}, tools::consts::MAX_WAIT_TIME};

use super::{consts::{Maze, MazeOptions, FRAME_COUNT}, options::MazeData};

pub fn update_maze(data: &MazeData, maze: &Maze, always_render: bool) -> anyhow::Result<()> {
    update_maze_debug(data, maze,  &Vec::new(), always_render)
}


pub fn update_maze_overwrite(data: &MazeData, maze: &Maze, always_render: bool) -> anyhow::Result<()> {
    update_maze_debug_overwrite(data, maze, &Vec::new(), always_render, true)
}


pub fn update_maze_debug(data: &MazeData, maze: &Maze, visual_overwrites: &Vec<Option<VisualIndicator>>, always_render: bool) -> anyhow::Result<()> {
    update_maze_debug_overwrite(data, maze, visual_overwrites, always_render, false)
}


pub fn update_maze_debug_overwrite(data: &MazeData, maze: &Maze, visual_overwrites: &Vec<Option<VisualIndicator>>, always_render: bool, overwrite: bool) -> anyhow::Result<()> {
    if data.should_exit() {
        return Err(anyhow!("Terminated."));
    }

    let show_animation = data.show_anim();
    if !show_animation && !overwrite { return Ok(()); }

    let mut s = FRAME_COUNT.write().unwrap();
    *s += 1;
    let count = s.clone();

    drop(s);

    let speed = data.speed_anim();
    if count % (speed as u128).max(1) != 0 && !always_render { return Ok(()); }

    draw_maze_overwrites(data, maze, visual_overwrites)?;
    if speed < 1.0 {
        let millis = (1.0 - speed) * MAX_WAIT_TIME;
        std::thread::sleep(Duration::from_millis(millis.floor() as u64));
    }
    Ok(())
}

fn draw_maze_overwrites(data: &MazeData, maze: &Maze, visual_overwrites: &Vec<Option<VisualIndicator>>) -> anyhow::Result<()> {
    let MazeOptions { size, .. } = data.get_opt();
    let mut buffer = vec![Color32::BLACK; size * size];

    for pos in 0..maze.len() {
        let point = *maze.get(pos).unwrap();
        let overwrite = visual_overwrites.get(pos).unwrap_or(&None);
        let color = obtain_color(&point, overwrite);

        buffer[pos] = color;
    }

    data.set_pixels(buffer);
    Ok(())
}

fn obtain_color(point: &PointState, overwrite: &Option<VisualIndicator>) -> Color32 {
    if overwrite.is_some() {
        let overwrite = overwrite.unwrap();
        match overwrite {
            VisualIndicator::SolvePath => Color32::from_rgb(255, 128, 0),
            VisualIndicator::Searching => Color32::from_rgb(0, 0, 255),
            VisualIndicator::Match => Color32::from_rgb(255, 0, 255),
            VisualIndicator::End => Color32::from_rgb(0, 255, 0),
            VisualIndicator::Start => Color32::from_rgb(255, 0, 0),
            VisualIndicator::Custom(c) => c
        }
    } else {
        match point {
            PointState::Passage => Color32::from_rgb(255, 255, 255),
            PointState::Wall => Color32::from_rgb(0, 0, 0),
        }
    }
}