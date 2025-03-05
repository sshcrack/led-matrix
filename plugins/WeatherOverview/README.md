# Weather Overview Plugin

This plugin displays current weather information and forecasts on your LED Matrix.

## Features

- Shows current temperature, conditions, and weather icons
- Displays weather forecasts
- Customizable location settings

## Configuration

The plugin requires location coordinates to fetch accurate weather data:

```json
{
  "pluginConfigs": {
    "weatherLat": "52.5200",  // Latitude (default: Berlin)
    "weatherLon": "13.4050"   // Longitude (default: Berlin)
  }
}
```

## Font Configuration

The plugin uses BDF fonts to display text:
- Header font: 7x13.bdf 
- Body font: 5x8.bdf

You can set a custom directory for fonts by using the environment variable:
```
WEATHER_FONT_DIRECTORY=/path/to/your/fonts
```

## Installation
1. Ensure the necessary fonts are available in the expected directory
2. Configure your location in the configuration file
3. If this plugin is not deleted from the plugins directory, install is automatic!
4. Restart the LED Matrix service
