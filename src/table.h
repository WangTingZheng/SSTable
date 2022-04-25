#ifndef SSTABLE_TABLE_H
#define SSTABLE_TABLE_H

#include "status.h"
#include "iterator.h"
#include "options.h"
#include "env.h"
#include "format.h"
#include "block.h"

namespace leveldb {
    struct Options;

    class RandomAccessFile;

    class Table {
    public:
        static Status Open(const Options &options, RandomAccessFile *file,
                           uint64_t size, Table **table);

        Table(const Table &) = delete;

        Status InternalGet(const ReadOptions &, const Slice &key,
                           void (*handle_result)(const Slice &k, const Slice &v));

    private:
        struct Rep;

        Iterator *BlockReader(void *arg, const ReadOptions &options,
                              const Slice &index_value);

        explicit Table(Rep *rep) : rep_(rep) {};

        Rep *const rep_;
    };
}

#endif //SSTABLE_TABLE_H
