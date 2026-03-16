#include "canvas.h"
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>

#include <shared/matrix/server/common.h>

#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/utils/utils.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/plugin_loader/loader.h"
#include <spdlog/spdlog.h>
#include <algorithm>

#ifdef ENABLE_EMULATOR
#include "emulator.h"
#endif

using namespace std;
using namespace spdlog;

using rgb_matrix::RGBMatrixBase;

rgb_matrix::Color ERROR_COLOR = rgb_matrix::Color(255, 0, 0);
rgb_matrix::Font ERROR_FONT = rgb_matrix::Font();
bool load_font_error = false;
bool loaded_font_success = false;

namespace
{
    std::vector<std::pair<int, std::shared_ptr<Scenes::Scene>>> build_weighted_scenes(
        const std::vector<std::shared_ptr<Scenes::Scene>> &scenes,
        bool is_desktop_connected,
        std::string exclude_scene_name = "")
    {
        std::vector<std::pair<int, std::shared_ptr<Scenes::Scene>>> weighted_scenes;
        for (const auto &item : scenes)
        {
            auto weight = item->get_weight();
            if (weight <= 0)
                continue;
            if (item->get_name() == exclude_scene_name)
                continue;

            if (item->needs_desktop_app() && !is_desktop_connected)
                continue;

            weighted_scenes.emplace_back(weight, item);
        }

        return weighted_scenes;
    }

    std::shared_ptr<Scenes::Scene> select_scene(const std::vector<std::pair<int, std::shared_ptr<Scenes::Scene>>> &weighted_scenes)
    {
        if (weighted_scenes.empty())
        {
            return nullptr;
        }

        int total_weight = 0;
        for (const auto &[weight, _scene] : weighted_scenes)
        {
            total_weight += weight;
        }

        const auto selected = get_random_number_inclusive(0, total_weight);
        int curr_weight = 0;

        for (const auto &[weight, curr_scene] : weighted_scenes)
        {
            curr_weight += weight;

            if (curr_weight >= selected)
            {
                return curr_scene;
            }
        }

        return weighted_scenes.front().second;
    }

    tmillis_t resolve_transition_duration(const std::shared_ptr<ConfigData::Preset> &preset,
                                          const std::shared_ptr<Scenes::Scene> &scene)
    {
        const auto scene_override = scene->get_transition_duration();
        if (scene_override > 0)
        {
            return scene_override;
        }

        return preset->transition_duration;
    }

    std::string resolve_transition_name(const std::shared_ptr<ConfigData::Preset> &preset,
                                        const std::shared_ptr<Scenes::Scene> &scene)
    {
        const auto scene_override = scene->get_transition_name();
        if (!scene_override.empty() && scene_override != Plugins::TRANSITION_NAME_GLOBAL_DEFAULT)
        {
            return scene_override;
        }
        return preset->transition_name;
    }

    bool should_schedule_transition(tmillis_t transition_duration, tmillis_t scene_duration)
    {
        return transition_duration > 0 && transition_duration < scene_duration;
    }

    void copy_canvas(FrameCanvas *dst, FrameCanvas *src, int width, int height)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                uint8_t r = 0;
                uint8_t g = 0;
                uint8_t b = 0;
                src->GetPixel(x, y, &r, &g, &b);
                dst->SetPixel(x, y, r, g, b);
            }
        }
    }

    void apply_transition_frame(
        FrameCanvas *dst,
        FrameCanvas *from,
        FrameCanvas *to,
        float alpha_progress,
        int width,
        int height,
        const std::string &transition_name)
    {
        TransitionEffect *transition_effect = nullptr;
        if (Constants::global_transition_manager != nullptr)
        {
            transition_effect = Constants::global_transition_manager->get_transition(transition_name);
            if (transition_effect == nullptr)
            {
                transition_effect = Constants::global_transition_manager->get_transition("blend");
            }
        }

        if (transition_effect != nullptr)
        {
            transition_effect->apply(dst, from, to, alpha_progress, width, height);
            return;
        }

        // If no transition effect is available, render a hard cut to the next scene.
        copy_canvas(dst, to, width, height);
    }

    void notify_scene_active(const std::shared_ptr<Scenes::Scene> &scene)
    {
        {
            std::unique_lock lock(Server::currSceneMutex);
            Server::currScene = scene;
        }

        {
            std::shared_lock lock(Server::registryMutex);
            spdlog::debug("Now displaying scene: {}", scene->get_name());
            for (const auto ws_handle : Server::registry | views::values)
            {
                restinio::websocket::basic::message_t message;
                message.set_opcode(restinio::websocket::basic::opcode_t::text_frame);
                message.set_payload("active:" + scene->get_name());

                ws_handle->send_message(message);
            }
        }
    }
}

void render_fallback(RGBMatrixBase *canvas)
{
    if (!loaded_font_success && !load_font_error)
    {
        if (!ERROR_FONT.LoadFont((get_exec_dir() / "7x13.bdf").c_str()))
        {
            spdlog::error("Could not load error font");
            load_font_error = true;
            return;
        }

        loaded_font_success = true;
    }

    if (load_font_error)
    {
        spdlog::error("Error font not loaded, cannot render fallback");
        return;
    }

    canvas->Fill(0, 0, 0); // Fill with black
    rgb_matrix::DrawText(canvas, ERROR_FONT, 0, 11, ERROR_COLOR, "No scene available");
}

void update_canvas(RGBMatrixBase *matrix, FrameCanvas *&first_offscreen_canvas, FrameCanvas *&second_offscreen_canvas, FrameCanvas *&composite_offscreen_canvas, std::shared_ptr<Scenes::Scene> &forced_scene)
{
    const auto preset = config->get_curr();
    const auto &scenes = preset->scenes;
    const int matrix_width = matrix->width();
    const int matrix_height = matrix->height();

    for (const auto &item : scenes)
    {
        if (!item->is_initialized())
            item->initialize(matrix_width, matrix_height);
    }

    int no_scene_count = 0;
    while (!exit_canvas_update)
    {
        bool is_desktop_connected = Server::is_desktop_connected();

        std::shared_ptr<Scenes::Scene> scene = forced_scene;
        forced_scene = nullptr;
        if (scene == nullptr)
        {
            auto weighted_scenes = build_weighted_scenes(scenes, is_desktop_connected, scene != nullptr ? scene->get_name() : "");
            scene = select_scene(weighted_scenes);
        }

        if (scene == nullptr)
        {
            if (no_scene_count < 3)
                error("Could not find scene to display.");
            no_scene_count++;

            Server::currScene = nullptr;
            render_fallback(matrix);

#ifdef ENABLE_EMULATOR
            ((rgb_matrix::EmulatorMatrix *)matrix)->Render();
#endif

            SleepMillis(300);
            continue;
        }

        no_scene_count = 0;
        const tmillis_t start_ms = GetTimeInMillis();
        const tmillis_t end_ms = start_ms + scene->get_duration();

        notify_scene_active(scene);

        std::shared_ptr<Scenes::Scene> next_scene;
        const auto transition_duration = resolve_transition_duration(preset, scene);
        const auto transition_name = resolve_transition_name(preset, scene);
        if (should_schedule_transition(transition_duration, scene->get_duration()))
        {
            const auto weighted_scenes = build_weighted_scenes(scenes, is_desktop_connected, scene != nullptr ? scene->get_name() : "");
            next_scene = select_scene(weighted_scenes);
            if (next_scene != nullptr && !next_scene->is_initialized())
            {
                next_scene->initialize(matrix_width, matrix_height);
            }
        }

        // Phase 1: render current scene until transition window or scene end
        bool early_exit = false;
        while (GetTimeInMillis() < end_ms)
        {
            const auto should_continue = scene->render(composite_offscreen_canvas);

            if (!should_continue || interrupt_received || exit_canvas_update)
            {
                trace("Exiting scene early.");
                early_exit = true;
                break;
            }

            if (Constants::global_post_processor)
            {
                Constants::global_post_processor->apply_effects(composite_offscreen_canvas);
            }

            composite_offscreen_canvas = matrix->SwapOnVSync(composite_offscreen_canvas, 1);

#ifdef ENABLE_EMULATOR
            ((rgb_matrix::EmulatorMatrix *)matrix)->Render();
#endif
        }

        // Phase 2: cross-fade to next scene
        if (!early_exit && next_scene != nullptr)
        {
            scene->before_transition_stop();

            tmillis_t transition_start_ms = GetTimeInMillis();
            int iteration = 0;
            while (true)
            {
                const auto now_ms = GetTimeInMillis();
                auto current_continue = true;
                auto next_continue = true;

                if (iteration == 0 || iteration % 2 == 0)
                {
                    current_continue = scene->render(first_offscreen_canvas);
                }

                if(iteration == 0 || iteration % 2 == 1)
                {
                    next_continue = next_scene->render(second_offscreen_canvas);
                }

                iteration++;
                if (!current_continue || !next_continue || interrupt_received || exit_canvas_update)
                {
                    trace("Exiting scene early.");
                    break;
                }

                const auto elapsed_transition = now_ms - transition_start_ms;
                const auto alpha_progress = std::clamp(
                    static_cast<float>(elapsed_transition) / static_cast<float>(std::max<tmillis_t>(1, transition_duration)),
                    0.0f,
                    1.0f);
                apply_transition_frame(composite_offscreen_canvas,
                                       first_offscreen_canvas,
                                       second_offscreen_canvas,
                                       alpha_progress,
                                       matrix_width,
                                       matrix_height,
                                       transition_name);

                if (Constants::global_post_processor)
                {
                    Constants::global_post_processor->apply_effects(composite_offscreen_canvas);
                }

                composite_offscreen_canvas = matrix->SwapOnVSync(composite_offscreen_canvas, 1);

#ifdef ENABLE_EMULATOR
                ((rgb_matrix::EmulatorMatrix *)matrix)->Render();
#endif

                if (alpha_progress >= 1.0f)
                {
                    forced_scene = next_scene;
                    break;
                }
            }
        }

        scene->after_render_stop();
    }
}