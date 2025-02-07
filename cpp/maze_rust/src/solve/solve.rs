use std::fmt::Display;

use anyhow::Result;

use crate::{tools::{consts::Maze, options::MazeData}, point::{point::Point, point_state::VisualIndicator}};

use super::a_star::a_star;

pub fn solve(maze: &mut Maze, data: &MazeData, options: &SolveOptions) -> Result<(Vec<Point>, Vec<Option<VisualIndicator>>)> {
    let SolveOptions { algorithm, .. } = options;
    let res = match algorithm {
        SolveAlgorithm::AStar => a_star(maze, data, options),
        SolveAlgorithm::None => {
            let no_visual = vec![None; maze.len()];
            Ok((Vec::new(), no_visual))
        }
    };

    res
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SolveAlgorithm {
    None,
    AStar
}

impl  SolveAlgorithm {
    pub fn all() -> Vec<Self> {
        vec![SolveAlgorithm::AStar, SolveAlgorithm::None]
    }
}

impl Display for SolveAlgorithm {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let e = match self {
            Self::AStar => "A*",
            Self::None => "None"
        };

        write!(f, "{}", e)
    }
}

#[derive(Debug, Clone, Copy)]
pub struct SolveOptions {
    pub start: Point,
    pub end: Point,
    pub algorithm: SolveAlgorithm
}

impl SolveOptions {
    pub fn new(size: usize) -> Self {
        let end_coords = size - 2;

        let start = Point { x: 1, y: 1 };
        let end = Point {
            x: end_coords,
            y: end_coords,
        };

        SolveOptions {
            algorithm: SolveAlgorithm::AStar,
            start,
            end,
        }
    }
}