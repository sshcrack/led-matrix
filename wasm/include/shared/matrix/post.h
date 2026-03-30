#pragma once
// WASM stub: shared/matrix/post.h without Magick++ image-processing.
// The Post class is referenced by image_providers/general.h but is never
// instantiated during WASM scene rendering.

#include <string>
#include <optional>
#include <vector>

using std::string;
using std::vector;
using std::optional;

/// Minimal Post stub for WASM builds. Image loading is not available.
class Post {
public:
    explicit Post(const string &img_url, bool = true) : img_url_(img_url) {}

    string get_filename() { return ""; }
    string get_image_url() { return img_url_; }

    // process_images not available in WASM builds

private:
    string img_url_;
};
