//
// Created by hendrik on 3/9/25.
//

#pragma once
#include <string>


class SongBpmApi {
public:
    [[nodiscard]] static float get_bpm(const std::string &song_name, const std::string &artist_name);
};
