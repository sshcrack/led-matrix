// scraper.cpp

#include <Magick++.h>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "matrix_control/hardware.h"
#include "restino/core.hpp"

using namespace spdlog;
using namespace std;


int main(int argc, char *argv[])
{
    Magick::InitializeMagick(*argv);
    spdlog::cfg::load_env_levels();

    auto hardware = initialize_hardware(argc, argv);

    if(!hardware.has_value())
        return hardware.error();

    info("Hardware initialized successfully");
    hardware->share().wait();
    info("Exiting mainloop");

    restinio::run(
            restinio::on_this_thread()
                    .port(8080)
                    .address("localhost")
                    .request_handler([](auto req) {
                        return req->create_response().set_body("Hello, World!").done();
                    }));

    return 0;
}