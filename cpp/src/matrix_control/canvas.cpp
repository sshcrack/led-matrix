#include "canvas.h"
#include "shared/utils/utils.h"
#include "Scene.h"
#include "scene/WatermelonPlasmaScene.h"
#include "scene/WaveScene.h"
#include "scene/SpotifyScene.h"
#include "shared/utils/shared.h"
#include "../spotify/shared_spotify.h"
using namespace std;

using rgb_matrix::RGBMatrix;


void update_canvas(RGBMatrix *matrix) {
    auto watermelon_plasma_scene = Scenes::WatermelonPlasmaScene(matrix);
    auto wave_scene = Scenes::WaveScene(matrix);
    auto spotify_scene = Scenes::SpotifyScene(matrix);

    while (!exit_canvas_update) {
        if(spotify->has_changed())
            while(spotify_scene.tick(matrix) && !exit_canvas_update) {}

        tmillis_t start_ms = GetTimeInMillis();
        tmillis_t end_ms = start_ms + 1000 * 15;

        while(!wave_scene.tick(matrix) && !exit_canvas_update && GetTimeInMillis() < end_ms) {}

        start_ms = GetTimeInMillis();
        end_ms = start_ms + 1000 * 15;
        while(!watermelon_plasma_scene.tick(matrix) && !exit_canvas_update && GetTimeInMillis() < end_ms) {}
    }
}
