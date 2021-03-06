#include "table_builder.h"

namespace leveldb {
    // 这里之所以要特意用一个结构体来存储变量而不直接在类中定义变量
    // 是因为table_builder.cc是供用户使用的
    // leveldb不希望底层的参数被用户访问或者修改
    // 此外也是为了把参数和接口解耦，便于升级
    // https://stackoverflow.com/questions/33427916/why-table-and-tablebuilder-in-leveldb-use-struct-rep
    struct TableBuilder::Rep {
        Rep(const Options &opt, WritableFile *f)
                : options(opt),
                  index_block_options(opt),
                  index_block(&index_block_options),
                  data_block(&opt),
                  file(f),
                  offset(0),
                  pending_index_entry(false)// 刚刚开始时，不向index block写入数据
                  {

        }

        Options options;
        Options index_block_options;

        BlockBuilder data_block;
        BlockBuilder index_block;

        BlockHandle pending_handle;
        WritableFile *file;
        bool pending_index_entry;
        Status status;

        std::string compressed_output;
        uint64_t offset;

        std::string last_key;
    };

    TableBuilder::TableBuilder(const Options &options, WritableFile *file)
        :rep_(new Rep(options, file)){

    }

    void TableBuilder::Add(const Slice &key, const Slice &value) {
        Rep *r = rep_;

        // 如果之前持久化了一个datablock，则准备向index block插入一条指向它的kv对
        if (r->pending_index_entry) {
            // 找一个介于last_key和key之间的，最短的key
            // 比如说zzzzb + zzc = zzzd
            r->options.comparator->FindShortestSeparator(&r->last_key, key);

            // 把datablock的handle编码为字符串
            std::string handle_encoding;
            r->pending_handle.EncodeTo(&handle_encoding);

            // 写入index block
            r->index_block.Add(r->last_key, Slice(handle_encoding));
            r->pending_index_entry = false;
        }

        // 更新last_key，由于不用前缀压缩，所以直接把key复制进来
        r->last_key.assign(key.data(), key.size());
        // 写入datablock
        r->data_block.Add(key, value);

        // 估计datablock的大小
        const size_t estimated_block_size = r->data_block.CurrentSizeEstimate();
        //如果 Block的大小达到了阈值，则此Block以满
        if (estimated_block_size >= r->options.block_size) {
            // 合并Entry和restart point，并持久化到磁盘
            Flush();
        }
    }

    void TableBuilder::Flush() {
        Rep *r = rep_;

        // 持久化到磁盘，并生成BlockHandle到pending_handle
        // 先用snappy压缩，后进行crc编码，最终持久化到磁盘
        WriteBlock(&r->data_block, &r->pending_handle);

        if(ok()){
            r->pending_index_entry = true;
            // 调用文件系统的刷新接口，实际上并没有真正地持久化到磁盘，还是有可能存储在文件系统的buffer pool
            // 甚至是FTL的cache里
            r->status = r->file->Flush();
        }
    }

    // 持久化一个Block
    // 本函数的工作是对block中的数据进行压缩（如果需要的话）
    // 压缩之后调用WriteRawBlock真正进行持久化
    void TableBuilder::WriteBlock(BlockBuilder *block, BlockHandle *handle) {
        Rep *r = rep_;
        // 将Block的各个部分合并
        Slice raw = block->Finish();
        // 获取压缩类型，默认是采用snappy压缩
        CompressionType type = r->options.compression;
        Slice block_contents;

        switch (type) {
            // 如果不压缩，则直接持久化原数据
            case kNoCompression:
                block_contents = raw;
                break;
            // 如果采用snappy压缩，则要先检查压缩率是否达标
            case kSnappyCompression:{
                std::string *compressed = &r->compressed_output;

                // 当CMakeLists中的HAVE_SNAPPY没有被开启，Snappy_Compress就不运行实际的压缩程序
                // 压缩就会失败，Snappy_Compress()就会返回false
                // 如果压缩成功，且压缩率大于1/8，说明压缩了之后不会增加解析时间
                if(port::Snappy_Compress(raw.data(), raw.size(), compressed) &&
                   compressed->size() < raw.size() - (raw.size() / 8u)){
                    // 把压缩后的数据持久化
                    block_contents = *compressed;
                } else{ // 如果未启用Snappy，或者压缩率不够，则还是只持久化原数据
                    block_contents = raw;
                    // 压缩类型改为不压缩，这样读取的时候就不会解压缩
                    type = kNoCompression;
                }
                break;
            }
        }

        // 将处理好的数据block_contents和压缩类型type持久化到磁盘
        // 并且赋值偏移量和长度到handle
        WriteRawBlock(block_contents, type, handle);

        // 清除保存压缩数据的变量
        r->compressed_output.clear();
        // 重置block builer的缓存数据
        block->Reset();
    }

    // 真正持久化经过压缩处理的block数据
    // 持久化前进行crc编码，方便校验
    void TableBuilder::WriteRawBlock(const Slice &block_contents, CompressionType type, BlockHandle *handle) {
        Rep *r = rep_;

        // 更新handle，赋值block在文件中的头偏移量和长度
        // 当前文件的尾偏移量就是要持久化block的开头偏移量
        handle->set_offset(r->offset);
        // size不包括type和crc值
        // 这样也可以读取到，也type和crc的长度都是固定的
        handle->set_size(block_contents.size());

        // 向文件末尾追加这个block数据
        r->status = r->file->Append(block_contents);

        // 如果写入成功，就进行crc编码
        if(r->status.ok()){
            // kBlockTrailerSize = type + crc值
            // type(8bit, 1Byte) + crc(uint32_t, 32bit, 4Byte) = 5Byte
            char trailer[kBlockTrailerSize];
            // 赋值入压缩的类型
            trailer[0] = type;

            // 计算block数据的crc值
            // 底层调用了下面的crc32c::Extend
            // Extend(0, data, n)
            // 计算data[0, n - 1]的crc值
            uint32_t crc = crc32c::Value(block_contents.data(), block_contents.size());

            // 计算trailer[0, 0]的crc值
            // trailer[0, 0]就是type
            // 使用block的crc值作为种子
            crc = crc32c::Extend(crc, trailer, 1);

            // 计算crc的掩码，写入到trailer的后4个Byte
            // 注释说只计算crc值会有问题
            // 这里有更详细的讨论: https://stackoverflow.com/questions/61639618/why-leveldb-and-rocksdb-need-a-masked-crc32
            EncodeFixed32(trailer + 1, crc32c::Mask(crc));

            // 将type和crc校验码写入文件
            r->status = r->file->Append(Slice(trailer, kBlockTrailerSize));

            if (r->status.ok()) {
                // 更新文件偏移量
                r->offset += block_contents.size() + kBlockTrailerSize;
            }
        }
    }

    Status TableBuilder::Finish() {
        Rep *r = rep_;
        Flush();
        BlockHandle index_block_handle;

        if (ok()) {
            // 还有没达到阈值的datablock，需要额外封装成一个datablock
            if (r->pending_index_entry) {
                // 知道一个比last_key大的短key
                // 因为最后一个datablock已经没有下一个datablock了
                // zzzzb -> zzzzc
                r->options.comparator->FindShortSuccessor(&r->last_key);

                // 编码成字符串
                std::string handle_encoding;
                r->pending_handle.EncodeTo(&handle_encoding);

                //写入index block
                r->index_block.Add(r->last_key, Slice(handle_encoding));
                r->pending_index_entry = false;
            }
            // 所有datablock的handler都已经被写入到了index block了，持久化index block
            // 获得index block的index block handle
            WriteBlock(&r->index_block, &index_block_handle);
        }

        if (ok()) {
            Footer footer;
            // 将index_block_handle写入到footer
            footer.set_index_handle(index_block_handle);

            // 给footer加入padding和magic number
            // 把它编码为字符串
            std::string footer_encoding;
            footer.EncodeTo(&footer_encoding);

            // 写入footer数据
            r->status = r->file->Append(Slice(footer_encoding));

            // 更新偏移量
            if (r->status.ok()) {
                r->offset += footer_encoding.size();
            }
        }

        return r->status;
    }

    // 把内存的数据都写入到磁盘
    Status TableBuilder::Sync() {
        return rep_->file->Sync();
    }

    Status TableBuilder::status() const {
        return rep_->status;
    }

    // 临时加的，方便测试用，实际是不可能把rep_的内部数据返回的
    // 完整的sstable中，Block的BlockHandle是写入index block的
    BlockHandle TableBuilder::ReturnBlockHandle() {
        return rep_->pending_handle;
    }

    uint64_t TableBuilder::FileSize() const {
        return rep_->offset;
    }
}
