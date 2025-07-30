/// CREDITS TO https://github.com/lasselukkari/MimeTypes

#pragma once
#include <string>
#include <string_view>
#include <array>

class MimeTypes {
public:
    static std::string getType(std::string_view path);
    static std::string getExtension(std::string_view type, size_t skip = 0);

private:
    struct Entry {
        std::string_view fileExtension;
        std::string_view mimeType;
    };
    
    static constexpr inline bool iequals(std::string_view a, std::string_view b);
    static std::array<Entry, 347> types;  // Definition moved to cpp file
};