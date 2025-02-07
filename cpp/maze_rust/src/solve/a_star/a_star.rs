use std::collections::BinaryHeap;

use anyhow::{Result, anyhow};
use egui::Color32;

use crate::{tools::{consts::{Maze, get_size}, matrix::{go_to_dir, get_available_dirs_state, has_passage_between, get_pos_between}, math::{set_point, point_to_numb, linear_dist, points_to_dir, get_point}, window::update_maze_debug, options::MazeData}, solve::solve::SolveOptions, point::{point_state::{VisualIndicator, PointState}, point::Point}};
use super::Node;

pub fn a_star(maze: &mut Maze, data: &MazeData, options: &SolveOptions) -> Result<(Vec<Point>, Vec<Option<VisualIndicator>>)> {
    println!("Running a*");
    let SolveOptions { start, end, ..} = options;
    let size = get_size(data)?;

    let mut visual_overwrites = vec![None; size * size];
    let mut nodes = Vec::with_capacity(size * size);
    for y in 0..size {
        for x in 0..size {
            nodes.push(Node::new(Point { x, y }, &end));
        }
    }

    let start_index = point_to_numb(&start, size);
    let start_node = nodes.get_mut(start_index).unwrap();

    start_node.set_start_node();

    let start_cost = start_node.calculate_cost(&start_node);
    drop(start_node);

    let mut pending = BinaryHeap::new();

    set_point(&mut visual_overwrites, &start, Some(VisualIndicator::Start));
    set_point(&mut visual_overwrites, &end, Some(VisualIndicator::End));

    let mut found = false;
    pending.push(start.clone());

    let mut end_node = None;
    while !pending.is_empty() && !found {
        let pos = pending.pop().unwrap();
        let node = nodes.get(point_to_numb(&pos, size)).unwrap().clone();

        let dirs = get_available_dirs_state(&size, maze, &pos, PointState::Passage)?;
        for dir in dirs {
            let neighbor = go_to_dir(&size, &pos, &dir);
            if neighbor.is_none() { continue; }

            let between_pos = get_pos_between(&size, &pos, &dir)?.unwrap();
            let has_passage = has_passage_between(&size, maze, &pos, &dir)?;

            let has_passage = has_passage.unwrap_or(false);
            if !has_passage { continue; }

            let neighbor_pos = neighbor.unwrap();

            let neighbor_index = point_to_numb(&neighbor_pos, size);
            let neighbor = nodes.get_mut(neighbor_index).unwrap();

            let temp_cost = neighbor.calculate_cost(&node);
            if temp_cost < neighbor.get_cost() {
                let color = ((1.0 - (linear_dist(&neighbor_pos, &end) as f64) / (start_cost as f64)) * 255.0).abs().floor() as u8;

                set_point(&mut visual_overwrites, &neighbor_pos, Some(VisualIndicator::Custom(Color32::from_rgb(color, 0, 255))));
                set_point(&mut visual_overwrites, &between_pos, Some(VisualIndicator::Custom(Color32::from_rgb(color, 0, 255))));
                neighbor.update(&node);
                pending.push(neighbor_pos);
            } else {
                drop(neighbor);
                //clear_path(&nodes, &neighbor_pos, &mut visual_overwrites)?;
            }

            if &neighbor_pos == end {
                println!("FOUND!");
                found = true;

                end_node = Some(neighbor_index);
                set_point(&mut visual_overwrites, &end, Some(VisualIndicator::End));
                if data.show_anim() {
                    for _ in 0..100 {
                        update_maze_debug(data, maze, &visual_overwrites, true)?;
                    }
                }
                break;
            }
        }

        for _ in 0..2 {
            update_maze_debug(data, maze, &visual_overwrites, false)?;
        }
    }

    if end_node.is_none() {
        return Err(anyhow!("Could not solve maze."))
    }


    let end_node = nodes.get(end_node.unwrap()).unwrap();
    Ok((node_to_path(&nodes, end_node, start), visual_overwrites))
}

fn node_to_path(nodes: &Vec<Node>, node: &Node, start: &Point) -> Vec<Point> {
    let mut path = Vec::new();
    let mut curr_node = node.clone();

    while curr_node.get_parent().is_some() {
        let pos = curr_node.get_pos();
        if path.iter().any(|e| e == &pos) {
            eprintln!("Duplicate tf");
            break;
        }

        path.push(pos);

        let parent = curr_node.get_parent().unwrap();
        curr_node = get_point(nodes, &parent);
    }

    path.push(start.clone());
    path.reverse();
    return path;
}

#[allow(dead_code)]
fn clear_path(data: &MazeData, nodes: &Vec<Node>, curr_pos: &Point, visual_overwrites: &mut Vec<Option<VisualIndicator>>) -> anyhow::Result<()> {
    let size = get_size(&data)?;
    let curr = get_point(nodes, curr_pos);
    let parent = curr.get_parent();
    if parent.is_some() {
        let p_pos = parent.unwrap();

        let dir = points_to_dir(&curr_pos, &p_pos);
        if dir.is_some() {
            let dir = dir.unwrap();
            let between = get_pos_between(&size, &p_pos, &dir)?;
            if between.is_some() {
                let between = between.unwrap();
                set_point(visual_overwrites, &between, None);
            }
        }
    }

    set_point(visual_overwrites, &curr_pos, None);
    Ok(())
}