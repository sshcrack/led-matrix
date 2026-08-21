#include "../plugins/AudioVisualizer/desktop/AudioVisualizerDesktop.h"

#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

struct AudioVisualizerDesktopTestAccess {
    static std::unique_lock<std::mutex> lockState(AudioVisualizerDesktop &plugin) {
        return std::unique_lock(plugin.stateMutex);
    }
};

int main() {
    using namespace std::chrono_literals;

    ImGuiContext *imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui);
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1000.0f, 900.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char *pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    AudioVisualizerDesktop plugin;
    plugin.post_init();

    std::promise<void> stateLocked;
    auto stateLockedFuture = stateLocked.get_future();
    std::thread audioWorker([&] {
        auto lock = AudioVisualizerDesktopTestAccess::lockState(plugin);
        stateLocked.set_value();
        std::this_thread::sleep_for(20ms);
    });
    stateLockedFuture.wait();

    ImGui::NewFrame();
    ImGui::Begin("Plugin host");
    const float before = ImGui::GetCursorPosY();
    plugin.render();
    const float after = ImGui::GetCursorPosY();
    ImGui::End();
    ImGui::EndFrame();

    audioWorker.join();
    plugin.before_exit();
    ImGui::DestroyContext(imgui);

    if (after <= before + 1.0f) {
        std::cerr << "AudioVisualizer rendered an empty frame while the audio worker held stateMutex\n";
        return 1;
    }

    std::cout << "AudioVisualizer kept plugin content present during worker contention\n";
    return 0;
}
