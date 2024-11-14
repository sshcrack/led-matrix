#pragma once

#include <cstdint>
#include <filesystem>
#include "led-matrix.h"
#include <expected>
#include <optional>
#include <vector>
#include <Magick++.h>

typedef int64_t tmillis_t;

tmillis_t GetTimeInMillis();
void SleepMillis(tmillis_t milli_seconds);
bool try_remove(const std::filesystem::path&);

void floatPixelSet(rgb_matrix::FrameCanvas* canvas, int x, int y, float r, float g, float b);
bool is_valid_filename(const std::string& filename);
bool replace(std::string &str, const std::string &from, const std::string &to);
std::string stringify_url(const std::string& url);
std::optional<std::string> get_exec_dir();

int get_random_number_inclusive(int start, int end);
std::expected<std::string,std::string> execute_process(const std::string& cmd, const std::vector<std::string>& args);

std::vector<uint8_t> magick_to_rgb(const Magick::Image& img);
