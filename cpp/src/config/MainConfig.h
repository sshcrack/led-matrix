#pragma once

#include <shared_mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include "data.h"
#include "../resources.h"

using namespace std;
using json = nlohmann::json;

namespace Config {
    class MainConfig {
    private:
        shared_mutex data_mutex;
        ConfigData::Root data;

        shared_mutex update_mutex;
        bool dirty;
        const string &file_name;
    public:
        explicit MainConfig(const string &filename);

        void mark_dirty(bool dirty_local);
        bool is_dirty();

        string get_curr_id();
        ConfigData::Group get_curr();
        map<string, ConfigData::Group> get_groups();

        void setCurr(string id);
        bool save();

    };
}