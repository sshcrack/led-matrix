use std::collections::VecDeque;

use crate::{
    point::{
        direction::Direction,
        point::Point,
        point_state::{PointState, VisualIndicator},
    },
    tools::{
        consts::{get_seeder, get_size, Maze, set_seeder},
        math::{point_to_numb, set_point, vec2_to_numb},
        matrix::{get_available_dirs_state, get_surrounding_walls, go_to_dir},
        options::MazeData,
        window::{update_maze, update_maze_debug},
    }
};
use anyhow::{anyhow, Result};
use rand::Rng;

use super::tools::{count_to_percentage, rand_el};

pub fn hunt_and_kill(maze: &mut Maze, data: &MazeData) -> anyhow::Result<()> {
    let size = get_size(data)?;
    let cell_size = (size - 1) / 2;
    let mut last_percentage = 0.0;
    let mut seeder = get_seeder(data);

    // making sure that passage are always on odd points
    let x = seeder.gen_range(0..cell_size) * 2 + 1;
    let y = seeder.gen_range(0..cell_size) * 2 + 1;

    let mut count = 0 as u64;
    let mut pending = VecDeque::new();
    let mut to_hunt = VecDeque::new();

    let mut show_anim = data.show_anim();

    let start_p = Point { x, y };
    pending.push_back(start_p.clone());
    to_hunt.push_back(start_p);

    let mut update_rng = rand::thread_rng();
    while !pending.is_empty() {
        if update_rng.gen_bool(0.3) {
            show_anim = data.show_anim();
        }

        let mut p = pending.pop_back().unwrap();
        let mut dirs = get_surrounding_walls(&size, maze, &p)?;

        if dirs.is_empty() {
            hunt_phase(
                data,
                size,
                show_anim,
                maze,
                &mut p,
                &mut dirs,
                &mut to_hunt,
            )?;

            if dirs.is_empty() {
                break;
            }
        }

        let rand_dir = rand_el(&mut seeder, &dirs);
        let neighbor = go_to_dir(&size, &p, &rand_dir);
        count += 1;
        count_to_percentage(data, size, count, &mut last_percentage).and_then(|e| {
            last_percentage = e;
            data.set_gen_proc(e);
            data.request_repaint();
            println!("Generation: {}%", (e * 100.0 * 100.0).round() / 100.0);
            Some(())
        });

        if neighbor.is_none() {
            continue;
        }

        let neighbor = neighbor.unwrap();

        remove_wall(size, maze, &p, &neighbor)?;
        pending.push_back(neighbor);
        to_hunt.push_back(neighbor);

        update_maze(data, maze, false)?;
    }

    set_seeder(data, seeder);
    data.set_gen_proc(1.0);
    data.request_repaint();
    for _ in 0..25 {
        update_maze(data, maze, true)?;
    }

    return Ok(());
}

fn hunt_phase(
    data: &MazeData,
    size: usize,
    show_anim: bool,
    maze: &mut Maze,
    out: &mut Point,
    dirs: &mut Vec<Direction>,
    to_hunt: &mut VecDeque<Point>,
) -> Result<()> {
    let desired_size = if show_anim { size * size } else { 0 };
    let mut visual_overwrites = vec![None; desired_size];


    let all_dirs = Direction::all().len();
    let mut tried = Vec::new();
    while !to_hunt.is_empty() {
        let p =  to_hunt.pop_front().unwrap();
        if show_anim {
            visual_overwrites[point_to_numb(&p, size)] = Some(VisualIndicator::Searching);
        }

        let passages = get_available_dirs_state(&size, maze, &p, PointState::Passage)?;
        if passages.is_empty() {
            return Err(anyhow!("No passages and somehow no walls ?!?!?"))
        }

        *dirs = get_surrounding_walls(&size, maze, &p)?;
        if dirs.is_empty() {
            if passages.len() != all_dirs {
                tried.push(p);
                continue;
            }
            if show_anim {
                visual_overwrites[point_to_numb(&p, size)] = Some(VisualIndicator::SolvePath);
            }


            set_point(maze, &p, PointState::Passage);
            continue;
        }

        *out = p;
        break;
    }

    for e in tried {
        to_hunt.push_back(e);
    }

    if show_anim {
        visual_overwrites[point_to_numb(&out, size)] = Some(VisualIndicator::Match);
        for _ in 0..5 {
            update_maze_debug(data, maze, &visual_overwrites, false)?;
        }
    }

    Ok(())
}

pub fn remove_wall(size: usize, maze: &mut Maze, from: &Point, to: &Point) -> Result<()> {
    if from.x != to.x && from.y != to.y {
        return Err(anyhow!(
            "Points have to be either the same on x or y axis to draw a wall"
        ));
    }

    let Point {
        x: from_x,
        y: from_y,
    } = from;
    let Point { x: to_x, y: to_y } = to;

    let diff_x = (*to_x as i32) - (*from_x as i32);
    let diff_y = (*to_y as i32) - (*from_y as i32);

    let step_x = if diff_x != 0 {
        diff_x / diff_x.abs()
    } else {
        0
    };
    let step_y = if diff_y != 0 {
        diff_y / diff_y.abs()
    } else {
        0
    };

    for curr_x in 0..diff_x.abs() + 1 {
        let correct_x = *from_x as i32 + curr_x * step_x;

        for curr_y in 0..diff_y.abs() + 1 {
            let correct_y = *from_y as i32 + curr_y * step_y;
            if correct_x < 0
                || correct_y < 0
                || correct_x >= size as i32
                || correct_y >= size as i32
            {
                continue;
            }

            let correct_x = correct_x as usize;
            let correct_y = correct_y as usize;

            let ind = vec2_to_numb(correct_x, correct_y, size);
            maze[ind] = PointState::Passage;
        }
    }

    Ok(())
}
