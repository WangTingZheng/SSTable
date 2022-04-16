#ifndef SSTABLE_BLOCK_H
#define SSTABLE_BLOCK_H

#include "slice.h"

namespace leveldb{
    class Block{
        public:
            explicit Block(Slice contents);

            bool ParseNextKey();

            Slice key();

            Slice value();

        private:
            const char * data_;
            size_t size_;

            uint32_t current_;

            Slice key_;
            Slice value_;
    };
}

#endif //SSTABLE_BLOCK_H
