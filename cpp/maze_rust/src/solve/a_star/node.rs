use crate::{point::point::Point, tools::math::get_dist};

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Node {
    parent: Option<Point>,
    cost: u64,
    steps: u64,
    pos: Point,
    end: Point
}

impl Node {
    pub fn new(pos: Point, end: &Point) -> Self {
        Self {
            parent: None,
            cost: u64::MAX -1,
            steps: u64::MAX -1,
            end: end.clone(),
            pos
        }
    }
}

impl Node {
    pub fn get_parent(&self) -> &Option<Point> {
        return &self.parent;
    }

    pub fn get_cost(&self) -> u64 {
        return self.cost;
    }

    pub fn get_pos(&self) -> Point {
        return self.pos;
    }

    pub fn update(&mut self, parent: &Node) {
        if self.parent.is_some() {
            println!("Has parent already. {} -> {}", self.get_pos(), self.parent.unwrap());
            return;
        }

        let steps = self.calculate_steps(parent);
        let cost = self.calculate_cost_steps_given(steps);


        self.parent = Some(parent.get_pos());
        self.cost = cost;
        self.steps = steps;
    }

    pub fn calculate_steps(&self, parent: &Node) -> u64 {
        parent.get_steps() +1
    }

    pub fn get_steps(&self) -> u64 {
        return self.steps;
    }

    pub fn set_start_node(&mut self) {
        self.steps = 0;
        self.cost = get_dist(&self.pos, &self.end);
    }

    pub fn calculate_cost(&self, parent: &Node) -> u64 {
        let steps = self.calculate_steps(parent);
        self.calculate_cost_steps_given(steps)
    }

    fn calculate_cost_steps_given(&self, steps: u64) -> u64 {
        let h_score = get_dist(&self.pos, &self.end);

        steps.checked_add(h_score).unwrap_or(u64::MAX)
    }
}