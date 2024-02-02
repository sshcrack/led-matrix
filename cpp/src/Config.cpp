#include "Config.h"
#include <mutex>

string Config::get_str(const string& key) {
    shared_lock<shared_mutex> lock(this->json_mutex);
    return this->json[key];
}

void Config::set_str(const string& key, const string& value) {
    unique_lock<shared_mutex> lock(this->json_mutex);
    this->json[key] = value;
}

void Config::mark_dirty(bool dirty_local) {
    unique_lock<shared_mutex> lock(this->update_mutex);
    this->dirty = dirty_local;
}

bool Config::is_dirty() {
    shared_lock<shared_mutex> lock(this->update_mutex);
    return this->dirty;
}
