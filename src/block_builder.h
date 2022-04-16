#ifndef CMAKE_PROJECT_TEMPLATE_BLOCK_BUILDER_H
#define CMAKE_PROJECT_TEMPLATE_BLOCK_BUILDER_H

#include <vector>
#include "slice.h"
#include "../util/coding.h"


namespace leveldb {
    class BlockBuilder {
    public:
        explicit BlockBuilder();

        void Add(const Slice &key, const Slice &value);

        Slice Finish();

    private:
        std::string buffer_;
        std::string last_key_;

        int counter_;
    };
}


#endif //CMAKE_PROJECT_TEMPLATE_BLOCK_BUILDER_H






