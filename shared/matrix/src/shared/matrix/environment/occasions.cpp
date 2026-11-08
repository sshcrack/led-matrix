#include <shared/matrix/environment/occasions.h>

#include <chrono>

namespace Environment {
namespace {
int day_number(const CivilDate &d)
{
    using namespace std::chrono;
    return static_cast<int>(sys_days{year{d.year} / month{static_cast<unsigned>(d.month)} / day{static_cast<unsigned>(d.day)}}
                                .time_since_epoch()
                                .count());
}

bool between(const CivilDate &d, int m0, int d0, int m1, int d1)
{
    const int v = d.month * 100 + d.day;
    return v >= m0 * 100 + d0 && v <= m1 * 100 + d1;
}

bool christmas(const CivilDate &d) { return between(d, 12, 1, 12, 26); }
bool new_years_eve(const CivilDate &d) { return between(d, 12, 31, 12, 31) || between(d, 1, 1, 1, 1); }
bool halloween(const CivilDate &d) { return between(d, 10, 25, 10, 31); }
bool easter(const CivilDate &d)
{
    // Holy week through Easter Monday.
    const int offset = day_number(d) - day_number(easter_sunday(d.year));
    return offset >= -7 && offset <= 1;
}
} // namespace

CivilDate easter_sunday(int y)
{
    const int a = y % 19, b = y / 100, c = y % 100, d = b / 4, e = b % 4;
    const int f = (b + 8) / 25, g = (b - f + 1) / 3, h = (19 * a + b - d - g + 15) % 30;
    const int i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m = (a + 11 * h + 22 * l) / 451;
    const int month = (h + l - 7 * m + 114) / 31;
    const int day = (h + l - 7 * m + 114) % 31 + 1;
    return {y, month, day};
}

const std::vector<Occasion> &occasions()
{
    static const std::vector<Occasion> registry{
        {"christmas", "Christmas", christmas},
        {"new_years_eve", "New Year's Eve", new_years_eve},
        {"halloween", "Halloween", halloween},
        {"easter", "Easter", easter},
    };
    return registry;
}

const Occasion *find_occasion(std::string_view id)
{
    for (const auto &o : occasions())
        if (o.id == id)
            return &o;
    return nullptr;
}

std::vector<std::string> active_occasions(const CivilDate &date)
{
    std::vector<std::string> ids;
    for (const auto &o : occasions())
        if (o.active != nullptr && o.active(date))
            ids.push_back(o.id);
    return ids;
}

} // namespace Environment
