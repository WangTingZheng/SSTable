#ifndef CMAKE_PROJECT_TEMPLATE_BLOCK_BUILDER_H
#define CMAKE_PROJECT_TEMPLATE_BLOCK_BUILDER_H

#include <vector>
#include "slice.h"
#include "../util/coding.h"
#include "../include/options.h"
#include "../include/comparator.h"

namespace leveldb {
    struct Options;
    class BlockBuilder {
    public:
        explicit BlockBuilder(const Options *options);

        void Add(const Slice &key, const Slice &value);

        Slice Finish();

    private:
        const Options *options_;
        std::string buffer_;
        std::vector<uint32_t> restarts_;
        std::string last_key_;

        int counter_;
    };
}


#endif //CMAKE_PROJECT_TEMPLATE_BLOCK_BUILDER_H






