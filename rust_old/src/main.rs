use std::{
    thread,
    time::{Duration, Instant},
};

use anyhow::Result;

use rand::seq::SliceRandom;
use rand::{thread_rng, Rng};

#[cfg(feature = "simulator")]
use embedded_graphics::{draw_target::DrawTarget, geometry::OriginDimensions, prelude::RgbColor};
use embedded_graphics::{
    draw_target::DrawTargetExt,
    image::{Image, ImageRaw},
};
#[cfg(feature = "simulator")]
use embedded_graphics_core::geometry::Size;
use embedded_graphics_core::{geometry::Point, image::ImageDrawable, pixelcolor::Rgb888, Drawable};
#[cfg(feature = "simulator")]
use embedded_graphics_simulator::{
    BinaryColorTheme, OutputSettingsBuilder, SimulatorDisplay, Window,
};

use lazy_static::lazy_static;
#[cfg(not(feature = "simulator"))]
use led::initialize_led;
use pixel_art::{get_image_link, get_pixel_posts};
use tinygif::Gif;

use crate::image::get_img_png;

mod image;
#[cfg(not(feature = "simulator"))]
mod led;
mod pixel_art;

lazy_static! {
    pub static ref DEFAULT_GIF_TIME: Duration = Duration::from_millis(100);
    pub static ref LATENCY_TIME: Duration = Duration::from_millis(8);
}

#[tokio::main]
async fn main() -> Result<()> {
    let gif_duration = Duration::from_secs(5);
    let img_duration = Duration::from_secs(8);

    #[cfg(not(feature = "simulator"))]
    let (_matrix, mut canvas) = initialize_led();

    #[cfg(feature = "simulator")]
    let mut canvas = SimulatorDisplay::<Rgb888>::new(Size::new(128, 128));

    #[cfg(not(feature = "simulator"))]
    let (width, height) = canvas.canvas_size();
    #[cfg(feature = "simulator")]
    let Size { height, width } = canvas.size();

    #[cfg(feature = "simulator")]
    let output_settings = OutputSettingsBuilder::new()
        .theme(BinaryColorTheme::Default)
        .build();

    #[cfg(feature = "simulator")]
    let mut w = Window::new("Out", &output_settings);

    loop {
        let mut posts = vec![("wef".to_string(), "sdf".to_string())];//get_pixel_posts(thread_rng().gen_range(1..=84)).await?;

        posts.shuffle(&mut thread_rng());
        for (post, _) in &posts {
            println!("Getting post link");
            let url = "https://pixeljoint.com/files/icons/full/drive_stars_003_pjrules_001.gif".to_string();//get_image_link(post).await?;
            let ext = url.split(".").last().unwrap();
            if ext.contains("gif") {
                println!("Getting gif img {}", url);
                let image = reqwest::get(url).await?.bytes().await?;
                let start_time = Instant::now();
                let image = Gif::<Rgb888>::from_slice(&image);
                if image.is_err() {
                    continue;
                }

                let image = image.unwrap();
                let mut should_run = true;
                println!("Showing frames");
                while should_run {
                    let frame_count = image.frames().count();
                    let centered_x = (width as i32 / 2) - (image.width() as i32 / 2);
                    let centered_y = (height as i32 / 2) - (image.height() as i32 / 2);

                    let wait_time = if frame_count > 1 {
                        gif_duration
                    } else {
                        img_duration
                    };
                    if frame_count == 0 {
                        break;
                    }

                    let offset = Point::new(centered_x, centered_y);
                    println!("{:?}", offset);
                    for frame in image.frames() {
                        #[cfg(not(feature = "simulator"))]
                        canvas.clear();

                        
                        #[cfg(feature = "simulator")]
                        canvas.clear(Rgb888::BLACK)?;

                        let translated_c = &mut canvas.translated(offset);
                        frame.draw(translated_c).unwrap();

                        #[cfg(feature = "simulator")]
                        w.update(&canvas);

                        let delay_ms = frame.delay_centis.checked_mul(10).unwrap_or(u16::MAX);
                        let duration_since = Instant::now().duration_since(start_time);
                        let left_time = wait_time
                            .checked_sub(duration_since)
                            .unwrap_or(Duration::ZERO);

                        let wait = Duration::from_millis(delay_ms as u64)
                            .max(*DEFAULT_GIF_TIME)
                            .min(left_time);

                        thread::sleep(wait);

                        if Instant::now().duration_since(start_time) > wait_time {
                            println!("Breaking loop");
                            should_run = false;
                            break;
                        }
                    }
                }

                continue;
            }

            println!("Showing simple img {}", url);
            let (raw, (img_width, img_height)) = get_img_png(&url).await?;


            let centered_x = (width as i32 / 2) - (img_width as i32 / 2);
            let centered_y = (height as i32 / 2) - (img_height as i32 / 2);

            let raw = ImageRaw::<Rgb888>::new(&raw, img_width);
            let img = Image::new(&raw, Point::new(centered_x, centered_y));

            #[cfg(not(feature = "simulator"))]
            canvas.clear();

            #[cfg(feature = "simulator")]
            canvas.clear(Rgb888::BLACK)?;

            img.draw(&mut canvas)?;

            #[cfg(feature = "simulator")]
            w.update(&canvas);
            std::thread::sleep(img_duration);
        }
    }
}
