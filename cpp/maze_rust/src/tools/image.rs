use anyhow::{Result, anyhow};
use egui::Color32;
use image::{ImageBuffer, RgbaImage, Rgba, DynamicImage};

use crate::point::point_state::{VisualIndicator, PointState};

use super::{consts::{Maze, MazeOptions}, options::MazeData, math::vec2_to_numb, window::update_maze_debug_overwrite};

pub fn maze_to_img(data: &MazeData, maze: &Maze, visual_overwrites: &Vec<Option<VisualIndicator>>) -> Result<RgbaImage> {
    let size = data.get_opt().size;

    update_maze_debug_overwrite(data, maze, visual_overwrites, true, true)?;
    let pixels = data.get_pixels();

    let mut out: RgbaImage = ImageBuffer::new(size as u32, size as u32);
    for pixel in out.enumerate_pixels_mut() {
        let index = vec2_to_numb(pixel.0 as usize, pixel.1 as usize, size);
        *pixel.2 = Rgba(pixels[index].to_array());
    }

    Ok(out)
}

pub fn img_to_maze(img: DynamicImage) -> Result<(Maze, MazeOptions)> {
    let img = img.as_rgba8().unwrap();
    let dim = img.dimensions();
    let is_odd = dim.0 % 2 == 1 && dim.1 % 2 == 1;
    if dim.0 != dim.1 || !is_odd {
        return Err(anyhow!("Invalid image dimensions"))
    }

    let size: usize = dim.0.try_into()?;
    let opt = MazeOptions::new(size, u64::MAX, 0);

    let total_pixels = size * size;
    let mut counter = 0 as u64;
    let log_size = (total_pixels as f64 * 0.05 as f64) as u64;

    let mut maze = vec![PointState::Wall; total_pixels];
    for pixel in img.enumerate_pixels() {
        let (x, y, color) = pixel;

        let color = color.0;
        let color = Color32::from_rgba_premultiplied(color[0], color[1], color[2], color[3]);
        if color != Color32::BLACK {
            let numb = vec2_to_numb(x as usize, y as usize, size);
            maze[numb] = PointState::Passage;
        }

        counter += 1;
        if counter % log_size == 0 {
            let percentage = (counter as f64 / total_pixels as f64) * 100.0 * 100.0;
            println!("{}%", percentage.round() / 100.0);
        }
    }

    Ok((maze, opt))

}