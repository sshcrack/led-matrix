#include <cstdint>
#include <filesystem>

typedef int64_t tmillis_t;

tmillis_t GetTimeInMillis();
void SleepMillis(tmillis_t milli_seconds);
bool try_remove(const std::filesystem::path&);