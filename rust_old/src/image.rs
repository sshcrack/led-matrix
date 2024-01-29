use anyhow::Result;
use image::{GenericImageView, ImageFormat, Pixel};

pub async fn get_img_png(url: &str) -> Result<(Vec<u8>, (u32, u32))> {
    let ext = url.split(".").last().unwrap();
    let format = ImageFormat::from_extension(ext).unwrap();

    let img = reqwest::get(url).await?;
    let img = img.bytes().await?;

    let img = image::load_from_memory_with_format(&img, format)?;
    let embedded: Vec<[u8; 3]> = img.pixels().map(|(_, _1, c)| {
        let a = c[3] as f64;
        let mut rgb = c.to_rgb();
        rgb.apply(|e| (e as f64 * (a / 255.0)).min(255.0) as u8);

        return rgb.0;
    }).collect();

    Ok((embedded.concat(), (img.width(), img.height())))
}