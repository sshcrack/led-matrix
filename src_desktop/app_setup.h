#pragma once

#include <string>

namespace desktop_app::detail {
#ifndef WIN32
    /// Looks up the Noto Color Emoji font file via fontconfig (empty if not found).
    std::string get_noto_color_emoji_path();
#endif

    /// Configures spdlog sinks and the HelloImGui assets folder.
    void setup_logging();

    /// Initializes the plugin manager and loads each plugin's saved config.
    void init_plugins();

    /// HelloImGui LoadAdditionalFonts callback body.
    void load_additional_fonts();
}
