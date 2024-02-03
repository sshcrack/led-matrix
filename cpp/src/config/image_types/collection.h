#ifndef MAIN_COLLECTION_H
#define MAIN_COLLECTION_H

#include "general.h"
namespace ImageTypes {
    class Collection: public General {
    private:
        vector<Post> images;
        vector<Post> already_shown;

    public:
        optional<Post> get_next_image() override;
        void flush() override;

        explicit Collection(const json& arguments);
    };
}


#endif //MAIN_COLLECTION_H
