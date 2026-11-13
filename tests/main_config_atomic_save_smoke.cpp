// MainConfig::save() must never expose a truncated file to a concurrent
// reader. Scene workers started in parallel share one temp config, and a
// worker that parsed it mid-write exited before printing anything (#180).
#include <shared/matrix/config/MainConfig.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>

namespace fs = std::filesystem;

int main()
{
    spdlog::set_level(spdlog::level::off);
    const auto dir = fs::temp_directory_path() / ("led-matrix-config-smoke-" + std::to_string(std::random_device{}()));
    fs::remove_all(dir);
    const auto path = (dir / "config.json").string();

    int failures = 0;
    {
        // A missing file starts from defaults and is written once, complete.
        Config::MainConfig writer(path);
        if (!fs::exists(path) || writer.get_presets().empty()) {
            std::cerr << "fresh config was not created with a default preset\n";
            ++failures;
        }

        std::atomic<bool> done{false};
        std::thread saver([&] {
            for (int i = 0; i < 400; ++i) writer.save();
            done = true;
        });

        int reads = 0, bad_reads = 0;
        while (!done || reads < 50) {
            std::ifstream in(path);
            try {
                if (!json::parse(in).is_object())
                    throw std::runtime_error("config is not an object");
                Config::MainConfig reader(path);
            }
            catch (const std::exception& e) {
                if (bad_reads++ == 0)
                    std::cerr << "read during save failed: " << e.what() << '\n';
            }
            ++reads;
        }
        saver.join();
        if (bad_reads) {
            std::cerr << bad_reads << " of " << reads << " reads saw a partial config\n";
            ++failures;
        }

        for (const auto& entry : fs::directory_iterator(dir))
            if (entry.path().filename() != "config.json") {
                std::cerr << "leftover temp file: " << entry.path() << '\n';
                ++failures;
            }
    }

    fs::remove_all(dir);
    if (failures)
        return 1;
    std::cout << "ok\n";
    return 0;
}
