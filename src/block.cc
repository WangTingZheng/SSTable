#include <status.h>
#include "block.h"
#include "../util/coding.h"


namespace leveldb {
    inline uint32_t Block::NumRestarts() const{
        // 从最后一个区域读取restart point的个数
        return 0;
    }
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

    Block::Block(Slice contents) // restart point的数量
            : data_(contents.data()), size_(contents.size()) {
        /*
         * data_
         * size_
         * restart point offset
         */
    }

    class Block::Iter : public Iterator{
        private:
                const Comparator* comparator_;

                // DataBlock的属性
                const char const *data_; // DataBlock 数据
                uint32_t const restarts_; // restart point的头指针
                uint32_t const num_restarts_; // restart point的个数，用于二分查找范围

                // 磁头
                uint32_t current_;
                uint32_t restart_index_;

                // key-value
                std::string key_;
                Slice value_;

                // 读取之后的状态
                Status status_;


        public:
            Iter(const Comparator * comparator, const char *data,
                    uint32_t restarts,  uint32_t num_restarts)
                    : comparator_(comparator), data_(data), restarts_(restarts), num_restarts_(num_restarts) {
                assert(num_restarts >0);
            }

        bool Valid() const override {
            return false;
        }

        void Seek(const Slice &target) override {

        }

        void Next() override {

        }

        Slice key() const override {
            return Slice();
        }

        Slice value() const override {
            return Slice();
        }

        Status status() const override {
            return Status();
        }

        void SeekToFirst() override {

        }

        void SeekToLast() override {

        }

        void Prev() override {

        }

        private: void CorruptionError(){

        }

        bool ParseNextKey() {
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
    };

    Iterator* Block::NewIterator(const Comparator *comparator){
        const uint32_t num_restarts_ = NumRestarts();
        return new Iter(comparator, data_, restarts_offset_, num_restarts_);
    }
}
