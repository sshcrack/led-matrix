#include "shared/matrix/preview.h"

namespace {
std::atomic<bool> preview_active{false};
}

bool Previews::Runtime::active()
{
    return preview_active.load(std::memory_order_relaxed);
}

void Previews::Runtime::set_active(const bool active)
{
    preview_active.store(active, std::memory_order_relaxed);
}
