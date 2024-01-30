#include "led-matrix.h"
#include "pixel_art.h"
#include "interrupt.h"
#include "image.h"
#include "canvas.h"
#include <sys/time.h>
#include "spdlog/spdlog.h"

#include <random>
#include <Magick++.h>

using namespace std;
using namespace spdlog;

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;



tmillis_t GetTimeInMillis() {
    struct timeval tp{};
    gettimeofday(&tp, nullptr);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

void SleepMillis(tmillis_t milli_seconds) {
    if (milli_seconds <= 0) return;
    struct timespec ts{};
    ts.tv_sec = milli_seconds / 1000;
    ts.tv_nsec = (milli_seconds % 1000) * 1000000;
    nanosleep(&ts, nullptr);
}

void StoreInStream(const Magick::Image &img, int64_t delay_time_us,
                          bool do_center,
                          rgb_matrix::FrameCanvas *scratch,
                          rgb_matrix::StreamWriter *output) {
    scratch->Clear();
    const int x_offset = do_center ? (scratch->width() - img.columns()) / 2 : 0;
    const int y_offset = do_center ? (scratch->height() - img.rows()) / 2 : 0;
    for (size_t y = 0; y < img.rows(); ++y) {
        for (size_t x = 0; x < img.columns(); ++x) {
            const Magick::Color &c = img.pixelColor(x, y);
            if (c.alphaQuantum() < 255) {
                scratch->SetPixel(x + x_offset, y + y_offset,
                                  MagickCore::ScaleQuantumToChar(c.redQuantum()),
                                  MagickCore::ScaleQuantumToChar(c.greenQuantum()),
                                  MagickCore::ScaleQuantumToChar(c.blueQuantum()));
            }
        }
    }
    output->Stream(*scratch, delay_time_us);
}

void update_canvas(FrameCanvas *canvas, RGBMatrix *matrix, int page_end) {
    const int height = canvas->height();
    const int width = canvas->width();


    int start = 1;

    random_device rd; // obtain a random number from hardware
    mt19937 gen(rd()); // seed the generator
    uniform_int_distribution<> distr(start, page_end); // define the range
    auto posts = get_posts(distr(gen));

    for (auto &item: posts) {
        if(interrupt_received)
            break;

        item.fetch_link();
        if(!item.image.has_value()) {
            error("Could not load image {}", item.url);
            continue;
        }

        string img_url = item.image.value();
        string out_file = img_url.substr(img_url.find_last_of('/') +1);

        // Downloading image first
        download_image(img_url, out_file);

        vector<Magick::Image> frames;
        string err_msg;
        if (!LoadImageAndScale(out_file.c_str(), width, height, true, true, &frames, &err_msg)) {
            error("Error loading image: {}", err_msg);
            continue;
        }


        FileInfo *file_info;

        ImageParams params = ImageParams();
        if(frames.size() > 1) {
            params.anim_duration_ms = 15000;
        } else {
            params.wait_ms = 5000;
        }

        file_info = new FileInfo();
        file_info->params = params;
        file_info->content_stream = new rgb_matrix::MemStreamIO();
        file_info->is_multi_frame = frames.size() > 1;
        rgb_matrix::StreamWriter out(file_info->content_stream);
        for (const auto & img : frames) {
            tmillis_t delay_time_us;
            if (file_info->is_multi_frame) {
                delay_time_us = img.animationDelay() * 10000; // unit in 1/100s
            } else {
                delay_time_us = file_info->params.wait_ms * 1000;  // single image.
            }
            if (delay_time_us <= 0) delay_time_us = 100 * 1000;  // 1/10sec
            StoreInStream(img, delay_time_us, true, canvas, &out);
        }


        info("Showing animation for {} ({})", img_url, out_file);
        DisplayAnimation(file_info, matrix, canvas);
        remove(out_file.c_str());
    }
}


void DisplayAnimation(const FileInfo *file,
                      RGBMatrix *matrix, FrameCanvas *offscreen_canvas) {
    const tmillis_t duration_ms = (file->is_multi_frame
                                   ? file->params.anim_duration_ms
                                   : file->params.wait_ms);

    rgb_matrix::StreamReader reader(file->content_stream);

    const tmillis_t end_time_ms = GetTimeInMillis() + duration_ms;
    const tmillis_t override_anim_delay = file->params.anim_delay_ms;
    while (!interrupt_received && GetTimeInMillis() < end_time_ms) {
        uint32_t delay_us = 0;
        bool success_reading = reader.GetNext(offscreen_canvas, &delay_us);

        if(!success_reading) {
            reader.Rewind();
            continue;
        }

        const tmillis_t anim_delay_ms =
                override_anim_delay >= 0 ? override_anim_delay : delay_us / 1000;

        const tmillis_t start_wait_ms = GetTimeInMillis();
        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, file->params.vsync_multiple);
        const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;

        if(!file->is_multi_frame) {
            auto sleep_time = file->params.wait_ms - time_already_spent;
            debug("Sleeping for %d", sleep_time);
            SleepMillis(sleep_time);
            break;
        }

        debug("Waiting for frame");
        SleepMillis(anim_delay_ms - time_already_spent);

    }
}