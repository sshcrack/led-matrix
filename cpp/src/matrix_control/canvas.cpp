#include "canvas.h"
#include "shared/utils/utils.h"
#include "Scene.h"
#include "../../plugins/Spotify/scenes/SpotifyScene.h"
#include "shared/utils/shared.h"
#include "../spotify/shared_spotify.h"
using namespace std;

using rgb_matrix::RGBMatrix;


void update_canvas(RGBMatrix *matrix) {
    while (!exit_canvas_update) {
        if(spotify->has_changed())
            while(spotify_scene.tick(matrix) && !exit_canvas_update) {}

        tmillis_t start_ms = GetTimeInMillis();
        tmillis_t end_ms = start_ms + 1000 * 15;
    }
}
