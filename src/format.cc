#include "format.h"

namespace leveldb {

    void BlockHandle::EncodeTo(std::string *dst) const {
        // 若0是以uint64_t的方式存储的
        // 那么~static_cast<uint64_t>(0)就是uint64_t可以保存的最大值
        //  0 =  0000....0000
        // ~0 = ~0000....0000 = 1111....1111
        // 为什么要这么检查呢？
        assert(offset_ != ~static_cast<uint64_t>(0));
        assert(size_ != ~static_cast<uint64_t>(0));
        PutVarint64(dst, offset_);
        PutVarint64(dst, size_);
    }

    Status BlockHandle::DecodeFrom(Slice *input) {
        if (GetVarint64(input, &offset_) && GetVarint64(input, &size_)) {
            return Status::OK();
        } else {
            return Status::Corruption("bad block handle");
        }
    }

    // index_handle + padding
    // magic number
    void Footer::EncodeTo(std::string *dst) const {
        const size_t original_size = dst->size();

        index_handle_.EncodeTo(dst); // add index_handle to dst
        dst->resize(BlockHandle::kMaxEncodedLength); // padding

        // @todo 为什么不直接调用PutFixed64接口进行持久化？
        PutFixed32(dst, static_cast<uint32_t>(kTableMagicNumber & 0xffffffffu));
        PutFixed32(dst, static_cast<uint32_t>(kTableMagicNumber >> 32));
        //PutFixed64(dst, kTableMagicNumber);

        assert(original_size + kEncodedLength == dst->size());
        (void) original_size;
    }

    //index_handle + padding
    //magic number
    Status Footer::DecodeFrom(Slice *input) {
        const char *magic_ptr = input->data() + kEncodedLength - 8;

        const uint32_t magic_lo = DecodeFixed32(magic_ptr);
        const uint32_t magic_hi = DecodeFixed32(magic_ptr + 4);
        const uint64_t magic = ((static_cast<uint64_t>(magic_hi) << 32) |
                                (static_cast<uint64_t>(magic_lo)));
        //const uint64_t magic = DecodeFixed64(magic_ptr);

        if (magic != kTableMagicNumber) {
            return Status::Corruption("not an sstable (bad magic number)");
        }

        Status status = index_handle_.DecodeFrom(input);

        // @todo 为什么还要对input进行这样的处理呢？
        if (status.ok()) {
            const char *end = magic_ptr + 8;
            *input = Slice(end, input->data() + input->size() - end);
        }

        return status;
    }

    Status ReadBlock(RandomAccessFile *file, const ReadOptions &options,
                     const BlockHandle &handle, BlockContents *result) {
        result->data = Slice();
        result->cachable = false;
        result->heap_allocated = false;

        // 准备好保存block的空间
        size_t n = static_cast<size_t>(handle.size());
        char *buf = new char[n + kBlockTrailerSize];

        Slice contents;
        // 根据BlockHandle从文件中读取数据到buf
        // 如果底层用mmap，会把磁盘中的数据映射到content中
        Status s = file->Read(handle.offset(), n + kBlockTrailerSize, &contents, buf);

        if(!s.ok()){
            delete[] buf;
            return s;
        }

        if(contents.size() != n + kBlockTrailerSize){
            delete[] buf;
            return Status::Corruption("truncated block read");
        }

        // crc校验
        const char *data = contents.data();

        if(options.verify_checksums){
            // 读取出crc值，data后面就是type和crc值
            const uint32_t crc = crc32c::Unmask(DecodeFixed32(data + n + 1));
            // 计算data[0, n]的crc
            const uint32_t actual = crc32c::Value(data, n + 1);
            // 如果校验失败
            if(actual != crc){
                delete[] buf;
                s = Status::Corruption("block checksum mismatch");
                return s;
            }
        }

        // 解压缩
        switch (data[n]) {
            case kNoCompression:
                // 如果两者不相等
                // 说明读取时调用的是mmap接口
                if (data != buf){
                    delete[] buf;

                    // 直接从映射的内存中读取数据返回
                    result->data = Slice(data, n);

                    // 已经在内存中cache了，不需要LRUCache
                    result->cachable = false;
                    // Block对象释放的时候不允许释放内存中的Block数据
                    // 下回读取磁盘中同一块区域的数据还能从内存中读取
                    result->heap_allocated = false;
                }else{
                    // 返回临时变量中的data数据
                    result->data = Slice(buf, n);
                    // 因为buf是临时的，所以需要cache到LRUCache
                    result->cachable = true;

                    result->heap_allocated = true;
                }
                break;
            case kSnappyCompression:{
                size_t ulength = 0;
                if(!port::Snappy_GetUncompressedLength(contents.data(), contents.size(), &ulength)){
                    delete[] buf;
                    return Status::Corruption("corrupted compressed block contents");
                }
                char *ubuf = new char[ulength];
                if (!port::Snappy_Uncompress(data, n, ubuf)) {
                    delete[] buf;
                    delete[] ubuf;
                    return Status::Corruption("corrupted compressed block contents");
                }

                delete[] buf;
                result->data = Slice(ubuf, ulength);
                result->heap_allocated = true;
                result->cachable = true;
                break;
            }
            default:
                delete[] buf;
                return Status::Corruption("bad block type");
        }
        return Status::OK();
    }
}
