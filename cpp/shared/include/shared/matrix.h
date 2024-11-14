#pragma once

#define USE_EMULATOR

//TODO move to cmake, but I really tried and I don't know how to do it
#ifdef USE_EMULATOR

#include "shared/emulator/MatrixEmulator.h"
#include "shared/emulator/FrameCanvasEmulator.h"

using ProxyMatrix = MatrixEmulator;
using ProxyFrameCanvas = FrameCanvasEmulator;
using ProxyCanvas = CanvasEmulator;
#else
#include "led-matrix.h"
using ProxyFrameCanvas = rgb_matrix::FrameCanvas;
using ProxyMatrix = rgb_matrix::RGBMatrix;
using ProxyCanvas = rgb_matrix::Canvas
#endif