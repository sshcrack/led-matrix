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
    }

    string get_str(const string& key);
    void set_str(const string &key, const string& value);
};


#endif //MAIN_CONFIG_H
