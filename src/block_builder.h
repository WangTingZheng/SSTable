//
// Created by 14037 on 2022/4/16.
//

#ifndef SSTABLE_BLOCK_BUILDER_H
#define SSTABLE_BLOCK_BUILDER_H

#include "../include/slice.h"
#include "../util/coding.h"

namespace leveldb{
    class BlockBuilder {
        public:
            void Add(const Slice &key, const Slice &value);
            Slice Finish();

        private:
            std::string buffer_;
    };

}


#endif //SSTABLE_BLOCK_BUILDER_H
