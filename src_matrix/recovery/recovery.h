#pragma once

#include <optional>

#include "recovery_state.h"

namespace Recovery {
    /// Removes `--recovery=<reason>` from argv; the reason when it was given
    /// (an unknown reason counts as a crash).
    std::optional<Reason> reason_from_args(int &argc, char **argv);

    /// Recovery mode (docs/RECOVERY.md): the error screen on the panel and the
    /// recovery web page, until SIGTERM or an install from that page. Loads no
    /// config, plugins or scenes.
    int run(int argc, char **argv, Reason reason);
}  // namespace Recovery
