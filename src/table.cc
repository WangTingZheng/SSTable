#include "table.h"

namespace leveldb{
    struct Table::Rep{

        Block *index_block;
        RandomAccessFile *file;
        Options options;
    };

    Status Table::Open(const Options &options, RandomAccessFile *file, uint64_t size, Table **table) {
        Slice footer_input;

        char footer_space[Footer::kEncodedLength];

        Status s = file->Read(size - Footer::kEncodedLength, Footer::kEncodedLength, &footer_input, footer_space);

        if(!s.ok()) return s;

        Footer footer;
        s = footer.DecodeFrom(&footer_input);
        if(!s.ok()) return s;

        BlockContents index_block_contents;
        ReadOptions opt;

        opt.verify_checksums = true;

        s = ReadBlock(file, opt, footer.index_handle(), &index_block_contents);

        if(s.ok()) {
            Block* index_block = new Block(index_block_contents);
            Rep *rep = new Table::Rep;
            rep->index_block = index_block;
            rep->file = file;
            rep->options = options;

            *table = new Table(rep);
        }

        return Status::OK();
    }

    Iterator *Table::BlockReader(void *arg, const ReadOptions &options, const Slice &index_value) {
        Table *table = reinterpret_cast<Table *>(arg);

        BlockHandle handle;
        Slice input = index_value;
        BlockContents contents;
        handle.DecodeFrom(&input);

        Status s = ReadBlock(table->rep_->file, options, handle, &contents);
        Block *block = new Block(contents);

        return block->NewIterator(table->rep_->options.comparator);
    }

    Status Table::InternalGet(const ReadOptions &options, const Slice &key,
                              void (*handle_result)(const Slice &, const Slice &)) {
        Status s;

        // 给index block建立迭代器
        Iterator *iterator = rep_->index_block->NewIterator(rep_->options.comparator);

        // 定位到key
        iterator->Seek(key);

        if(iterator->Valid()){
            // 去除datablock的handle
            Slice handle_value = iterator->value();
            // handle中有datablock的offset和size
            // 用blockreader根据datablock handle建立datablock的迭代器
            Iterator *block_iter = BlockReader(this, options, handle_value);
            // 定位到key
            block_iter->Seek(key);
            if(block_iter->Valid()){
                // 用handle_result函数处理kv对
                (*handle_result)(block_iter->key(), block_iter->value());
            }
            s = block_iter->status();
            delete block_iter;
        }

        if(s.ok()){
            s = iterator->status();
        }

        delete iterator;
        return s;
    }
}