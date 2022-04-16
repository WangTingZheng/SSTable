#include <status.h>
#include "block.h"
#include "../util/coding.h"

namespace leveldb {
    static inline const char *DecodeEntry(const char *p, const char *limit,
                                          uint32_t *shared,
                                          uint32_t *non_shared,
                                          uint32_t *value_length) {
        if (limit - p < 2) return nullptr;

        *shared       = reinterpret_cast<const uint8_t *>(p)[0];
        // 添加非共享的key的长度
        *non_shared   = reinterpret_cast<const uint8_t *>(p)[1];
        *value_length = reinterpret_cast<const uint8_t *>(p)[2];

        // 如果三种长度都是只占一个比特
        if ((*shared | *non_shared | *value_length) < 128) {
            // Fast path: all three values are encoded in one byte each
            p += 3;
        } else {
            if ((p = GetVarint32Ptr(p, limit, shared)) == nullptr) return nullptr;
            if ((p = GetVarint32Ptr(p, limit, non_shared)) == nullptr) return nullptr;
            if ((p = GetVarint32Ptr(p, limit, value_length)) == nullptr) return nullptr;
        }

        // 如果剩下的空间都少于key和value的长度了，说明解析失败
        if (static_cast<uint32_t>(limit - p) < (*non_shared + *value_length)) {
            return nullptr;
        }
        return p;
    }

    leveldb::Block::Block(Slice contents)
            : data_(contents.data()), size_(contents.size()) {
        current_ = 0;
    }

    bool Block::Vaild() const{
        // 读取最后一个Entry之后，current_会等于size_，这个时候key_和value_读取到的值还是有效的
        // 如果current_ > size_，那就说明上一次ParseNextKey()时读取到了超越了Block末尾的区域
        return current_ <= size_;
    }

    Status Block::status(){
        return status_;
    }

    void Block::CorruptionError() {
        // 状态设置为无效
        current_ = size_ + 1;
        status_ = Status::Corruption("bad entry in block");
        key_.clear();
        value_.clear();
    }

    Slice Block::key() {
        // 如果ParseNextKey()读到了无效的空间，则不能返回key_
        assert(Vaild());
        return key_;
    }

    Slice Block::value() {
        // 如果ParseNextKey()读到了无效的空间，则不能返回key_
        assert(Vaild());
        return value_;
    }

    bool Block::ParseNextKey() {
        // 计算下一个Entry的开头位置
        const char *p = data_ + current_;
        // 计算Block的末尾位置
        const char *limit = data_ + size_;

        // 如果下一个Entry的开头位置不在Block以内，则肯定读取不到Entry
        if (p >= limit){
            // 没有Entry可读了，重置为Block末尾指针的下一个地址，此时Vaild()函数会指示失效
            current_ = size_ + 1;
            // 返回false通知调用者解析下一个Entry失败
            return false;
        }

        uint32_t shared, non_shared, value_length;

        // 从下一个Entry中解析出key的共享长度、key的非共享长度和value的长度，并将指针移动到非共享的key的位置
        p = DecodeEntry(p, limit, &shared, &non_shared, &value_length);

        // 如果key的指针为空或者上一个key还没到共享长度，那就拼接不起来完整的key
        if(p == nullptr || key_.size() < shared){
            // 如果读取一个Entry失败，就将status改成错误的提示
            CorruptionError();
            return false;
        }else{
            // 去掉上一个key中，非共享的部分
            key_.resize(shared);
            // 加上本次读取的本Entry的非共享的部分组成完整的key
            key_.append(p, non_shared);
            // 取出value
            value_ = Slice(p + non_shared, value_length);

            // 如果下一个还有Entry
            if(current_ <  size_)
                // p为key的头指针，non_shared + value_length为key-value的长度，current_
                // p + non_shared + value_length为value的末尾指针
                // p + non_shared + value_length - data_ 为下一个Entry的相对开头指针
                current_ = p + non_shared + value_length - data_;

            return true;
        }
    }
}
