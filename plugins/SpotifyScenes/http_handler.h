#pragma once
#include <optional>
#include <restinio/all.hpp>

std::optional<restinio::request_handling_status_t> handle_request(const restinio::request_handle_t &req);