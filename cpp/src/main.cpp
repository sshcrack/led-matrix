
#include "server/server.h"
#include <Magick++.h>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "matrix_control/hardware.h"

using namespace spdlog;
using namespace std;


int main(int argc, char *argv[])
{
    Magick::InitializeMagick(*argv);
    spdlog::cfg::load_env_levels();

    debug("Starting mainloop");
    auto server_res = server_mainloop(8080);
    debug("Initializing...");
    auto hardware = initialize_hardware(argc, argv);

    //if(!hardware.has_value())
    //    return hardware.error();

    if(!server_res.has_value()) {
        error("Could not start server: {}", server_res.error());

        return -1;
    }

    sleep(120);
    info("Hardware initialized successfully");

    //hardware->share().wait();

    info("Stopping http server");

    thread t;
    server_t* server = nullptr;

    tie(server, t) = std::move(server_res.value());
    restinio::initiate_shutdown(*server);
    t.join();

    return 0;
}