#include <shared/matrix/utils/canvas_image.h>
#include <shared/matrix/utils/AnimationPlayer.h>
#include <emulator.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>

#define CHECK(c) do { if (!(c)) { std::cerr << "Failed line " << __LINE__ << ": " << #c << '\n'; return 1; } } while (0)
static bool Pixel(rgb_matrix::FrameCanvas *canvas, int red, int green, int blue) {
    uint8_t r=0,g=0,b=0;
    return canvas->GetPixel(0,0,&r,&g,&b) && r==red && g==green && b==blue;
}
int main(int argc, char **argv) {
    Magick::InitializeMagick(argv[0]);
    rgb_matrix::RGBMatrix::Options options; options.rows=32; options.cols=32;
    rgb_matrix::EmulatorOptions emulator; emulator.headless=true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(options,emulator));
    CHECK(matrix);
    auto *canvas=matrix->CreateFrameCanvas();
    rgb_matrix::MemStreamIO stream; rgb_matrix::StreamWriter writer(&stream);
    canvas->Fill(255,0,0); CHECK(writer.Stream(*canvas,40000));
    canvas->Fill(0,255,0); CHECK(writer.Stream(*canvas,40000));
    canvas->Fill(0,0,255); CHECK(writer.Stream(*canvas,40000));
    AnimationPlayer player(&stream);
    CHECK(player.render(canvas,0) && Pixel(canvas,255,0,0));
    CHECK(player.render(canvas,39) && Pixel(canvas,255,0,0));
    CHECK(player.render(canvas,48) && Pixel(canvas,0,255,0));
    CHECK(player.render(canvas,80) && Pixel(canvas,0,0,255));
    CHECK(player.render(canvas,120) && Pixel(canvas,255,0,0));
    CHECK(player.render(canvas,1000000)); // Late render has bounded catch-up work.

    auto directory=std::filesystem::temp_directory_path()/
        ("led-matrix-media-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path,ec); } } cleanup{directory};
    auto source=directory/"source.png";
    Magick::Image(Magick::Geometry(64,64),Magick::Color("red")).write(source.string());
    int calls=0;
    auto callback=[&](Magick::Image *) { ++calls; };
    auto first=LoadImageAndScale(source,16,16,true,true,true,true,callback,"test-v1");
    CHECK(first && first->front().columns()==16 && calls==1);
    auto second=LoadImageAndScale(source,32,32,true,true,true,true,callback,"test-v1");
    CHECK(second && second->front().columns()==32 && calls==2);
    auto cached=LoadImageAndScale(source,32,32,true,true,true,true,callback,"test-v1");
    CHECK(cached && calls==2);
    for (const auto &entry: std::filesystem::directory_iterator(directory)) {
        if (entry.path()!=source) std::ofstream(entry.path()) << "corrupt cache";
    }
    auto recovered=LoadImageAndScale(source,32,32,true,true,true,true,callback,"test-v1");
    CHECK(recovered && recovered->front().columns()==32 && calls==3);
    auto changed=LoadImageAndScale(source,32,32,true,true,true,true,callback,"test-v2");
    CHECK(changed && calls==4);
    auto uncached=LoadImageAndScale(source,32,32,true,true,true,true,callback);
    CHECK(uncached && calls==5); // Unversioned callbacks may not be bypassed.
    CHECK(!LoadImageAndScale(source,0,32,true,true,true));
    canvas->Clear();
    Magick::Image overlay(Magick::Geometry(2,2),Magick::Color("red"));
    CHECK(SetImageTransparent(canvas,-1,-1,overlay));
    CHECK(Pixel(canvas,255,0,0));
    CHECK(!SetImageTransparent(canvas,0,0,Magick::Image{}));
    return 0;
}
