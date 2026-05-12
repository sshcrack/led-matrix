#pragma once

#ifdef _WIN32
#define PLUGIN_EXPORT [[maybe_unused]] __declspec(dllexport)
#else
#define PLUGIN_EXPORT [[maybe_unused]] __attribute__((visibility("default")))
#endif

#include <memory>
#include <type_traits>

constexpr int MATRIX_PLUGIN_API_VERSION = 1;
constexpr int DESKTOP_PLUGIN_API_VERSION = 1;

template <typename T>
using PluginOwned = std::unique_ptr<T, void (*)(T *)>;

template <typename Concrete, typename Base>
PluginOwned<Base> make_plugin_owned()
{
    static_assert(std::is_base_of_v<Base, Concrete>, "Concrete must derive from Base");
    return {
        new Concrete(),
        [](Base *p)
        {
            delete static_cast<Concrete *>(p);
        }};
}

#define DECLARE_PLUGIN_API_VERSION(symbol_name, version_value) \
    extern "C" PLUGIN_EXPORT int symbol_name() { return version_value; }

#define REGISTER_MATRIX_PLUGIN(ConcreteType) \
    extern "C" PLUGIN_EXPORT Plugins::BasicPlugin *plugin_create() { return new ConcreteType(); } \
    extern "C" PLUGIN_EXPORT void plugin_destroy(Plugins::BasicPlugin *p) { delete static_cast<ConcreteType *>(p); }

#define REGISTER_DESKTOP_PLUGIN(ConcreteType) \
    extern "C" PLUGIN_EXPORT Plugins::DesktopPlugin *plugin_create() { return new ConcreteType(); } \
    extern "C" PLUGIN_EXPORT void plugin_destroy(Plugins::DesktopPlugin *p) { delete static_cast<ConcreteType *>(p); }
