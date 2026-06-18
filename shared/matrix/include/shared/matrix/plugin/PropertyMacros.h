#pragma once

#define MAKE_PROPERTY(name, type, default_value) \
    std::shared_ptr<Plugins::Property<type>>(new Plugins::Property<type>(name, default_value, false))


#define MAKE_PROPERTY_REQ(name, type, default_value) \
    std::shared_ptr<Plugins::Property<type>>(new Plugins::Property<type>(name, default_value, true))

#define MAKE_PROPERTY_MINMAX(name, type, default_value, min_value, max_value) \
    std::shared_ptr<Plugins::Property<type>>(new Plugins::Property<type>(name, default_value, false, min_value, max_value))

#define MAKE_PROPERTY_REQ_MINMAX(name, type, default_value, min_value, max_value) \
    std::shared_ptr<Plugins::Property<type>>(new Plugins::Property<type>(name, default_value, true, min_value, max_value))

#define MAKE_ENUM_PROPERTY(name, enum_type, default_value) \
    std::shared_ptr<Plugins::Property<Plugins::EnumProperty<enum_type>>>(new Plugins::Property<Plugins::EnumProperty<enum_type>>(name, Plugins::EnumProperty<enum_type>(default_value), false))

#define MAKE_ENUM_PROPERTY_REQ(name, enum_type, default_value) \
    std::shared_ptr<Plugins::Property<Plugins::EnumProperty<enum_type>>>(new Plugins::Property<Plugins::EnumProperty<enum_type>>(name, Plugins::EnumProperty<enum_type>(default_value), true))

#define MAKE_STRING_LIST_PROPERTY(name, default_value) \
    std::shared_ptr<Plugins::Property<std::vector<std::string>>>(new Plugins::Property<std::vector<std::string>>(name, default_value, false))

#define MAKE_STRING_LIST_PROPERTY_REQ(name, default_value) \
    std::shared_ptr<Plugins::Property<std::vector<std::string>>>(new Plugins::Property<std::vector<std::string>>(name, default_value, true))

template<typename N>
using PropertyPointer = std::shared_ptr<Plugins::Property<N>>;
