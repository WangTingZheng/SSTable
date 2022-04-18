#ifndef SSTABLE_BLOCK_H
#define SSTABLE_BLOCK_H

#include <status.h>
#include "slice.h"
#include "../include/comparator.h"
#include "../include/iterator.h"

namespace leveldb{
    class Block{
    public:
        // 传入一个构建好的Block内容初始化Block对象
        // 禁止Block block = "contents";这种调用方法
        // 只能Block block = Block::Block("contents");
        explicit Block(Slice contents);
        Iterator *NewIterator(const Comparator*comparator);
    private:
        class Iter;

        const char * data_;
        size_t size_; //size_要参与和sizeof()的计算，同时它并不会为了持久化被编码，所以声明为size_t，其它的变量都是uint32_t
        uint32_t restarts_offset_;

        uint32_t NumRestarts() const;
    };
}
#endif //SSTABLE_BLOCK_H
