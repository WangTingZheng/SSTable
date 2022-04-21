#include "format.h"

namespace leveldb {
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
                if(!port::Snappy_Uncompress(data, n, buf)){
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
