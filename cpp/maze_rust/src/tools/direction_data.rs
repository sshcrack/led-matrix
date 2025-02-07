use crate::point::direction::Direction;

// Used just to store relative data to check if neighbors are alright
#[derive(Debug, Clone, Copy)]
pub struct DirectionData {
    pub x: i32,
    pub y: i32,
    pub dir: Direction
}

impl DirectionData {
    pub fn from_tuple(x: i32, y: i32, dir: Direction) -> Self {
        Self {
            x, y, dir
        }
    }

    // Converts for example x: 10 and y: -10 to x: 1 y: -1
    pub fn normalize(&self) -> Self {
        return DirectionData {
            x: if self.x == 0 { 0 } else { self.x.abs() / self.x },
            y: if self.y == 0 { 0 } else { self.y.abs() / self.y },
            dir: self.dir
        }
    }
}