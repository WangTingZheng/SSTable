#ifndef SSTABLE_BLOCK_H
#define SSTABLE_BLOCK_H

#include <status.h>
#include "slice.h"

namespace leveldb{
    class Block{
    public:
        // 传入一个构建好的Block内容初始化Block对象
        // 禁止Block block = "contents";这种调用方法
        // 只能Block block = Block::Block("contents");
        explicit Block(Slice contents);

        // 从一个Block中解析出一对KV对到key_和value_
        bool ParseNextKey();

        // 读取
        Slice key();

        Slice value();

        Status status();

    private:
        const char * data_;
        size_t size_;

        uint32_t current_;

        std::string key_;
        Slice value_;

        Status status_;

        bool Vaild() const;

        void CorruptionError();
    };
}

#endif //SSTABLE_BLOCK_H
