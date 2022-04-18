#include <status.h>
#include "block.h"
#include "../util/coding.h"


namespace leveldb {
    static inline const char *DecodeEntry(const char *p, const char *limit,
                                          uint32_t *shared,
                                          uint32_t *non_shared,
                                          uint32_t *value_length) {
        if (limit - p < 3) return nullptr;

        // 在小长度的情况下，避免跳入函数，减少栈空间创建
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

    // 读取Block最后的restart point length，获得保存的restart point个数
    // 获取restart point length的头地址
    inline uint32_t Block::NumRestarts() const{
        // data_ + size_ Block尾地址
        return DecodeFixed32(data_ + size_ - sizeof(uint32_t));
    }

    // ------- <- data_
    //
    // ------- <- restart offset
    //
    // ------- <- data_ + size_

    // 初始化Block的三个地址：data_、restart offset、size_
    // 错误处理，检查restart point length是否合法
    Block::Block(Slice contents) // restart point的数量
            : data_(contents.data()), size_(contents.size()) {

        // 防止size_ - sizeof(uint32_t)溢出
        // 同时防止NumRestarts()读取到Block前面的数据造成读取restart point出错
        if(size_ < sizeof(uint32_t)){
            size_ = 0;
        }else{
            // 由于size_是size_t类型的，是非负数，如果size_ < sizeof(uint32_t)，那么size_ - sizeof(uint32_t) < 0会溢出
            size_t max_restarts_allowed = (size_ - sizeof(uint32_t)) / sizeof(uint32_t);
            // 如果实际存储的restart point比最大的restart point还多的话，说明Block保存的restart point length不合法
            if(NumRestarts() > max_restarts_allowed){
                size_ = 0;
            }else{
                // restart point的头偏移量是总的偏移量减去restart point length和restart point所占的长度
                restarts_offset_ = size_ - (1 + NumRestarts()) * sizeof(uint32_t);
            }
        }
    }

    class Block::Iter : public Iterator{
        private:
                // 要进行二分查找，一定要对比两个key的大小
                const Comparator* comparator_;

                // Block的静态属性
                // 除了data_是地址，别的都是偏移量
                const char  *data_; // Block的头地址
                uint32_t const restarts_; // restart point的头偏移量
                uint32_t const num_restarts_; // restart point的个数，用于二分查找范围

                // Block的动态属性
                uint32_t restart_index_; //组磁头：指向Block中某一组磁头
                uint32_t current_; //Entry磁头：指向一组中某一Entry的磁头

                // 临时变量
                std::string key_; // 读取到的key，需要用resize操作舍弃非共享部分，以减少数据拷贝
                Slice value_; // 读取到的value

                // 操作之后的状态
                Status status_;

        // 封装二分查找要用的Compare
        inline int Compare(const Slice &a, const Slice &b) const{
            return comparator_->Compare(a, b);
        }

        // 获得组磁头的地址
        uint32_t GetRestartPoint(uint32_t index){
            assert(index < num_restarts_);
            return DecodeFixed32(data_ + restarts_ + index * sizeof(uint32_t));
        }

        // 获得下一个Entry的头偏移量
        inline uint32_t NextEntryOffset() const{
            return (value_.data() + value_.size()) - data_;
        }

        // 移动组磁头和Entry磁头指向index组的第一个Entry
        void SeekToRestartPoint(uint32_t index){
            key_.clear();
            restart_index_ = index;

            uint32_t offset = GetRestartPoint(index);

            // NextEntryOffset = data_ + offset + 0 - data_ = offset;
            // 为什么不通过更新current_的方式来重新定位呢？
            // current_ = offset
            // ParseNextKey()同一组时，value_的尾地址就是下一个Entry的开头地址
            // 利用这个特性，顺序ParseNextKey()可以很自然地进行下去
            // Seek的时候为了兼容它，也选择修改value_到offset
            value_ = Slice(data_ + offset, 0);
        }

        public:
            Iter(const Comparator * comparator,
                 const char *data,
                 uint32_t num_restarts,
                 uint32_t restarts)
                    // 二分查找用的Compare
                    : comparator_(comparator),

                    // Block的静态属性
                    data_(data),
                    restarts_(restarts),
                    num_restarts_(num_restarts),

                    // Block的动态属性
                    // 磁头最开始指向Entry区的尾偏移量
                    restart_index_(num_restarts_),
                    current_(restarts){

                assert(num_restarts > 0);
            }

        // 侦测到无效时，会使得磁头指向restarts_
        bool Valid() const override {
            return current_ < restarts_;
        }

        // 找到第一个key ≥ target的value
        // 找到最后一个key < target的组
        void Seek(const Slice &target) override {
            uint32_t left = 0;
            uint32_t right = num_restarts_ - 1;

            while (left < right){
                // 取中点，这里把mid可能会泄露
                // 故可以写成uint32_t mid = left + ((right - left) / 2);
                // 但其实没必要，因为uint32_t可以存0 - 2^32 - 1，也就是0 - 4,294,967,295，
                // 一个Block应该没有那么多组，所以要泄露还是比较难的
                // 好玩的是 / 2和 >> 1在汇编层面是一样的，可以通过https://gcc.godbolt.org/这个网站看到底层汇编代码

                uint32_t mid = (left + right + 1) / 2;

                uint32_t region_offset = GetRestartPoint(mid);

                uint32_t shared,  non_shared, value_length;

                const char *key_ptr = DecodeEntry(data_ + region_offset, data_ + restarts_,
                                                  &shared, &non_shared, &value_length);

                if (key_ptr == nullptr || shared != 0){
                    CorruptionError();
                    return;
                }

                // restart point指向的都是组的第一个Entry，保存有完整的key，shared = 0
                Slice mid_key(key_ptr, non_shared);


                if(Compare(mid_key, target) < 0 ){
                    // [left, mid - 1]的key都满足key < target，但是肯定不是第一个，所以舍弃
                    // 留下[mid, right]
                    left = mid;
                } else{
                    // [mid, right]的key都满足key ≥ target，所以舍弃
                    // 留下[left, mid - 1]
                    right = mid - 1;
                }
            }

            // 移动磁头对准找到的组的第一个Entry
            SeekToRestartPoint(left);

            while (true){
                // 一直顺序遍历
                if(!ParseNextKey()){
                    return;
                }

                // 我们要找的是第一个key ≥ target的Entry
                // 读到大于等于target的key就返回
                if(Compare(key_, target) >= 0){
                    return;
                }
            }
        }

        // 一直顺序遍历
        void Next() override {
            assert(Valid());
            ParseNextKey();
        }

        // 获得缓冲区保存的key
        Slice key() const override {
            assert(Valid());
            return Slice(key_);
        }

        // 获得缓冲区中保存的value
        Slice value() const override {
            assert(Valid());
            return Slice(value_);
        }

        // 返回状态信息，通常请情况下是ok的状态
        Status status() const override {
            return Status();
        }

        // 移动并读取整个Block中第一个Entry的位置
        void SeekToFirst() override {
            SeekToRestartPoint(0);
            ParseNextKey();
        }

        // 移动并读取整个Block中最后一个Entry的位置
        void SeekToLast() override {
            // 移动到最后一组的第一个Entry
            SeekToRestartPoint(num_restarts_ - 1);

            // 向右顺序遍历，找到遍历到的Entry的尾偏移量大于等于Entry区的尾偏移量
            while (ParseNextKey() && NextEntryOffset() < restarts_){

            }
        }

        // 向前遍历一个Entry
        // 因为向前遍历的Entry有可能会在其它的组中，所以需要先找到向前遍历的Entry所在的组
        // 前一个Entry的头偏移量是小于当前Entry的头偏移量的
        // 这个组的第一个Entry的头偏移量是小于原Entry的头偏移量的
        // 而且是从右往左数第一个小于当前Entry的头偏移量的
        void Prev() override {
            const uint32_t original = current_;
            // 当前组的第一个Entry的头偏移量没有小于当前Entry的头偏移量，而是大于等于，说明我们要找的Entry不在这个组
            while (GetRestartPoint(restart_index_) >= original){
                // 如果移到头了，还找不到，就不能再往前移了
                if(restart_index_ == 0){
                    // 重置磁头
                    restart_index_ = num_restarts_;
                    current_ = restarts_;
                    return; //说明找不到，前序遍历失败
                }

                // 向左移，寻找下一个可能的组
                restart_index_ --;
            }

            // 移动到上一个Entry所在组的第一个Entry
            SeekToRestartPoint(restart_index_);

            // 向右顺序遍历，如果遍历到的Entry的尾偏移量大于等于原Entry的头偏移量，说明已经过了，肯定找不到
            while (ParseNextKey() && NextEntryOffset() < original){

            }
        }

        private: void CorruptionError(){
            // 重置磁头
            restart_index_ = num_restarts_; //重置组磁头
            current_ = restarts_; // 重置Entry磁头

            status_ = Status::Corruption("bad entry in block");

            key_.clear();
            value_.clear();
        }

        bool ParseNextKey() {
          current_ = NextEntryOffset(); // 计算下一个Entry的开头位置
          const char *p = data_ + current_; // 计算restart point的开头地址
          const char *limit = data_ + restarts_; // 计算Block的末尾地址

          // 如果下一个Entry的开头位置不在Block以内，则肯定读取不到Entry
          if (p >= limit){
              restart_index_ = num_restarts_;
              // 没有Entry可读了，重置为Block末尾指针的偏移量，此时Vaild()函数会指示失效
              current_ = restarts_;
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

              // 顺序遍历可能会遍历到下一个组中，导致restart_index和current_不匹配
              while (restart_index_ + 1 < num_restarts_ // 不是最后一个组
                        && GetRestartPoint(restart_index_ + 1) < current_){ // current_移动到restart_index组右侧的组去了
                  ++restart_index_;
              }

              return true;
          }
        }
    };

    Iterator* Block::NewIterator(const Comparator *comparator){
        // 倘若size_ < sizeof(uint32_t)，则会导致data_ + size_ - sizeof(uint32_t) < data_
        // 调用NumRestarts()读取restart length肯定会出错
        if(size_ < sizeof(uint32_t)){
            return NewErrorIterator(Status::Corruption("bad block contents"));
        }

        // 获得Block中的restart point的数量
        const uint32_t num_restarts_ = NumRestarts();

        // 如果为0，说明Block为空
        if(num_restarts_ == 0){
            return NewEmptyIterator();
        }else { // 如果不为零，则说明Block正常，生成迭代器
            return new Iter(comparator, data_, num_restarts_, restarts_offset_);
        }
    }
}
