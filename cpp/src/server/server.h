#ifndef SERVER_H
#define SERVER_H

#include <restinio/core.hpp>
#include <expected>


using server_t = restinio::http_server_t<>;
using namespace std;

expected<tuple<server_t*, thread>, string> server_mainloop(uint16_t port);

#endif