#pragma once

#include <iostream>
#include<optional>
#include <utility>
#include <vector>
#include <Magick++.h>
#include <magick/image.h>
#include "libxml/HTMLparser.h"

using namespace std;

bool download_image(const string& url_str, const string& out_file);
htmlDocPtr fetch_page(const string &url_str);