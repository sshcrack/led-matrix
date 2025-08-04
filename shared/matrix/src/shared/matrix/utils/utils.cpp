#include "shared/matrix/utils/utils.h"
#include "shared/matrix/utils/shared.h"
#include <iostream>
#include <expected>
#include <thread>
#include <regex>
#include "spdlog/spdlog.h"
#include "shared/matrix/canvas_consts.h"
#include <random>

using namespace spdlog;

void SleepMillis(tmillis_t milli_seconds)
{
    if (milli_seconds <= 0)
        return;
    tmillis_t end_time = GetTimeInMillis() + milli_seconds;

    while (GetTimeInMillis() < end_time)
    {
        try
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        catch (std::exception &e)
        {
            error("Sleep interrupted: {}", e.what());
            break;
        }

        if (skip_image || exit_canvas_update)
        {
            skip_image.store(false);
            break;
        }

        if(Constants::isRenderingSceneInitially)
        {
            break;
        }
    }
}

std::expected<std::string, std::string> execute_process(const string &cmd, const vector<std::string> &args)
{
    std::stringstream fullCmd;
    fullCmd << cmd;

    for (const auto &arg : args)
    {
        fullCmd << " " << arg;
    }

    char out_file[] = "/tmp/spotify.XXXXXX";
    int temp_fd = mkstemp(out_file);

    if (temp_fd == -1)
    {
        return unexpected("Could not create temporary file");
    }

    // Redirect output of the command appropriately
    std::ostringstream ss;
    ss << fullCmd.str() << " >" << out_file;
    const auto finalCmd = ss.str();

    // Call the command
    const auto status = std::system(finalCmd.c_str());

    if (status != 0)
    {
        cout << "Status " << status << " \n";
        return unexpected("Command failed");
    }

    // Read the output from the files and remove them
    std::stringstream out_stream;

    std::ifstream file(out_file);
    if (file)
    {
        out_stream << file.rdbuf();
        file.close();
    }

    close(temp_fd);
    std::remove(out_file);

    return out_stream.str();
}

void floatPixelSet(rgb_matrix::FrameCanvas *canvas, int x, int y, float r, float g, float b)
{
    const uint8_t rByte = static_cast<uint8_t>(max(0.0f, min(1.0f, r)) * 255.0f);
    const uint8_t gByte = static_cast<uint8_t>(max(0.0f, min(1.0f, g)) * 255.0f);
    const uint8_t bByte = static_cast<uint8_t>(max(0.0f, min(1.0f, b)) * 255.0f);

    canvas->SetPixel(x, y, rByte, gByte, bByte);
}

std::vector<uint8_t> magick_to_rgb(const Magick::Image &img)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(img.columns() * img.rows() * 3);

    for (int y = 0; y < img.columns(); ++y)
    {
        for (int x = 0; x < img.rows(); ++x)
        {
            const Magick::Color &c = img.pixelColor(x, y);
            float alpha = 1.0f - (static_cast<float>(ScaleQuantumToChar(c.alphaQuantum())) / 255.0f);

            buffer.push_back(
                static_cast<unsigned char>(static_cast<float>(ScaleQuantumToChar(c.redQuantum())) * alpha));
            buffer.push_back(
                static_cast<unsigned char>(static_cast<float>(ScaleQuantumToChar(c.greenQuantum())) * alpha));
            buffer.push_back(
                static_cast<unsigned char>(static_cast<float>(ScaleQuantumToChar(c.blueQuantum())) * alpha));
        }
    }

    return buffer;
}
