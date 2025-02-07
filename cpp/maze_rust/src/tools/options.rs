use std::{sync::{RwLock, Arc}, time::Duration};

use egui::{Color32, Context};

use super::consts::{MazeOptionsArc, MazeOptions};


pub type PixelVector = Arc<RwLock<Vec<Color32>>>;
pub type ShouldExit = Arc<RwLock<bool>>;
pub type IsDone = Arc<RwLock<bool>>;
pub type ShowDebug = Arc<RwLock<bool>>;
pub type ShowAnim = Arc<RwLock<bool>>;
pub type SpeedAnim = Arc<RwLock<f64>>;
pub type SaveRequestedArc = Arc<RwLock<Option<String>>>;
pub type GenerationPercentage = Arc<RwLock<f64>>;
pub type TimeElapsedArc = Arc<RwLock<Option<Duration>>>;

#[derive(Clone, Debug)]
pub struct AnimOptions {
    show_debug: ShowDebug,
    show_anim: ShowAnim,
    speed: SpeedAnim,
}

impl Default for AnimOptions {
    fn default() -> Self {
        Self {
            show_debug: Arc::new(RwLock::new(true)),
            show_anim: Arc::new(RwLock::new(true)),
            speed: Arc::new(RwLock::new(1.0))
        }
    }
}

impl AnimOptions {
    pub fn new(show_debug: bool, show_anim: bool, speed: f64) -> Self {
        Self {
            show_debug: Arc::new(RwLock::new(show_debug)),
            show_anim: Arc::new(RwLock::new(show_anim)),
            speed: Arc::new(RwLock::new(speed))
        }
    }
}

#[derive(Clone, Debug)]
pub struct MazeData {
    pixels: PixelVector,
    should_exit: ShouldExit,
    is_done: IsDone,
    maze_opt: MazeOptionsArc,
    anim: AnimOptions,
    ctx: Context,
    save_requested: SaveRequestedArc,
    gen_proc: GenerationPercentage,
    time_elapsed: TimeElapsedArc
}

impl MazeData {
    pub fn new(ctx: &Context, pixels: &PixelVector, maze_opt: &MazeOptions, anim_opt: &AnimOptions) -> Self {
        let should_exit = Arc::new(RwLock::new(false));
        let is_done = Arc::new(RwLock::new(false));
        Self {
            ctx: ctx.clone(),
            pixels: pixels.clone(),
            is_done,
            maze_opt: Arc::new(RwLock::new(maze_opt.clone())),
            anim: anim_opt.clone(),
            should_exit: should_exit.clone(),
            save_requested: SaveRequestedArc::default(),
            gen_proc: GenerationPercentage::default(),
            time_elapsed: TimeElapsedArc::default()
        }
    }

    pub fn get_opt(&self) -> MazeOptions {
        self.maze_opt.read().unwrap().clone()
    }

    pub fn write_opt(&self, opt: &MazeOptions) {
        *self.maze_opt.write().unwrap() = opt.clone();
    }


    pub fn set_done(&self, done: bool) {
        *self.is_done.write().unwrap() = done;
    }

    pub fn is_done(&self) -> bool {
        self.is_done.read().unwrap().clone()
    }

    pub fn set_show_debug(&self, debug: bool) {
        *self.anim.show_debug.write().unwrap() = debug;
    }

    pub fn show_debug(&self) -> bool {
        return self.anim.show_debug.read().unwrap().clone();
    }

    pub fn set_show_anim(&self, anim: bool) {
        *self.anim.show_anim.write().unwrap() = anim;
    }

    pub fn show_anim(&self) -> bool {
        return self.anim.show_anim.read().unwrap().clone();
    }

    pub fn speed_anim(&self) -> f64 {
        return self.anim.speed.read().unwrap().clone();
    }

    pub fn set_speed_anim(&self, speed: f64) {
        *self.anim.speed.write().unwrap() = speed;
    }

    pub fn set_pixels(&self, pixels: Vec<Color32>) {
        *self.pixels.write().unwrap() = pixels;
        self.ctx.request_repaint();
    }

    pub fn get_pixels(&self) -> Vec<Color32> {
        self.pixels.read().unwrap().clone()
    }

    pub fn should_exit(&self) -> bool {
        let b = self.should_exit.read().unwrap().clone();
        return b;
    }

    pub fn set_should_exit(&self, should_exit: bool) {
        *self.should_exit.write().unwrap() = should_exit;
    }

    pub fn set_requested(&self, path: String) {
        *self.save_requested.write().unwrap() = Some(path);
    }

    pub fn take_requested(&self) -> Option<String> {
        if self.save_requested.read().unwrap().is_none() { return None; }

        self.save_requested.write().unwrap().take()
    }

    pub fn request_repaint(&self) {
        self.ctx.request_repaint();
    }

    pub fn set_gen_proc(&self, proc: f64) {
        *self.gen_proc.write().unwrap() = proc;
    }

    pub fn get_gen_proc(&self) -> f64 {
        self.gen_proc.read().unwrap().clone()
    }

    pub fn get_time_elapsed(&self) -> Option<Duration> {
        self.time_elapsed.read().unwrap().clone()
    }

    pub fn set_time_elapsed(&self, dur: Duration) {
        *self.time_elapsed.write().unwrap() = Some(dur);
    }
}
