#include <iostream>
#include<optional>
#include <vector>

using namespace std;

class Post
{
    public:
        optional<string> image;
        string thumbnail;
        string url;

        bool fetch_link();

        Post(string thumbnail, string url) {
            this->thumbnail = thumbnail;
            this->url = url;
        }
};

std::vector<Post> get_posts(int page);
std::optional<int> get_page_size();