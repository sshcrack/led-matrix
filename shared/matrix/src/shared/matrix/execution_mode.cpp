#include <shared/matrix/execution_mode.h>

#include <atomic>

namespace {
std::atomic<SceneExecution::Mode> current_mode{SceneExecution::Mode::Matrix};
}

SceneExecution::Mode SceneExecution::mode()
{
    return current_mode.load(std::memory_order_relaxed);
}

void SceneExecution::set_mode(const Mode mode)
{
    current_mode.store(mode, std::memory_order_relaxed);
}

SceneExecution::Scope::Scope(const Mode requested) : previous_(mode())
{
    set_mode(requested);
}

SceneExecution::Scope::~Scope()
{
    set_mode(previous_);
}
