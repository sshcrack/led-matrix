#ifndef MAIN_CONFIG_H
#define MAIN_CONFIG_H
#include <shared_mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include "resources.h"

using namespace std;
using json = nlohmann::json;

class Config {
private:
        shared_mutex json_mutex;
        nlohmann::json json;

        shared_mutex update_mutex;
        bool dirty;
public:
    explicit Config(const string& filename) {
        if(!filesystem::exists(filename)) {
            ofstream file;
            file.open(filename);
            file << jsonDefault;
            file.close();
        }

        ifstream f(filename);
        this->json = json::parse(f);
        this->dirty = false;
    }

    string get_str(const string& key);
    void set_str(const string &key, const string& value);
    void mark_dirty(bool dirty_local);
    bool is_dirty();
};


#endif //MAIN_CONFIG_H
