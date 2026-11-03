#include <shared/matrix/ambiance_packs.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/audio_state.h>
#include <emulator.h>
#include <iostream>
#include <memory>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#define CHECK(c) do { if (!(c)) { std::cerr << "Failed line " << __LINE__ << ": " << #c << '\n'; return 1; } } while (0)
int main(int argc, char **argv) {
    auto *manager = Plugins::PluginManager::instance();
    manager->initialize();
    const auto packs = Ambiance::catalog();
    CHECK(packs.size() == 3);
    rgb_matrix::RGBMatrix::Options options; options.rows = options.cols = 64;
    rgb_matrix::EmulatorOptions emulator; emulator.headless = true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(options, emulator));
    CHECK(matrix);
    auto *canvas = matrix->CreateFrameCanvas();
    std::unordered_set<std::string> ids;
    for (const auto &definition : packs) {
        CHECK(definition.at("available").get<bool>());
        const auto id = definition.at("id").get<std::string>();
        CHECK(ids.insert(id).second);
        auto preset = Ambiance::create_preset(id);
        auto copy = Ambiance::create_preset(id);
        CHECK(preset->scenes.size() == 2 && preset->transition_name == "blend");
        CHECK(preset->scenes[0]->get_uuid() != copy->scenes[0]->get_uuid());
        double red = 0, green = 0, blue = 0;
        std::size_t lit = 0, pixels = 0;
        std::size_t scene_index = 0;
        for (auto &scene : preset->scenes) {
            CHECK(!scene->get_capabilities().requires_desktop && !scene->get_capabilities().requires_network);
            CHECK(scene->get_effective_runtime_inputs().required.empty());
            CHECK(scene->get_duration() > preset->transition_duration * 10);
            CHECK(scene->to_json().at("look_palette") != "ORIGINAL");
            CHECK(scene->to_json().at("look_brightness").get<float>() <= 0.55f);
            canvas->Fill(255,0,255);
            scene->set_random_seed(42);
            scene->initialize(64, 64);
            if (scene->get_name() == "rain") {
                CHECK(scene->render_frame(canvas, 0.0, true));
                for (int y=0; y<64; ++y) for (int x=0; x<64; ++x) {
                    uint8_t r=0,g=0,b=0; canvas->GetPixel(x,y,&r,&g,&b);
                    CHECK(r != 255 || g != 0 || b != 255);
                }
            }
            std::size_t scene_lit = 0;
            for (int frame = 0; frame < 48; ++frame) {
                CHECK(scene->render_frame(canvas, 1.0 / 24.0, true));
                for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) {
                    uint8_t r = 0, g = 0, b = 0; CHECK(canvas->GetPixel(x, y, &r, &g, &b));
                    red += r; green += g; blue += b;
                    if (r + g + b > 6) { ++lit; ++scene_lit; }
                    ++pixels;
                }
            }
            CHECK(scene_lit > 100);
            if (argc > 1) {
                std::filesystem::create_directories(argv[1]);
                std::ofstream image(std::filesystem::path(argv[1]) / (id + "-" + scene->get_name() + ".ppm"), std::ios::binary);
                image << "P6\n64 64\n255\n";
                for (int y=0; y<64; ++y) for (int x=0; x<64; ++x) {
                    uint8_t pixel[3]{}; canvas->GetPixel(x,y,&pixel[0],&pixel[1],&pixel[2]);
                    image.write(reinterpret_cast<char *>(pixel),3);
                }
            }
            std::vector<uint8_t> expected(64 * 64 * 3);
            for (int y=0; y<64; ++y) for (int x=0; x<64; ++x) {
                auto *pixel = expected.data() + (y*64+x)*3;
                canvas->GetPixel(x,y,&pixel[0],&pixel[1],&pixel[2]);
            }
            scene->after_render_stop();
            auto &repeated = copy->scenes[scene_index++];
            repeated->set_random_seed(42);
            repeated->initialize(64, 64);
            if (repeated->get_name() == "rain") CHECK(repeated->render_frame(canvas, 0.0, true));
            for (int frame=0; frame<48; ++frame) CHECK(repeated->render_frame(canvas, 1.0 / 24.0, true));
            for (int y=0; y<64; ++y) for (int x=0; x<64; ++x) {
                uint8_t r=0,g=0,b=0; canvas->GetPixel(x,y,&r,&g,&b);
                const auto *pixel = expected.data() + (y*64+x)*3;
                CHECK(r == pixel[0] && g == pixel[1] && b == pixel[2]);
            }
            repeated->after_render_stop();
        }
        const double mean = (red + green + blue) / (pixels * 255.0 * 3);
        CHECK(mean > 0.00005 && mean < 0.18); // Catch black output and whole-panel blowouts.
        CHECK(lit < pixels * 0.65); // Retain the negative space that makes these packs restful.
        if (id == "quiet-evening") CHECK(red > green * 1.3 && green > blue * 1.2);
        else CHECK(blue > red * 1.3);
        std::cout << id << ": visual checks passed (mean=" << mean << ")\n";
    }
    try { (void)Ambiance::create_preset("missing"); CHECK(false); } catch (const std::invalid_argument &) {}
    return 0;
}
