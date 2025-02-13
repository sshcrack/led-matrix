#pragma once

#define MAKE_PROPERTY(name, type, default_value) \
    std::unique_ptr<Property<type>, void (*)(Property<type>*)>( \
        new Property<type>(name, default_value, false), \
        [](Property<type>* p) { delete p; } \
    )


#define MAKE_PROPERTY_REQ(name, type, default_value) \
    std::unique_ptr<Property<type>, void (*)(Property<type>*)>( \
        new Property<type>(name, default_value, true), \
        [](Property<type>* p) { delete p; } \
    )

template<typename N>
using PropertyPointer = std::shared_ptr<Property<N>>;//std::unique_ptr<Property<N>, void (*)(Property<N> *)>;