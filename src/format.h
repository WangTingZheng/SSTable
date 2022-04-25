#ifndef SSTABLE_FORMAT_H
#define SSTABLE_FORMAT_H

#include <cstdint>
#include <string>
#include "../include/status.h"
#include "../include/env.h"
#include "table_builder.h"

namespace leveldb {
    class BlockHandle {
    public:
        void set_offset(uint64_t offset) { offset_ = offset; }

        uint64_t offset() const { return offset_; }

        void set_size(uint64_t size) { size_ = size; }

        uint64_t size() const { return size_; }

        void EncodeTo(std::string *dst) const;

        Status DecodeFrom(Slice *input);

        // offset_和size_都是uint64_t，uint64_t为64比特，8Byte
        // 两个参数，总共8 * 2Byte
        // 一个enum最大4Byte
        enum {
            kMaxEncodedLength = 8 * 2 + 4
        };
    private:
        // file的read的offset是uint64_t的
        // 从逻辑上来说，偏移量和大小是以Byte算的
        // uint32_t最大可以保存2^32 Byte的磁盘空间
        // 2^32 Byte = 2^22 KByte = 2^12 MByte = 2^2 GByte = 4GByte
        // 连一般的SSD都256GByte起步，4GByte根本不够
        // 而用64位的uint64_t可以保存2^64 Byte = 2^34 GByte，理论上肯定够了
        uint64_t offset_;
        uint64_t size_;
    };

    class Footer {
    public:
        // 一个index block handle加上padding是BlockHandle::kMaxEncodedLength
        // magic number为uint64_t，8Byte
        enum {
            kEncodedLength = BlockHandle::kMaxEncodedLength + 8
        };

        Footer() = default;

        void set_index_handle(const BlockHandle &index_handle) { index_handle_ = index_handle; }

        BlockHandle index_handle() const { return index_handle_; }

        void EncodeTo(std::string *dst) const;

        Status DecodeFrom(Slice *input);

    private:
        BlockHandle index_handle_;
    };

    // kTableMagicNumber was picked by running
    //    echo http://code.google.com/p/leveldb/ | sha1sum
    // and taking the leading 64 bits.
    static const uint64_t kTableMagicNumber = 0xdb4775248b80fb57ull;

    // 1Byte的type加上4Byte的CRC校验值
    static const size_t kBlockTrailerSize = 5;

    struct BlockContents {
        Slice data;
        bool cachable;
        bool heap_allocated;
    };

    Status ReadBlock(RandomAccessFile *file, const ReadOptions &options,
                     const BlockHandle &handle, BlockContents *result);
}
























/*
namespace leveldb {
    class Block;
    class RandomAccessFile;
    struct ReadOptions;

    class BlockHandle {
        public:
            // offset_和size_各8个Byte
            // enum是4Byte
            enum {kMaxEncodedLength = 10 + 10};
            BlockHandle();

            uint64_t offset() const { return offset_; }

            void set_offset(uint64_t offset) { offset_ = offset; }

            uint64_t size() const { return size_; }

            void set_size(uint64_t size) { size_ = size; }
            void EncodeTo(std::string* dst) const;
            Status BlockHandle::DecodeFrom(Slice *input);

    private:
            uint64_t offset_;
            uint64_t size_;
    };

    class Footer{
        public:
            // Footer至少是一个index_handle加上8Byte的magic number
            enum{kEncodedLength = BlockHandle::kMaxEncodedLength + 8};

            Footer() = default;

            const BlockHandle& index_handle() const{return index_handle_;}

            void set_handle_handle(const BlockHandle& h){index_handle_ = h;}

            void EncodingTo(std::string *dst);
            Status DecodingTo(Slice *input);
        private:
            BlockHandle index_handle_;
    };

// kTableMagicNumber was picked by running
//    echo http://code.google.com/p/leveldb/ | sha1sum
// and taking the leading 64 bits.
    static const uint64_t kTableMagicNumber = 0xdb4775248b80fb57ull;

    struct BlockContents{
        Slice data;
        bool cachable;
        bool heap_allocated;
    };


    static const size_t kBlockTrailerSize = 5;

    Status
    ReadBlock(RandomAccessFile *file, const ReadOptions &options, const BlockHandle &handle, BlockContents *result);

    inline BlockHandle::BlockHandle()
            : offset_(~static_cast<uint64_t>(0)), size_(~static_cast<uint64_t>(0)) {}
}
 */
#endif //SSTABLE_FORMAT_H
