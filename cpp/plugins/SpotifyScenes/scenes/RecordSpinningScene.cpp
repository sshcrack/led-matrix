#include "RecordSpinningScene.h"
#include "Magick++.h"
#include <spdlog/spdlog.h>
#include "../manager/shared_spotify.h"
#include "shared/utils/canvas_image.h"
#include "shared/utils/image_fetch.h"
#include "led-matrix.h"

using namespace spdlog;
using namespace std;
using namespace Scenes;


bool RecordSpinningScene::render(rgb_matrix::RGBMatrix *matrix) {

}


int RecordSpinningScene::get_weight() const {
    if (spotify != nullptr) {
        if(spotify->has_changed(false))
            return 100;

        if(spotify->get_currently_playing().has_value())
            return Scene::get_weight();
    }

    // Don't display this scene if no song is playing
    return 0;
}

string RecordSpinningScene::get_name() const {
    return "spotify";
}

Scenes::Scene *RecordSpinningSceneWrapper::create_default() {
    return new RecordSpinningScene(Scene::create_default(3, 10 * 1000));
}

Scenes::Scene *RecordSpinningSceneWrapper::from_json(const json &args) {
    return new RecordSpinningScene(args);
}
