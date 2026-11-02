#include <shared/matrix/utils/image_fetch.h>
#include <shared/matrix/post.h>
#include <shared/matrix/utils/consts.h>
#include <thread>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 4) return 2;
    const std::string mode=argv[3];
    if (mode=="post-retry" || mode=="post-cleanup") {
        Magick::InitializeMagick(argv[0]);
        std::filesystem::create_directories(Constants::post_dir);
        Post post(argv[1],false);
        if (mode=="post-retry" && post.process_images(16,16)) return 3;
        if (std::filesystem::exists(Constants::post_dir/post.get_filename())) return 4;
        if (!post.process_images(16,16)) return 5;
        return std::filesystem::exists(Constants::post_dir/post.get_filename()) ? 6 : 0;
    }
    std::stop_source stop;
    std::jthread cancellation;
    if (mode=="cancel") cancellation=std::jthread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); stop.request_stop();
    });
    auto result=utils::download_image(std::string(argv[1]),std::string(argv[2]),stop.get_token());
    if (!result) std::cerr << result.error() << '\n';
    return result ? 0 : 1;
}
