#include <expected>
#include <future>

std::expected<std::future<void>, int> initialize_hardware(int argc, char *argv[]);