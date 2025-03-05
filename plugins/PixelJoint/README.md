# Pixel Joint Plugin

This plugin allows you to display pixel art from [PixelJoint](https://pixeljoint.com/) on your LED Matrix.

## Features

- Browse and display high-quality pixel art from the PixelJoint community
- Auto-scaling of artwork to fit your matrix
- Automatic image cycling with configurable timing
- Support for displaying art from specific artists or collections

## Configuration

You can configure the plugin with the following settings:

```json
{
  "pluginConfigs": {
    "pixelJointInterval": 60,  // Time in seconds between image changes
    "pixelJointFavorites": []  // Array of favorite piece IDs to display
  }
}
```

## How It Works

The plugin fetches pixel art from PixelJoint, processes the images to fit your LED matrix dimensions, and displays them with appropriate scaling. Images are cached locally to reduce bandwidth usage and improve performance.

## Installation
If this plugin is not deleted from the plugins directory, install is automatic!