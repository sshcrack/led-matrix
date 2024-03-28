//
// Created by hendrik on 3/28/24.
//

#include "Scene.h"

Scene::Scene(RGBMatrix *matrix) {
    this->offscreen_canvas = matrix->CreateFrameCanvas();
}
