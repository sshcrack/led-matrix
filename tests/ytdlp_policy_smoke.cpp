#include <shared/desktop/utils.h>

#include <iostream>
#include <string_view>

int main()
{
    const std::string_view selector = get_ytdlp_video_format_selector();
    if (selector.find("bestvideo") == std::string_view::npos) {
        std::cerr << "display-video yt-dlp policy still requires a progressive best format: "
                  << selector << '\n';
        return 1;
    }
    if (selector.find("best[ext=mp4]/best") != std::string_view::npos) {
        std::cerr << "obsolete progressive-only yt-dlp fallback is still present\n";
        return 2;
    }

#ifdef _WIN32
    const auto command = run_command_capture("echo ERROR: useful child diagnostic 1>&2 & exit /b 7");
#else
    const auto command = run_command_capture("printf 'noise\nERROR: useful child diagnostic\n' >&2; exit 7");
#endif
    if (command.exit_code != 7 || command.output.find("ERROR: useful child diagnostic") == std::string::npos) {
        std::cerr << "external-tool command diagnostics were not captured (exit="
                  << command.exit_code << ", output=" << command.output << ")\n";
        return 3;
    }

    std::cout << "yt-dlp display-video policy accepts modern video-only formats\n";
    return 0;
}
