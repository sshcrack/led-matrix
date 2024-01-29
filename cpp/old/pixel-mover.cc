// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Small example how to use the input bits
//
// This code is public domain
// (but note, that the led-matrix library this depends on is GPL v2)

#include "led-matrix.h"
#include "graphics.h"

#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <deque>

using namespace rgb_matrix;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}

static int usage(const char *progname) {
  fprintf(stderr, "usage: %s [options]\n", progname);
  rgb_matrix::PrintMatrixFlags(stderr);
  return 1;
}

static char getch() {
  static bool is_terminal = isatty(STDIN_FILENO);

  struct termios old;
  if (is_terminal) {
    if (tcgetattr(0, &old) < 0)
      perror("tcsetattr()");

    // Set to unbuffered mode
    struct termios no_echo = old;
    no_echo.c_lflag &= ~ICANON;
    no_echo.c_lflag &= ~ECHO;
    no_echo.c_cc[VMIN] = 1;
    no_echo.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &no_echo) < 0)
      perror("tcsetattr ICANON");
  }

  char buf = 0;
  if (read(STDIN_FILENO, &buf, 1) < 0)
    perror ("read()");

  if (is_terminal) {
    // Back to original terminal settings.
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
      perror ("tcsetattr ~ICANON");
  }

  return buf;
}

// Interpolation of color between head and tail of trail.
static uint8_t quantize(float c) {
  return c < 0 ? 0 : c > 255 ? 255 : roundf(c);
}
static Color interpolate(const Color &c1, const Color &c2, float fraction) {
  float c2_fraction = 1 - fraction;
  return { quantize(c1.r * fraction + c2.r * c2_fraction),
           quantize(c1.g * fraction + c2.g * c2_fraction),
           quantize(c1.b * fraction + c2.b * c2_fraction)};
}

int main(int argc, char *argv[]) {
  RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_opt;
  if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,
                                         &matrix_options, &runtime_opt)) {
    return usage(argv[0]);
  }

  RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
  if (matrix == NULL)
    return usage(argv[0]);

  rgb_matrix::FrameCanvas *canvas = matrix->CreateFrameCanvas();
  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  bool running = true;
  while (!interrupt_received && running) {
    canvas->Clear();
    const int height = canvas->height();
    const int width = canvas->width();

    canvas->SetPixels(0, 0, width, height /2, );
  

    canvas = matrix->SwapOnVSync(canvas);

    const char c = tolower(getch());
    switch (c) {
      // All kinds of conditions which we use to exit
    case 0x1B:           // Escape
    case 'q':            // 'Q'uit
    case 0x04:           // End of file
    case 0x00:           // Other issue from getch()
      running = false;
      break;
    }
  }

  // Finished. Shut down the RGB matrix.
  delete matrix;
  printf("\n");
  return 0;
}
