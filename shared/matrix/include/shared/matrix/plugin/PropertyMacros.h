#pragma once

#define MAKE_PROPERTY(name, type, default_value) \
    std::unique_ptr<Plugins::Property<type>, void (*)(Plugins::Property<type>*)>( \
        new Plugins::Property<type>(name, default_value, false), \
        [](Plugins::Property<type>* p) { delete p; } \
    )


#define MAKE_PROPERTY_REQ(name, type, default_value) \
    std::unique_ptr<Plugins::Property<type>, void (*)(Plugins::Property<type>*)>( \
        new Plugins::Property<type>(name, default_value, true), \
        [](Plugins::Property<type>* p) { delete p; } \
    )

#define MAKE_PROPERTY_MINMAX(name, type, default_value, min_value, max_value) \
    std::unique_ptr<Plugins::Property<type>, void (*)(Plugins::Property<type>*)>( \
        new Plugins::Property<type>(name, default_value, false, min_value, max_value), \
        [](Plugins::Property<type>* p) { delete p; } \
    )

#define MAKE_PROPERTY_REQ_MINMAX(name, type, default_value, min_value, max_value) \
    std::unique_ptr<Plugins::Property<type>, void (*)(Plugins::Property<type>*)>( \
        new Plugins::Property<type>(name, default_value, true, min_value, max_value), \
        [](Plugins::Property<type>* p) { delete p; } \
    )

// Macro for creating enum properties
#define MAKE_ENUM_PROPERTY(name, enum_type, default_value) \
    std::unique_ptr<Plugins::Property<Plugins::EnumProperty<enum_type>>, void (*)(Plugins::Property<Plugins::EnumProperty<enum_type>>*)>( \
        new Plugins::Property<Plugins::EnumProperty<enum_type>>(name, Plugins::EnumProperty<enum_type>(default_value), false), \
        [](Plugins::Property<Plugins::EnumProperty<enum_type>>* p) { delete p; } \
    )

#define MAKE_ENUM_PROPERTY_REQ(name, enum_type, default_value) \
    std::unique_ptr<Plugins::Property<Plugins::EnumProperty<enum_type>>, void (*)(Plugins::Property<Plugins::EnumProperty<enum_type>>*)>( \
        new Plugins::Property<Plugins::EnumProperty<enum_type>>(name, Plugins::EnumProperty<enum_type>(default_value), true), \
        [](Plugins::Property<Plugins::EnumProperty<enum_type>>* p) { delete p; } \
    )

template<typename N>
using PropertyPointer = std::shared_ptr<Plugins::Property<N>>;//std::unique_ptr<Property<N>, void (*)(Property<N> *)>;