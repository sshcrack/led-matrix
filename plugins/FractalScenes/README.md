# Fractal Scenes Plugin

This plugin adds various fractal and algorithm-based visualizations to the LED Matrix.

## Included Scenes

### Julia Set
An animated Julia set fractal visualization. The parameters of the Julia set slowly change over time, creating a mesmerizing effect.

#### Properties:
- **zoom**: Controls the zoom level of the fractal (0.1-3.0)
- **move_speed**: Speed of the parameter animation (0.0-1.0)
- **max_iterations**: Iteration depth for the fractal calculation (10-500)
- **animate_params**: Enable/disable animation of fractal parameters
- **color_shift**: Shift the color spectrum (0.0-1.0)

### Wave Pattern
A colorful animated wave pattern with customizable parameters.

#### Properties:
- **num_waves**: Number of overlapping waves (1-10)
- **speed**: Animation speed (0.1-5.0)
- **color_speed**: Color cycling speed (0.0-2.0)
- **rainbow_mode**: Toggle between rainbow colors and a blue-cyan theme
- **wave_height**: Control the amplitude of the waves (0.1-3.0)

### Game of Life
Conway's Game of Life cellular automaton running on the LED matrix.

#### Properties:
- **update_rate**: Simulation steps per second (1-20)
- **random_fill**: Percentage of cells that start alive (5%-50%)
- **auto_reset**: Reset the simulation after this many steps (0 to disable)
- **age_coloring**: Color cells based on their age (blue→red)

## Installation
If this plugin is not deleted from the plugins directory, install is automatic!