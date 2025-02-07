use lazy_static::lazy_static;

use crate::tools::direction_data::DirectionData;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT,
}

impl Direction {
    pub fn all() -> Vec<Direction> {
        vec![
            Direction::UP,
            Direction::LEFT,
            Direction::RIGHT,
            Direction::DOWN,
        ]
    }

    pub fn to_data(&self) -> DirectionData {
        for data in DIRECTION_VEC.iter() {
            if &data.dir == self {
                return data.clone();
            }
        }

        panic!(
            "wtf just happened there is no vec data in DIRECTION_VEC for {:?}",
            self
        )
    }
}

lazy_static! {
    pub static ref DIRECTION_VEC: Vec<DirectionData> = vec![
        DirectionData::from_tuple(0, -2, Direction::UP),
        DirectionData::from_tuple(-2, 0, Direction::LEFT),
        DirectionData::from_tuple(2, 0, Direction::RIGHT),
        DirectionData::from_tuple(0, 2, Direction::DOWN),
    ];
}
