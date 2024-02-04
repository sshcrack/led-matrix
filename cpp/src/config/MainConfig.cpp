#include "MainConfig.h"
#include <mutex>
#include <utility>


namespace Config {
    void MainConfig::mark_dirty(bool dirty_local) {
        unique_lock<shared_mutex> lock(this->update_mutex);
        this->dirty = dirty_local;
    }

    bool MainConfig::is_dirty() {
        shared_lock<shared_mutex> lock(this->update_mutex);
        return this->dirty;
    }

    string MainConfig::get_curr() {
        shared_lock<shared_mutex> lock(this->data_mutex);

        return this->data.curr;
    }

    void MainConfig::setCurr(string id) {
        unique_lock<shared_mutex> lock(this->data_mutex);

        this->data.curr = std::move(id);
        this->mark_dirty(true);
    }

    map<string, ConfigData::Groups> MainConfig::get_groups() {
        shared_lock<shared_mutex> lock(this->data_mutex);
        return this->data.groups;
    }
}
