#include <iostream>
#include<optional>
#include <utility>
#include <vector>
#include <Magick++.h>
#include <magick/image.h>

using namespace std;

class Post
{
    public:
        optional<string> image;
        optional<string> file_name;
        string thumbnail;
        string url;

        bool fetch_link();

        Post(string thumbnail, string url) {
            this->thumbnail = std::move(thumbnail);
            this->url = std::move(url);
        }
};

std::vector<Post> get_posts(int page);
std::optional<int> get_page_size();
bool download_image(const string& url_str, const string& out_file);