#pragma once

#include <cstdint>

namespace SceneExecution {

enum class Mode : std::uint8_t {
    Matrix = 0,
    Preview,
    RemoteWorker,
};

[[nodiscard]] Mode mode();
void set_mode(Mode mode);
[[nodiscard]] inline bool is_preview() { return mode() == Mode::Preview; }
[[nodiscard]] inline bool is_remote_worker() { return mode() == Mode::RemoteWorker; }
[[nodiscard]] inline bool is_headless_fixture_host() {
    return mode() == Mode::Preview || mode() == Mode::RemoteWorker;
}

class Scope {
public:
    explicit Scope(Mode requested);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
private:
    Mode previous_;
};

} // namespace SceneExecution
