#pragma once

namespace Constants {
    const static std::filesystem::path root_dir = std::filesystem::current_path() / "images";
    const static std::filesystem::path post_dir = root_dir / "processed";
    const static std::filesystem::path upload_dir = root_dir / "uploads";
}
