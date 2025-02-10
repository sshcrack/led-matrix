#pragma once
#include <restinio/core.hpp>
#include <expected>


using server_t = restinio::http_server_t<>;
using namespace std;

restinio::request_handling_status_t req_handler(const restinio::request_handle_t &req);
