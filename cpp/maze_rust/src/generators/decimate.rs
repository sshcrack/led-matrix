use rand::Rng;

use crate::{
    point::{direction::Direction, point::Point, point_state::PointState},
    tools::{
        consts::{get_seeder, Maze},
        math::{set_point, get_point},
        options::MazeData,
    },
};

pub fn decimate_maze(data: &MazeData, maze: &mut Maze, size: usize) {
    let factor = data.get_opt().decimate;
    let mut seeder = get_seeder(data);
    println!("Decimating...");
    for y in 1..size - 1 {
        for x in 1..size - 1 {
            let p = Point { x, y };
            let is_wall =
                |d: &Direction| {
                    let state = match *d {
                        Direction::DOWN => get_point(&maze, &p.add(0, -1).unwrap()),
                        Direction::UP => get_point(&maze, &p.add(0, 1).unwrap()),
                        Direction::LEFT => get_point(&maze, &p.add(-1,0).unwrap()),
                        Direction::RIGHT => get_point(&maze, &p.add(1, 0).unwrap())
                    };

                    state == PointState::Wall
                };

            if is_wall(&Direction::LEFT)
                && is_wall(&Direction::RIGHT)
                && !is_wall(&Direction::UP)
                && !is_wall(&Direction::DOWN)
                || is_wall(&Direction::UP)
                    && is_wall(&Direction::DOWN)
                    && !is_wall(&Direction::LEFT)
                    && !is_wall(&Direction::RIGHT)
            {
                if seeder.gen_range(0..100) < factor && seeder.gen_range(0..100) < factor
                //Probability of making a wall a coridor
                {
                    set_point(maze, &p, PointState::Passage);
                }
            }
        }
    }
}
