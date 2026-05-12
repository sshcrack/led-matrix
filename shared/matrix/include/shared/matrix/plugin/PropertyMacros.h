#pragma once

#include "shared/common/plugin_macros.h"

#define MAKE_PROPERTY(name, type, default_value) \
    PluginOwned<Plugins::Property<type>>( \
        new Plugins::Property<type>(name, default_value, false), \
        [](Plugins::Property<type>* p) { delete p; } \
    )


#define MAKE_PROPERTY_REQ(name, type, default_value) \
    PluginOwned<Plugins::Property<type>>( \
        new Plugins::Property<type>(name, default_value, true), \
        [](Plugins::Property<type>* p) { delete p; } \
    )

#define MAKE_PROPERTY_MINMAX(name, type, default_value, min_value, max_value) \
    PluginOwned<Plugins::Property<type>>( \
        new Plugins::Property<type>(name, default_value, false, min_value, max_value), \
        [](Plugins::Property<type>* p) { delete p; } \
    )

#define MAKE_PROPERTY_REQ_MINMAX(name, type, default_value, min_value, max_value) \
    PluginOwned<Plugins::Property<type>>( \
        new Plugins::Property<type>(name, default_value, true, min_value, max_value), \
        [](Plugins::Property<type>* p) { delete p; } \
    )

// Macro for creating enum properties
#define MAKE_ENUM_PROPERTY(name, enum_type, default_value) \
    PluginOwned<Plugins::Property<Plugins::EnumProperty<enum_type>>>( \
        new Plugins::Property<Plugins::EnumProperty<enum_type>>(name, Plugins::EnumProperty<enum_type>(default_value), false), \
        [](Plugins::Property<Plugins::EnumProperty<enum_type>>* p) { delete p; } \
    )


#define MAKE_ENUM_PROPERTY_REQ(name, enum_type, default_value) \
    PluginOwned<Plugins::Property<Plugins::EnumProperty<enum_type>>>( \
        new Plugins::Property<Plugins::EnumProperty<enum_type>>(name, Plugins::EnumProperty<enum_type>(default_value), true), \
        [](Plugins::Property<Plugins::EnumProperty<enum_type>>* p) { delete p; } \
    )

#define MAKE_STRING_LIST_PROPERTY(name, default_value) \
    PluginOwned<Plugins::Property<std::vector<std::string>>>( \
        new Plugins::Property<std::vector<std::string>>(name, default_value, false), \
        [](Plugins::Property<std::vector<std::string>>* p) { delete p; } \
    )

#define MAKE_STRING_LIST_PROPERTY_REQ(name, default_value) \
    PluginOwned<Plugins::Property<std::vector<std::string>>>( \
        new Plugins::Property<std::vector<std::string>>(name, default_value, true), \
        [](Plugins::Property<std::vector<std::string>>* p) { delete p; } \
    )

template<typename N>
using PropertyPointer = std::shared_ptr<Plugins::Property<N>>;//std::unique_ptr<Property<N>, void (*)(Property<N> *)>;
