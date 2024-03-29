#include <cstdint>
#include <filesystem>
#include <expected>
#include <vector>

typedef int64_t tmillis_t;

tmillis_t GetTimeInMillis();
void SleepMillis(tmillis_t milli_seconds);
bool try_remove(const std::filesystem::path&);

bool is_valid_filename(const std::string& filename);
bool replace(std::string &str, const std::string &from, const std::string &to);

std::expected<std::string,std::string> execute_process(const std::string& cmd, const std::vector<std::string>& args);