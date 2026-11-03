/**************************************************************************************************
 * RGB Matrix Renderer abstract class
 *
 * This abstract class provides a template for building renderer classes to control LED Matrix
 * displays. It provides methods to set the colour of any LED/pixel in a grid, and to keep the
 * position of points within the bounds of the grid (with or without wrapping over the edges).
 * A series of animation classes use the interface defined in this class to output to displays.
 * By writing a renderer class for any LED matrix hardware (or on screen display) this allows the
 * animations classes to be used with many different hardware display devices without requiring
 * code changes in the animation class.
 *
 * This class also implements a wrapping mode for panels arranged as faces of a cube, including
 * tracking the x and y components of acceleration in the plane of each face of the cube for a
 * 3D acceleration vector. The enables animations to update the motion of pixels on any panel
 * based on the cube face they appear on, and wrapping of pixels across panel edges in cube mode.
 *
 * Copyright (C) 2022 Paul Fretwell - aka 'Footleg'
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "RGBMatrixRenderer.h"
#include <stdexcept>

// default constructor
RGBMatrixRenderer::RGBMatrixRenderer(uint16_t width, uint16_t height, uint8_t brightnessLimit, bool inCubeMode)
    : gridWidth(width), gridHeight(height), maxBrightness(brightnessLimit), cubeMode(inCubeMode)
{
    // Set panel size if in cube mode
    if (inCubeMode)
    {
        // Width has to be 3/2 of height in cube mode
        if (width == height * 3 / 2)
        {
            panelSize = height / 2;
        }
        else
        {
            // Unsupported panel arrangement for cube mode
            throw std::invalid_argument("Cube mode only supports arrangements of 3 x 2 panels.");
        }
    }

    // Allocate memory for colour palette array
    palette = new RGB_color[maxColours];
    palette[0] = RGB_color{0, 0, 0};
    coloursDefined = 0;

    // Allocate memory for pixels array
    img = new uint16_t[width * height];

    clearImage();

} // RGBMatrixRenderer

// default destructor
RGBMatrixRenderer::~RGBMatrixRenderer()
{
    delete[] palette;
    delete[] img;
} //~RGBMatrixRenderer

uint16_t RGBMatrixRenderer::getGridWidth()
{
    return gridWidth;
}

uint16_t RGBMatrixRenderer::getGridHeight()
{
    return gridHeight;
}

uint8_t RGBMatrixRenderer::getMaxBrightness()
{
    return maxBrightness;
}

// Update Whole Matrix Display
void RGBMatrixRenderer::updateDisplay()
{
    // Update pixel data on display
    //    uint16_t pixels = 0;
    for (int y = 0; y < gridHeight; y++)
    {
        for (int x = 0; x < gridWidth; x++)
        {
            uint16_t colcode = img[y * gridWidth + x];
            if (colcode)
            {
                setPixel(x, y, getColor(colcode));
                //                pixels++;
            }
            else
            {
                setPixel(x, y, RGB_color{0, 0, 0});
            }
        }
    }
    showPixels();
    /*
        char msg[32];
        sprintf(msg, "Pixel count: %d\n", pixels);
        outputMessage(msg);
     */
}

void RGBMatrixRenderer::clearImage()
{
    // Clear img
    for (uint16_t i = 0; i < gridWidth * gridHeight; i++)
    {
        img[i] = 0;
    }
    // Wipe palette
    coloursDefined = 0;
}

uint16_t RGBMatrixRenderer::getPixelValue(uint16_t index)
{
    return img[index];
}

uint16_t RGBMatrixRenderer::getPixelValue(uint16_t x, uint16_t y)
{
    return img[y * gridWidth + x];
}

void RGBMatrixRenderer::setPixelValue(uint16_t index, uint16_t value)
{
    img[index] = value;
}

// Sets pixel colour in memory only (will not show changes until update display called) when persistent, else set instant
void RGBMatrixRenderer::setPixelColour(uint16_t x, uint16_t y, RGB_color colour, bool persistent)
{
    if (persistent)
    {
        img[y * gridWidth + x] = getColourId(colour);
    }
    else
    {
        setPixelInstant(x, y, colour);
    }
}

// Sets pixel colour directly on display. Faster and non-persistent as in memory display
// buffer does not get updated with the change
void RGBMatrixRenderer::setPixelInstant(uint16_t x, uint16_t y, RGB_color colour)
{
    setPixel(x, y, colour);
}
