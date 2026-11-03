// RGBMatrixRenderer: grid and cube-face position updates
#include "RGBMatrixRenderer.h"


// Method for a normal rectangular flat grid, updates a grid coordinate while keeping on the matrix, with optional wrapping
uint16_t RGBMatrixRenderer::newPosition(uint16_t position, uint16_t increment, uint16_t dimension, bool wrap)
{
    int16_t newPos = position + increment;

    // char msg[64];
    // sprintf(msg, "NewPos: Value  %d; Increment: %d; Limit: %d\n", position,  increment, dimension);
    // outputMessage(msg);

    if (wrap)
    {
        while (newPos < 0)
            newPos += dimension;
        while (newPos >= dimension)
            newPos -= dimension;
    }
    else
    {
        if (newPos < 0)
            newPos = 0;
        else if (newPos >= dimension)
            newPos = dimension - 1;
    }

    return newPos;
}

uint8_t RGBMatrixRenderer::getPanel(MovingPixel pixel)
{
    uint8_t panel = 0;

    uint8_t row = pixel.y / panelSize;
    uint8_t col = pixel.x / panelSize;

    panel = (row * gridWidth / panelSize) + col;

    return panel;
};

// Method for a cube, updates a grid coordinates while keeping on the matrix, with optional wrapping
MovingPixel RGBMatrixRenderer::updatePosition(MovingPixel pixel, bool wrap)
{
    MovingPixel newPixel(0, 0, 0, 0);

    // Update the pixel fine position (fraction of a pixel position)
    uint16_t fineInc = abs(pixel.fineX + pixel.vx);
    // char msg1[20];
    // sprintf(msg1, "FineIncX= %d ", fineInc);
    // outputMessage(msg1);
    if (fineInc >= SUBPIXEL_RES)
    {
        // Moved at least one pixel position, so update
        int16_t wholePix = fineInc / SUBPIXEL_RES;
        int16_t partPix = fineInc - (wholePix * SUBPIXEL_RES);
        if (pixel.vx < 0)
        {
            wholePix = -wholePix;
            partPix = -partPix;
        }
        newPixel.x = newPositionX(pixel.x, wholePix, wrap);
        newPixel.fineX = partPix;
        // char msg1[96];
        // sprintf(msg1, "WholePix=%d PartPix=%d OldFine: %d,%d NewFine: %d,%d\n", wholePix, partPix, pixel.fineX, pixel.fineY, newPixel.fineX, newPixel.fineY);
        // outputMessage(msg1);
    }
    else if (pixel.vx < 0)
    {
        newPixel.x = pixel.x;
        newPixel.fineX = -fineInc;
    }
    else
    {
        newPixel.x = pixel.x;
        newPixel.fineX = fineInc;
    }

    fineInc = abs(pixel.fineY + pixel.vy);
    // char msg2[20];
    // sprintf(msg2, "FineIncY= %d ", fineInc);
    // outputMessage(msg2);
    if (fineInc >= SUBPIXEL_RES)
    {
        // Moved at least one pixel position, so update
        int16_t wholePix = fineInc / SUBPIXEL_RES;
        int16_t partPix = fineInc - (wholePix * SUBPIXEL_RES);
        if (pixel.vy < 0)
        {
            wholePix = -wholePix;
            partPix = -partPix;
        }
        newPixel.y = newPositionY(pixel.y, wholePix, wrap);
        newPixel.fineY = partPix;
        // char msg1[96];
        // sprintf(msg1, "WholePix=%d PartPix=%d OldFine: %d,%d NewFine: %d,%d\n", wholePix, partPix, pixel.fineX, pixel.fineY, newPixel.fineX, newPixel.fineY);
        // outputMessage(msg1);
    }
    else if (pixel.vy < 0)
    {
        newPixel.y = pixel.y;
        newPixel.fineY = -fineInc;
    }
    else
    {
        newPixel.y = pixel.y;
        newPixel.fineY = fineInc;
    }

    newPixel.vx = pixel.vx;
    newPixel.vy = pixel.vy;

    if (cubeMode)
    {
        // For cube, the panels are arranged in 2 rows of 3. The bottom row of 3 panels represent
        // 3 sides with x being across, and y being up. The top row represents the top, back and bottom
        // sides of the cube. x and y directions vary across these 3 panels wrt to the underlying 3 x 2
        // matrix.

        // Determine which panel the pixel is on now
        uint8_t panelRow = pixel.y / panelSize;
        uint8_t panelCol = pixel.x / panelSize;
        uint16_t newX;
        uint16_t newY;
        int16_t newFineX;
        // int16_t newFineY;

        // Determine which panel the new pixel is on (before cube wrapping translation is applied)
        uint8_t panelRowNew = newPixel.y / panelSize;
        uint8_t panelColNew = newPixel.x / panelSize;

        // char msg1[96];
        // sprintf(msg1, "Cube Mode. Wrap=%d On Panel: %d,%d Moving to panel: %d,%d pos: %d,%d ", wrap, panelCol, panelRow, panelColNew, panelRowNew, pixel.x, pixel.y);
        // outputMessage(msg1);

        // Update wrapping if changed panel
        if (wrap == false)
        {
            // If not wrapping, then don't allow move off panel
            newPixel = pixel;
        }
        else if (panelRow == panelRowNew)
        {
            if (panelRow == 0)
            {
                // Bottom Row
                if (panelCol == 2 && panelColNew == 0)
                {
                    // Wrapped off RH edge of panel3, so transpose onto panel 5
                    newY = gridHeight - 1 - newPixel.x;
                    newX = panelSize + newPixel.y;
                    newPixel.x = newX;
                    newPixel.y = newY;
                    // Transpose part pixel positions
                    newFineX = newPixel.fineY;
                    newPixel.fineY = -newPixel.fineX;
                    newPixel.fineX = newFineX;
                    // Transpose X and Y velocities
                    newPixel.vx = pixel.vy;
                    newPixel.vy = -pixel.vx;
                }
                else if (panelCol == 0 && panelColNew == 2)
                {
                    // Wrapped off LH edge of panel1, so transpose onto panel 5
                    newY = gridWidth - 1 - newPixel.x + panelSize;
                    newX = panelSize + newPixel.y;
                    newPixel.x = newX;
                    newPixel.y = newY;
                    // Transpose part pixel positions
                    newFineX = newPixel.fineY;
                    newPixel.fineY = -newPixel.fineX;
                    newPixel.fineX = newFineX;
                    // Transpose X and Y velocities
                    newPixel.vx = pixel.vy;
                    newPixel.vy = -pixel.vx;
                }
                // else {
                //     //Remains on row 0, so just copy velocity to new pixel
                //     newPixel.vx = pixel.vx;
                //     newPixel.vy = pixel.vy;
                // }
            }
            else
            {
                // Upper Row
                if (panelCol == 2 && panelColNew == 0)
                {
                    // Wrapped off RH edge of panel6, so transpose onto panel 2
                    newY = panelSize - 1 - newPixel.x;
                    newX = newPixel.y;
                    newPixel.x = newX;
                    newPixel.y = newY;
                    // Transpose part pixel positions
                    newFineX = newPixel.fineY;
                    newPixel.fineY = -newPixel.fineX;
                    newPixel.fineX = newFineX;
                    // Transpose X and Y velocities
                    newPixel.vx = pixel.vy;
                    newPixel.vy = -pixel.vx;
                }
                else if (panelCol == 0 && panelColNew == 2)
                {
                    // Wrapped off LH edge of panel4, so transpose onto panel 5
                    newY = gridWidth - 1 - newPixel.x;
                    newX = newPixel.y;
                    newPixel.x = newX;
                    newPixel.y = newY;
                    // Transpose part pixel positions
                    newFineX = newPixel.fineY;
                    newPixel.fineY = -newPixel.fineX;
                    newPixel.fineX = newFineX;
                    // Transpose X and Y velocities
                    newPixel.vx = pixel.vy;
                    newPixel.vy = -pixel.vx;
                }
                // else {
                //     //Remains on row 1, so just copy velocity to new pixel
                //     newPixel.vx = pixel.vx;
                //     newPixel.vy = pixel.vy;
                // }
            }
        }
        else
        {
            // Change of panel row
            if (panelCol == panelColNew)
            {
                // Moving onto panel above or below (not diagonally)
                uint8_t panelColNewCube = 0;
                uint8_t panelRowNewCube = 0;
                int16_t translate = 0;
                if (panelCol == 0)
                {
                    if (panelRow == 0)
                    {
                        // On panel 1
                        if (pixel.vy > 0)
                        {
                            // Moving up from panel 1 to panel 6
                            // Shift 2 panels to the right. Y wraps as normal so no change
                            translate = 2 * panelSize;
                        }
                        else
                        {
                            // Moving down from panel 1 to 4 (flip X and Y)
                            panelColNewCube = 0;
                            panelRowNewCube = 1;
                        }
                    }
                    else
                    {
                        // On panel 4
                        if (pixel.vy > 0)
                        {
                            // Moving up from panel 4 to panel 3
                            // Shift 2 panels to the right. Y wraps as normal so no change
                            translate = 2 * panelSize;
                        }
                        else
                        {
                            // Moving down from panel 4 to 1 (flip X and Y)
                            panelColNewCube = 0;
                            panelRowNewCube = 0;
                        }
                    }
                }
                else if (panelCol == 1)
                {
                    if (panelRow == 0)
                    {
                        // On panel 2
                        if (pixel.vy > 0)
                        {
                            // Moving up from panel 2 to panel 6 (swap X and Y)
                            panelColNewCube = 2;
                            panelRowNewCube = 1;
                            translate = 1;
                        }
                        else
                        {
                            // Moving down from panel 2 to panel 4 (swap X and Y)
                            panelColNewCube = 0;
                            panelRowNewCube = 1;
                            translate = 1;
                        }
                    }
                    else
                    {
                        // On panel 5
                        if (pixel.vy > 0)
                        {
                            // Moving up from panel 5 to panel 3 (swap X and Y)
                            panelColNewCube = 2;
                            panelRowNewCube = 0;
                            translate = 1;
                        }
                        else
                        {
                            // Moving down from panel 5 to panel 1 (swap X and Y)
                            panelColNewCube = 0;
                            panelRowNewCube = 0;
                            translate = 1;
                        }
                    }
                }
                else
                {
                    if (panelRow == 0)
                    {
                        // On panel 3
                        if (pixel.vy > 0)
                        {
                            // Moving up from panel 3 to 6 (flip X and Y)
                            panelColNewCube = 2;
                            panelRowNewCube = 1;
                        }
                        else
                        {
                            // Moving down from panel 3 to panel 4
                            // Shift 2 panels to the left.
                            translate = -2 * panelSize;
                        }
                    }
                    else
                    {
                        // On panel 6
                        if (pixel.vy > 0)
                        {
                            // Moving up from panel 6 to panel 3 (flip X and Y)
                            panelColNewCube = 2;
                            panelRowNewCube = 0;
                        }
                        else
                        {
                            // Moving down from panel 6 to panel 1
                            // Shift 2 panels to the left.
                            translate = -2 * panelSize;
                        }
                    }
                }

                // Process operation
                if (translate == 0)
                {
                    // Flip X and Y within panel, then translate to position of final panel
                    newPixel.x = panelSize - 1 - (newPixel.x - panelColNew * panelSize) + panelColNewCube * panelSize;
                    newPixel.y = panelSize - 1 - (newPixel.y - panelRowNew * panelSize) + panelRowNewCube * panelSize;
                    // Reverse part pixel positions
                    newPixel.fineX = -newPixel.fineX;
                    newPixel.fineY = -newPixel.fineY;
                    // Reverse X and Y velocities
                    newPixel.vx = -pixel.vx;
                    newPixel.vy = -pixel.vy;
                }
                else if (translate == 1)
                {
                    // Swap X and Y within panel, flip Y, then translate to position of final panel
                    newX = panelSize - 1 - (newPixel.y - panelRowNew * panelSize) + panelColNewCube * panelSize;
                    newPixel.y = (newPixel.x - panelColNew * panelSize) + panelRowNewCube * panelSize;
                    newPixel.x = newX;
                    // Transpose part pixel positions
                    newFineX = -newPixel.fineY;
                    newPixel.fineY = newPixel.fineX;
                    newPixel.fineX = newFineX;
                    // Transpose X and Y velocities
                    newPixel.vx = -pixel.vy;
                    newPixel.vy = pixel.vx;
                }
                else
                {
                    // Translate x to new panel, Y wraps as for non-cube arrangement of flat panels, so no change in Y
                    newPixel.x = newPixel.x + translate;
                    // Retain same X and Y velocities
                    newPixel.vx = pixel.vx;
                    newPixel.vy = pixel.vy;
                }
            }
            else
            {
                // Diagonal cases
                if (panelCol == 0)
                {
                    if (panelRow == 0)
                    {
                        // On panel 1
                        if (pixel.vy > 0)
                        {
                            // Moving up
                        }
                        else
                        {
                            // Moving down
                        }
                    }
                    else
                    {
                        // On panel 4
                        if (pixel.vy > 0)
                        {
                            // Moving up
                        }
                        else
                        {
                            // Moving down
                        }
                    }
                }
                else if (panelCol == 1)
                {
                    if (panelRow == 0)
                    {
                        // On panel 2
                        if (pixel.vy > 0)
                        {
                            // Moving up
                        }
                        else
                        {
                            // Moving down
                        }
                    }
                    else
                    {
                        // On panel 5
                        if (pixel.vy > 0)
                        {
                            // Moving up
                        }
                        else
                        {
                            // Moving down
                        }
                    }
                }
                else
                {
                    if (panelRow == 0)
                    {
                        // On panel 3
                        if (pixel.vy > 0)
                        {
                            // Moving up
                        }
                        else
                        {
                            // Moving down
                        }
                    }
                    else
                    {
                        // On panel 6
                        if (pixel.vy > 0)
                        {
                            // Moving up
                        }
                        else
                        {
                            // Moving down
                        }
                    }
                }
            }
        }
    }

    return newPixel;
}

uint16_t RGBMatrixRenderer::newPositionX(uint16_t x, uint16_t increment, bool wrap)
{
    return newPosition(x, increment, gridWidth, wrap);
}

uint16_t RGBMatrixRenderer::newPositionY(uint16_t y, uint16_t increment, bool wrap)
{
    return newPosition(y, increment, gridHeight, wrap);
}
