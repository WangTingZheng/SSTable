#ifndef SSTABLE_FORMAT_H
#define SSTABLE_FORMAT_H

#include <cstdint>
#include <string>
#include "../include/status.h"
#include "../include/env.h"
#include "table_builder.h"

namespace leveldb {
    class Block;
    class RandomAccessFile;
    struct ReadOptions;

    class BlockHandle {
        public:
            BlockHandle();

            uint64_t offset() const { return offset_; }

            void set_offset(uint64_t offset) { offset_ = offset; }

            uint64_t size() const { return size_t; }

            void set_size(uint64_t size) { size_t = size; }

        private:
            uint64_t offset_;
            uint64_t size_t;
    };

    struct BlockContents{
        Slice data;
        bool cachable;
        bool heap_allocated;
    };


    static const size_t kBlockTrailerSize = 5;

    Status
    ReadBlock(RandomAccessFile *file, const ReadOptions &options, const BlockHandle &handle, BlockContents *result);

    inline BlockHandle::BlockHandle()
            : offset_(~static_cast<uint64_t>(0)), size_t(~static_cast<uint64_t>(0)) {}
}
#endif //SSTABLE_FORMAT_H
