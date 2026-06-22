#include "shared/matrix/utils/LoadingAnimation.h"
#include "shared/matrix/canvas_consts.h"
#include <led-matrix.h>

namespace LoadingAnimation {

void render(rgb_matrix::FrameCanvas *canvas, int frame,
            uint8_t r, uint8_t g, uint8_t b)
{
  const int width = Constants::width;
  const int height = Constants::height;

  canvas->Fill(0, 0, 0);

  // Dim border
  for (int x = 0; x < width; x++)
  {
    canvas->SetPixel(x, 0, 6, 6, 6);
    canvas->SetPixel(x, height - 1, 6, 6, 6);
  }
  for (int y = 0; y < height; y++)
  {
    canvas->SetPixel(0, y, 6, 6, 6);
    canvas->SetPixel(width - 1, y, 6, 6, 6);
  }

  // Bright segment traveling around the perimeter
  int perimeter = 2 * (width + height);
  int tail_len = perimeter / 16;
  int head = frame % perimeter;

  for (int i = 0; i < tail_len; i++)
  {
    int pos = (head - i + perimeter) % perimeter;
    int px, py;
    if (pos < width)
    {
      px = pos;
      py = 0;
    }
    else if (pos < width + height)
    {
      px = width - 1;
      py = pos - width;
    }
    else if (pos < 2 * width + height)
    {
      px = width - 1 - (pos - width - height);
      py = height - 1;
    }
    else
    {
      px = 0;
      py = height - 1 - (pos - 2 * width - height);
    }
    float fade = 1.0f - (float)i / tail_len;
    canvas->SetPixel(px, py,
                     (uint8_t)(r * fade),
                     (uint8_t)(g * fade),
                     (uint8_t)(b * fade));
  }
}

void render_overlay(rgb_matrix::FrameCanvas *canvas, int frame,
                    uint8_t r, uint8_t g, uint8_t b)
{
  const int width = Constants::width;
  const int height = Constants::height;

  int perimeter = 2 * (width + height);
  int tail_len = perimeter / 16;
  int head = frame % perimeter;

  for (int i = 0; i < tail_len; i++)
  {
    int pos = (head - i + perimeter) % perimeter;
    int px, py;
    if (pos < width)
    {
      px = pos;
      py = 0;
    }
    else if (pos < width + height)
    {
      px = width - 1;
      py = pos - width;
    }
    else if (pos < 2 * width + height)
    {
      px = width - 1 - (pos - width - height);
      py = height - 1;
    }
    else
    {
      px = 0;
      py = height - 1 - (pos - 2 * width - height);
    }
    float fade = 1.0f - (float)i / tail_len;
    canvas->SetPixel(px, py,
                     (uint8_t)(r * fade),
                     (uint8_t)(g * fade),
                     (uint8_t)(b * fade));
  }
}

} // namespace LoadingAnimation

