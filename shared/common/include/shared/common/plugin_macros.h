#ifdef _WIN32
#define PLUGIN_EXPORT [[maybe_unused]] __declspec(dllexport)
#else
#define PLUGIN_EXPORT [[maybe_unused]] __attribute__((visibility("default")))
#endif

#define REGISTER_PLUGIN(Name, Type)                                                  \
    extern "C" PLUGIN_EXPORT Type *create##Name() { return new Type(); }             \
    extern "C" PLUGIN_EXPORT void destroy##Name(Type *c) { delete c; }

#define REGISTER_PLUGIN_CUSTOM_DESTROY(Name, Type, DestroyBody)                      \
    extern "C" PLUGIN_EXPORT Type *create##Name() { return new Type(); }             \
    extern "C" PLUGIN_EXPORT void destroy##Name(Type *c) { DestroyBody; }

