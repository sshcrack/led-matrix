// RGBMatrixRenderer: colour palette and random colours
#include "RGBMatrixRenderer.h"
#include "spdlog/spdlog.h"

RGB_color RGBMatrixRenderer::getRandomColour()
{
    // Fetches a random colour from the palette if palette is full, otherwise returns a new one
    if (coloursDefined >= maxColours)
    {
        return getColor(random_int16(0, maxColours));
    }
    else
    {
        return newRandomColour();
    }
}

RGB_color RGBMatrixRenderer::newRandomColour()
{
    // Init colour randomly
    RGB_color colour;
    colour.r = random_int16(0, maxBrightness);
    colour.g = random_int16(0, maxBrightness);
    colour.b = random_int16(0, maxBrightness);
    uint8_t minBrightness = maxBrightness * 3 / 4;

    // Prevent colours being too dim
    if (colour.r < minBrightness && colour.g < minBrightness && colour.b < minBrightness)
    {
        uint8_t c = random_int16(0, 3);
        switch (c)
        {
        case 0:
            colour.r = 200;
            break;
        case 1:
            colour.g = 200;
            break;
        case 2:
            colour.b = 200;
            break;
        }
    }

    // This method for string concatenation was used as it compiles on both g++ on Linux
    // and arduino platforms
    /*
        char msg[32];
        sprintf(msg, "New RGB colour  %d, %d, %d\n", colour.r,  colour.g, colour.b);
        outputMessage(msg);
    */
    return colour;
}

RGB_color RGBMatrixRenderer::blendColour(RGB_color start, RGB_color end, uint8_t step, uint8_t steps)
{

    uint8_t r = start.r + (end.r - start.r) * step / steps;
    uint8_t g = start.g + (end.g - start.g) * step / steps;
    uint8_t b = start.b + (end.b - start.b) * step / steps;

    return RGB_color{r, g, b};
}

RGB_color RGBMatrixRenderer::getColor(uint16_t id)
{
    RGB_color colour = {0, 0, 0};

    if (id <= coloursDefined)
    {
        colour = palette[id];
    }

    return colour;
}

uint16_t RGBMatrixRenderer::getColourId(RGB_color colour)
{
    uint16_t id = 0;
    uint16_t lowestScore = 100;
    uint16_t closestMatch = 0;

    // Search palette for matching colour (black is always zero)
    if ((colour.r != 0) || (colour.g != 0) || (colour.b != 0))
    {
        for (uint16_t i = 1; i <= coloursDefined; i++)
        {

            // char msg1[64];
            // sprintf(msg1, "Searching palette %d (size %d))\n", i, coloursDefined);
            // outputMessage(msg1);

            if ((palette[i].r == colour.r) && (palette[i].g == colour.g) && (palette[i].b == colour.b))
            {
                id = i;
                lowestScore = 0;
                closestMatch = id;
                break;
            }
            else if (coloursDefined < maxColours - 1)
            {
                // Determine if closest match so far (only needed if palette is already full)
                uint16_t score = abs(palette[i].r - colour.r) + abs(palette[i].g - colour.g) + abs(palette[i].b - colour.b);
                if (score < lowestScore)
                {
                    lowestScore = score;
                    closestMatch = i;
                }
            }
        }

        // If match not found, add to palette if room
        if (id == 0)
        {
            if (coloursDefined < maxColours - 1)
            {
                coloursDefined++;

                // char msg2[64];
                // sprintf(msg2, "Adding colour: %d, %d, %d (Total: %d)\n", colour.r,  colour.g, colour.b, coloursDefined);
                // outputMessage(msg2);

                palette[coloursDefined] = colour;
                id = coloursDefined;
            }
            else
            {
                // Set to closest matching colour
                id = closestMatch;
                spdlog::warn("Asked for ({},{},{}) but got ({},{},{})", colour.r, colour.g, colour.b, palette[id].r, palette[id].g, palette[id].b);
            }
        }
    }

    // if (id > 0) {
    //     char msg[64];
    //     sprintf(msg, "Returned colour at index: %d\n", id);
    //     outputMessage(msg);
    // }

    return id;
}
