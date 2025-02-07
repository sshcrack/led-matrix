use std::{cmp::Ordering, fmt::Display};

#[derive(Clone, Debug, Copy, Eq)]
pub struct Point {
    pub x: usize,
    pub y: usize
}

impl Display for Point {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "P({},{})", self.x, self.y)
    }
}

impl Point {
    pub fn add(&self, x: i32, y: i32) -> Option<Self> {
        let new_x = self.x as i32 - x;
        let new_y = self.y as i32 - y;

        let new_x: Result<usize, _> = new_x.try_into();
        let new_y: Result<usize, _> = new_y.try_into();

        if new_x.is_err() || new_y.is_err() {
            eprintln!("Could not add because it would resolve in negative point");
            return None;
        }

        return Some(Point {
            x: new_x.unwrap(),
            y: new_y.unwrap()
        })
    }
}

impl Ord for Point {
    fn cmp(&self, other: &Self) -> Ordering {
        (self.x, self.y).cmp(&(other.x, other.y))
    }
}

impl PartialOrd for Point {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for Point {
    fn eq(&self, other: &Self) -> bool {
        (self.x, self.y) == (other.x, other.y)
    }
}

