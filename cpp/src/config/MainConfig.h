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
    public:
        explicit MainConfig(const string &filename) {
            if (!filesystem::exists(filename)) {
                ofstream file;
                file.open(filename);
                file << jsonDefault;
                file.close();
            }

            ifstream f(filename);
            json temp = json::parse(f);

            this->data = temp.template get<ConfigData::Root>();
            this->dirty = false;
        }

        void mark_dirty(bool dirty_local);
        bool is_dirty();

        string get_curr_id();
        ConfigData::Group get_curr();
        map<string, ConfigData::Group> get_groups();

        void setCurr(string id);
    };
}