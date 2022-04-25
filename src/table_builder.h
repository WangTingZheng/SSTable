#ifndef SSTABLE_TABLE_BUILDER_H
#define SSTABLE_TABLE_BUILDER_H

#include "../include/slice.h"
#include "../include/options.h"
#include "../include/env.h"

#include "../util/crc32c.h"
#include "../port/port_stdcxx.h"

#include "block_builder.h"
#include "format.h"

namespace leveldb {
    class BlockBuilder;
    class BlockHandle;
    class WritableFile;

    class TableBuilder {
    public:
        TableBuilder(const Options &options, WritableFile *file);

        void Add(const Slice &key, const Slice &value);

        void Flush();

        Status status() const;

        Status Finish();

        BlockHandle ReturnBlockHandle();

        uint64_t FileSize() const;

        Status Sync();

    private:
        void WriteBlock(BlockBuilder *block, BlockHandle *handle);

        void WriteRawBlock(const Slice &block_contents, CompressionType type, BlockHandle *handle);

        bool ok() const { return status().ok(); }

        struct Rep;
        Rep *rep_;
    };
}

#endif //SSTABLE_TABLE_BUILDER_H
