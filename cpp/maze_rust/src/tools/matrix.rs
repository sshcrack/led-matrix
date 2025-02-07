use anyhow::Result;

use crate::{point::{point::Point, point_state::PointState, direction::{Direction, DIRECTION_VEC}}};

use super::{consts::Maze, math::get_point, direction_data::DirectionData};

pub fn go_to_dir(size: &usize, point: &Point, dir: &Direction) -> Option<Point> {
    let mut pos = None;
    let size = *size as i32;
    let Point { x, y } = *point;

    for info in DIRECTION_VEC.iter() {
        if info.dir == *dir {
            let x = x as i32 + info.x;
            let y = y as i32 + info.y;
            if x < 0 || y < 0 || x >= size || y >= size {
                break;
            }

            let x = x as usize;
            let y = y as usize;
            pos = Some(Point { x, y });
            break;
        }
    }

    return pos;
}

pub fn get_surrounding_walls(size: &usize, maze: &Maze, point: &Point) -> Result<Vec<Direction>> {
    get_available_dirs_state(size, maze, point, PointState::Wall)
}

pub fn get_available_dirs_state(
    size: &usize,
    maze: &Maze,
    point: &Point,
    desired_state: PointState,
) -> anyhow::Result<Vec<Direction>> {
    let all = Direction::all();
    let all_dir = all.len();

    let mut available = Vec::with_capacity(all_dir);
    for dir in all {
        let p = go_to_dir(size, point, &dir);
        if p.is_none() {
            continue;
        }

        let p = p.unwrap();
        let state = get_point(maze, &p);

        if state == desired_state {
            available.push(dir);
        }
    }

    Ok(available)
}

// I hate carrying around data but I cant store it in consts file as well because I want to have multiple threads running mazes later :(
pub fn get_pos_between(size: &usize, src: &Point, dir: &Direction) -> Result<Option<Point>> {
    let dest = go_to_dir(size, src, dir);
    if dest.is_none() {
        return Ok(None);
    }

    let DirectionData { x: dir_x, y: dir_y, .. } = dir.to_data().normalize();
    Ok(src.add(-dir_x, -dir_y))
}

pub fn has_passage_between(size: &usize, maze: &Maze, src: &Point, dir: &Direction) -> Result<Option<bool>> {
    let between = get_pos_between(size, src, dir)?;
    if between.is_none() {
        return Ok(Some(false));
    }

    let between = between.unwrap();
    let state = get_point(maze, &between);

    return Ok(Some(state == PointState::Passage))
}
