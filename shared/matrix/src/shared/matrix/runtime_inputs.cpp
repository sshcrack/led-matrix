#include <shared/matrix/runtime_inputs.h>

#include <algorithm>
#include <mutex>

namespace {
using Clock = std::chrono::steady_clock;

struct StoredInput {
    bool available = false;
    RuntimeInputs::Signals signals;
    Clock::time_point updated_at = Clock::now();
    std::optional<std::chrono::milliseconds> ttl;
};

std::mutex inputs_mutex;
std::unordered_map<std::string, StoredInput> inputs;

nlohmann::json signal_to_json(const RuntimeInputs::SignalValue &value)
{
    return std::visit([](const auto &item) -> nlohmann::json { return item; }, value);
}
} // namespace

namespace RuntimeInputs {

bool Snapshot::available(std::string_view id) const
{
    const auto *state = find(id);
    return state != nullptr && state->available;
}

const InputState *Snapshot::find(std::string_view id) const
{
    const auto it = inputs_.find(std::string(id));
    return it == inputs_.end() ? nullptr : &it->second;
}

std::optional<SignalValue> Snapshot::signal(std::string_view id, std::string_view name) const
{
    const auto *state = find(id);
    if (state == nullptr || !state->available)
        return std::nullopt;
    const auto it = state->signals.find(std::string(name));
    if (it == state->signals.end())
        return std::nullopt;
    return it->second;
}

std::optional<double> Snapshot::number(std::string_view id, std::string_view name) const
{
    const auto value = signal(id, name);
    if (!value.has_value())
        return std::nullopt;
    if (const auto *number = std::get_if<double>(&*value))
        return *number;
    if (const auto *integer = std::get_if<std::int64_t>(&*value))
        return static_cast<double>(*integer);
    return std::nullopt;
}

std::optional<bool> Snapshot::boolean(std::string_view id, std::string_view name) const
{
    const auto value = signal(id, name);
    if (!value.has_value())
        return std::nullopt;
    if (const auto *boolean = std::get_if<bool>(&*value))
        return *boolean;
    return std::nullopt;
}

std::optional<std::string> Snapshot::text(std::string_view id, std::string_view name) const
{
    const auto value = signal(id, name);
    if (!value.has_value())
        return std::nullopt;
    if (const auto *text = std::get_if<std::string>(&*value))
        return *text;
    return std::nullopt;
}

void publish(std::string_view id, Signals signals,
             std::optional<std::chrono::milliseconds> ttl)
{
    set_available(id, true, std::move(signals), ttl);
}

void set_available(std::string_view id, bool available, Signals signals,
                   std::optional<std::chrono::milliseconds> ttl)
{
    if (id.empty())
        return;

    StoredInput input;
    input.available = available;
    input.signals = std::move(signals);
    input.updated_at = Clock::now();
    input.ttl = ttl;

    std::lock_guard lock(inputs_mutex);
    inputs.insert_or_assign(std::string(id), std::move(input));
}

void clear(std::string_view id)
{
    std::lock_guard lock(inputs_mutex);
    inputs.erase(std::string(id));
}

void clear_all()
{
    std::lock_guard lock(inputs_mutex);
    inputs.clear();
}

Snapshot snapshot()
{
    const auto now = Clock::now();
    std::unordered_map<std::string, InputState> result;

    std::lock_guard lock(inputs_mutex);
    result.reserve(inputs.size());
    for (const auto &[id, stored] : inputs) {
        InputState state;
        state.age_seconds = std::max(0.0, std::chrono::duration<double>(now - stored.updated_at).count());
        state.ttl_seconds = stored.ttl.has_value()
            ? std::optional<double>(std::chrono::duration<double>(*stored.ttl).count())
            : std::nullopt;
        state.stale = stored.available && stored.ttl.has_value()
            && now - stored.updated_at > *stored.ttl;
        state.available = stored.available && !state.stale;
        state.signals = stored.signals;
        result.emplace(id, std::move(state));
    }

    return Snapshot(std::move(result));
}

std::vector<std::string> missing_required(
    const Scenes::SceneInputSpec &spec, const Snapshot &snapshot)
{
    std::vector<std::string> missing;
    for (const auto &id : spec.required) {
        if (!snapshot.available(id))
            missing.push_back(id);
    }
    return missing;
}

bool satisfies(const Scenes::SceneInputSpec &spec, const Snapshot &snapshot)
{
    for (const auto &id : spec.required) {
        if (!snapshot.available(id))
            return false;
    }
    return true;
}

nlohmann::json to_json(const Snapshot &snapshot)
{
    nlohmann::json result = nlohmann::json::object();
    for (const auto &[id, state] : snapshot.all()) {
        nlohmann::json signals = nlohmann::json::object();
        for (const auto &[name, value] : state.signals)
            signals[name] = signal_to_json(value);

        nlohmann::json item{
            {"available", state.available},
            {"stale", state.stale},
            {"age_seconds", state.age_seconds},
            {"signals", std::move(signals)}
        };
        if (state.ttl_seconds.has_value())
            item["ttl_seconds"] = *state.ttl_seconds;
        result[id] = std::move(item);
    }
    return result;
}

void replace_from_json(const nlohmann::json &snapshot_json)
{
    if (!snapshot_json.is_object())
        return;

    std::unordered_map<std::string, StoredInput> replacement;
    const auto now = Clock::now();
    for (const auto &[id, item] : snapshot_json.items()) {
        if (!item.is_object() || id.empty())
            continue;
        StoredInput stored;
        stored.available = item.value("available", false);
        const double source_age_seconds = item.contains("age_seconds") && item.at("age_seconds").is_number()
            ? std::max(0.0, item.at("age_seconds").get<double>())
            : 0.0;
        stored.updated_at = now - std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(source_age_seconds));
        if (item.contains("ttl_seconds") && item.at("ttl_seconds").is_number()) {
            const double seconds = std::max(0.0, item.at("ttl_seconds").get<double>());
            stored.ttl = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::duration<double>(seconds));
        }
        const auto signals_it = item.find("signals");
        if (signals_it != item.end() && signals_it->is_object()) {
            for (const auto &[name, value] : signals_it->items()) {
                if (value.is_boolean()) stored.signals.emplace(name, value.get<bool>());
                else if (value.is_number_integer() || value.is_number_unsigned())
                    stored.signals.emplace(name, value.get<std::int64_t>());
                else if (value.is_number_float()) stored.signals.emplace(name, value.get<double>());
                else if (value.is_string()) stored.signals.emplace(name, value.get<std::string>());
            }
        }
        replacement.emplace(id, std::move(stored));
    }

    std::lock_guard lock(inputs_mutex);
    inputs = std::move(replacement);
}

} // namespace RuntimeInputs
