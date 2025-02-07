use rand::{rngs::StdRng, Rng};

use crate::tools::options::MazeData;

pub fn count_to_percentage(data: &MazeData, size: usize, count: u64, last_percentage: &mut f64) -> Option<f64> {
    if count % data.speed_anim().max(1.0) as u64 != 0 {
        return None;
    }

    let total = (size - 3)*(size + 1)/4;
    let rounded = (count as f64 / total as f64 * 100.0 * 10.0).round() / (10.0 * 100.0);
    if rounded - *last_percentage < 0.003 {
        return None;
    }


    Some(rounded)
}

pub fn rand_el<T>(seeder: &mut StdRng, arr: &[T]) -> T
    where
    T: Copy
{
    let index = seeder.gen_range(0..arr.len());
    return arr[index];
}