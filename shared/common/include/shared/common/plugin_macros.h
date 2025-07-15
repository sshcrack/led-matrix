#ifdef _WIN32
#define PLUGIN_EXPORT [[maybe_unused]] __declspec(dllexport)
#else
#define PLUGIN_EXPORT [[maybe_unused]] __attribute__((visibility("default")))
#endif