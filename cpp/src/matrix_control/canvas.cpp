#include "canvas.h"
#include "../utils/utils.h"
#include "scene/Scene.h"
#include "scene/ImageScene.h"
#include "scene/WatermelonPlasmaScene.h"
#include "scene/WaveScene.h"
#include "scene/SpotifyScene.h"
#include "../utils/shared.h"
#include "../spotify/shared_spotify.h"
using namespace std;

using rgb_matrix::RGBMatrix;


void update_canvas(RGBMatrix *matrix) {
    auto watermelon_plasma_scene = Scenes::WatermelonPlasmaScene(matrix);
    auto wave_scene = Scenes::WaveScene(matrix);
    auto spotify_scene = Scenes::SpotifyScene(matrix);
    auto preset_scene = Scenes::ImageScene(matrix);

    while (!exit_canvas_update) {
        if(spotify->has_changed())
            while(spotify_scene.tick(matrix) && !exit_canvas_update) {}

        tmillis_t start_ms = GetTimeInMillis();
        tmillis_t end_ms = start_ms + 1000 * 15;

        while(!wave_scene.tick(matrix) && !exit_canvas_update && GetTimeInMillis() < end_ms) {}

        start_ms = GetTimeInMillis();
        end_ms = start_ms + 1000 * 15;
        while(!watermelon_plasma_scene.tick(matrix) && !exit_canvas_update && GetTimeInMillis() < end_ms) {}
        while(!preset_scene.tick(matrix) && !exit_canvas_update) {
            SleepMillis(5);
        }
    }
}
