#include "daemon.h"

#include "spdlog/spdlog.h"

int main(int argc, char *argv[])
{
    try {
        Daemon daemon(argc, argv);
        return daemon.run();
    } catch (const std::exception &e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }
}
