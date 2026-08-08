#include <shared/matrix/plugin/property.h>

#include <iostream>

int main()
{
    Plugins::Property<int> property("num_particles", 40, false, 1, 12000);
    property.legacy_name("numParticles");

    property.load_from_json(nlohmann::json{{"numParticles", 77}});
    if (property.get() != 77) {
        std::cerr << "legacy property name was not loaded\n";
        return 1;
    }

    nlohmann::json serialized = nlohmann::json::object();
    property.dump_to_json(serialized);
    if (serialized.value("num_particles", 0) != 77 || serialized.contains("numParticles")) {
        std::cerr << "property did not serialize with canonical name only\n";
        return 1;
    }

    property.load_from_json(nlohmann::json{{"numParticles", 55}, {"num_particles", 99}});
    if (property.get() != 99) {
        std::cerr << "canonical property name did not take precedence\n";
        return 1;
    }

    return 0;
}
