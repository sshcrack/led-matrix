#pragma once

// Calendar occasions (holidays and the like) a scene may decorate for.
//
// To add one: append an entry to registry() in occasions.cpp with an id, a
// label and a date rule. Nothing else is required: the web UI lists every
// registered occasion, Environment::current() reports the active ones and
// scenes that know the id (Look::occasion("id")) decorate for it.
// Scenes that do not know an occasion simply ignore it.

#include <string>
#include <string_view>
#include <vector>

namespace Environment {

struct CivilDate {
    int year = 2000, month = 1, day = 1;  // month 1-12, day 1-31
};

struct Occasion {
    std::string id;     // stable API id, e.g. "christmas"
    std::string label;  // shown in the web UI
    /// True while the occasion should be shown on this local date.
    bool (*active)(const CivilDate &date) = nullptr;
};

[[nodiscard]] const std::vector<Occasion> &occasions();
[[nodiscard]] const Occasion *find_occasion(std::string_view id);
/// Ids of every occasion active on `date`, in registry order.
[[nodiscard]] std::vector<std::string> active_occasions(const CivilDate &date);
/// Gregorian Easter Sunday of `year` (anonymous Gregorian computus).
[[nodiscard]] CivilDate easter_sunday(int year);

} // namespace Environment
