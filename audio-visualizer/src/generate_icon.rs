use anyhow::Result;
use image::{Rgba, RgbaImage};
use std::path::Path;

pub fn generate_icon() -> Result<()> {
    let assets_dir = Path::new("assets");
    if !assets_dir.exists() {
        std::fs::create_dir_all(assets_dir)?;
    }

    // Create a 32x32 icon with audio visualizer theme
    let mut img = RgbaImage::new(32, 32);
    
    // Fill with dark background
    for x in 0..32 {
        for y in 0..32 {
            img.put_pixel(x, y, Rgba([32, 32, 40, 255]));
        }
    }
    
    // Draw some audio bars
    let bars = [
        (5, 20, Rgba([50, 220, 50, 255])),   // Green bar
        (10, 15, Rgba([220, 220, 50, 255])), // Yellow bar
        (15, 25, Rgba([50, 220, 50, 255])),  // Green bar
        (20, 10, Rgba([220, 50, 50, 255])),  // Red bar
        (25, 18, Rgba([220, 220, 50, 255])), // Yellow bar
    ];
    
    for (x, height, color) in bars {
        for y in 0..height {
            img.put_pixel(x, 32 - y - 1, color);
        }
    }
    
    // Save as PNG then convert to ICO format
    let png_path = assets_dir.join("icon.png");
    img.save(&png_path)?;
    
    // Convert to ICO manually (a simple approach)
    let ico_data = create_ico_from_png(&png_path)?;
    std::fs::write(assets_dir.join("icon.ico"), ico_data)?;
    
    Ok(())
}

fn create_ico_from_png(png_path: &Path) -> Result<Vec<u8>> {
    // This is a simplified function to create a basic ICO file
    // A real implementation would need more complexity
    let img = image::open(png_path)?.to_rgba8();
    let width = img.width();
    let height = img.height();
    
    // ICO header (6 bytes)
    let mut ico_data = vec![
        0, 0,       // Reserved, must be 0
        1, 0,       // Type: 1 = ICO
        1, 0,       // Number of images
    ];
    
    // Image directory (16 bytes)
    ico_data.extend_from_slice(&[
        width as u8, height as u8,  // Width, height (0 means 256)
        0,                         // Color palette size (0 for true color)
        0,                         // Reserved
        1, 0,                      // Color planes
        32, 0,                     // Bits per pixel
    ]);
    
    // Size of the image data
    let img_size = (width * height * 4) as u32 + 40; // 40 bytes for BITMAPINFOHEADER
    ico_data.extend_from_slice(&img_size.to_le_bytes());
    
    // Offset to the image data
    let offset = 6 + 16; // Header + Directory
    ico_data.extend_from_slice(&offset.to_le_bytes());
    
    // BITMAPINFOHEADER (40 bytes)
    ico_data.extend_from_slice(&[
        40, 0, 0, 0,  // Size of this header
    ]);
    
    // Width
    ico_data.extend_from_slice(&width.to_le_bytes());
    
    // Height (doubled for ICO format which stores height*2)
    let doubled_height = height * 2;
    ico_data.extend_from_slice(&doubled_height.to_le_bytes());
    
    ico_data.extend_from_slice(&[
        1, 0,        // Planes
        32, 0,       // Bit depth
        0, 0, 0, 0,  // Compression (none)
    ]);
    
    // Image size (can be 0 for no compression)
    let img_data_size = (width * height * 4) as u32;
    ico_data.extend_from_slice(&img_data_size.to_le_bytes());
    
    // Rest of the header
    ico_data.extend_from_slice(&[
        0, 0, 0, 0,  // X pixels per meter
        0, 0, 0, 0,  // Y pixels per meter
        0, 0, 0, 0,  // Colors used
        0, 0, 0, 0,  // Important colors
    ]);
    
    // Image data (bottom-up)
    for y in (0..height).rev() {
        for x in 0..width {
            let pixel = img.get_pixel(x, y);
            // BGRA format for ICO
            ico_data.push(pixel[2]); // B
            ico_data.push(pixel[1]); // G
            ico_data.push(pixel[0]); // R
            ico_data.push(pixel[3]); // A
        }
    }
    
    Ok(ico_data)
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_generate_icon() {
        generate_icon().unwrap();
    }
}
