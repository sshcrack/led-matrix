// RGBMatrixRenderer: circle drawing primitives
#include "RGBMatrixRenderer.h"

void RGBMatrixRenderer::drawOctants(int xc, int yc, int x, int y, int yPrev, RGB_color colour, bool solid, bool persistent)
{
    if (x > 0)
    {
        if (y == yPrev || solid == false)
        {
            setPixelColour(xc + x, yc + y, colour, persistent);
            setPixelColour(xc - x, yc + y, colour, persistent);
            setPixelColour(xc + x, yc - y, colour, persistent);
            setPixelColour(xc - x, yc - y, colour, persistent);
            if (solid)
            {
                for (int lineX = xc - y; lineX <= xc + y; lineX++)
                {
                    setPixelColour(lineX, yc + x, colour, persistent);
                    setPixelColour(lineX, yc - x, colour, persistent);
                }
            }
            else
            {
                setPixelColour(xc + y, yc + x, colour, persistent);
                setPixelColour(xc - y, yc + x, colour, persistent);
                setPixelColour(xc + y, yc - x, colour, persistent);
                setPixelColour(xc - y, yc - x, colour, persistent);
            }
        }
        else
        {
            // Inner most pixels of horizontal section, so fill all postions in x range
            for (int lineX = xc - x; lineX <= xc + x; lineX++)
            {
                setPixelColour(lineX, yc + y, colour, persistent);
                setPixelColour(lineX, yc - y, colour, persistent);
            }
            if (solid)
            {
                // Middle of circle horizontal lines for every line
                for (int lineX = xc - y; lineX <= xc + y; lineX++)
                {
                    setPixelColour(lineX, yc + x, colour, persistent);
                    setPixelColour(lineX, yc - x, colour, persistent);
                }
            }
        }
    }
    else
    {
        setPixelColour(xc, yc + y, colour, persistent);
        setPixelColour(xc, yc - y, colour, persistent);
        if (solid)
        {
            for (int lineX = xc - y; lineX <= xc + y; lineX++)
            {
                setPixelColour(lineX, yc, colour, persistent);
            }
        }
        else
        {
            setPixelColour(xc - y, yc, colour, persistent);
            setPixelColour(xc + y, yc, colour, persistent);
        }
    }
}

// Function for circle-generation
// using Bresenham's algorithm
void RGBMatrixRenderer::drawCircle(int xc, int yc, int radius, RGB_color colour, bool solid, bool persistent)
{
    int r = radius;
    int x = 0, y = r, yPrev = y;
    int d = 3 - 2 * r;
    // Draw the 4 pixels marking the top, bottom, left and right most points in the circle
    drawOctants(xc, yc, x, y, yPrev, colour, solid, persistent);
    // Now draw the positions either side of the last points drawn and continue moving
    // around the circumference drawing 8 pixels each step
    while (y >= x)
    {
        x++;
        // check for decision parameter
        // and correspondingly
        // update d, x, y
        if (d > 0)
        {
            y--;
            d = d + 4 * (x - y) + 10;
        }
        else
            d = d + 4 * x + 6;
        drawOctants(xc, yc, x, y, yPrev, colour, solid, persistent);
        yPrev = y;
    }
}
