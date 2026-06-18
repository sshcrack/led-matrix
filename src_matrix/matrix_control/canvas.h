#pragma once

#include "led-matrix.h"
#include "shared/matrix/post.h"
#include "shared/matrix/post_processor.h"
#include "content-streamer.h"
#include "shared/matrix/utils/utils.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/utils/canvas_image.h"
#include <vector>

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;
using rgb_matrix::StreamReader;

// Global post-processor instance

void update_canvas(RGBMatrixBase * matrix, FrameCanvas *&first_offscreen_canvas, FrameCanvas *&second_offscreen_canvas,  FrameCanvas *&composite_offscreen_canvas, std::shared_ptr<Scenes::Scene> &forced_scene, std::shared_ptr<Scenes::Scene> pinned_scene = nullptr);

bool render_scene_phase(RGBMatrixBase* matrix, std::shared_ptr<Scenes::Scene> scene, FrameCanvas*& composite_offscreen_canvas, tmillis_t end_ms);
void render_transition_phase(RGBMatrixBase* matrix, std::shared_ptr<Scenes::Scene> scene, std::shared_ptr<Scenes::Scene> next_scene, FrameCanvas* first_offscreen_canvas, FrameCanvas* second_offscreen_canvas, FrameCanvas*& composite_offscreen_canvas, int matrix_width, int matrix_height, tmillis_t transition_duration, const std::string& transition_name, std::shared_ptr<Scenes::Scene>& forced_scene);
