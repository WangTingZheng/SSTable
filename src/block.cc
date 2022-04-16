#include "block.h"
#include "../util/coding.h"

namespace leveldb {
    static inline const char *DecodeEntry(const char *p, const char *limit,
                                          uint32_t *key_length,
                                          uint32_t *value_length) {
        if (limit - p < 2) return nullptr;

        *key_length = reinterpret_cast<const uint8_t *>(p)[0];
        *value_length = reinterpret_cast<const uint8_t *>(p)[1];

        if ((*key_length | *value_length) < 128) {
            // Fast path: all three values are encoded in one byte each
            p += 2;
        } else {
            if ((p = GetVarint32Ptr(p, limit, key_length)) == nullptr) return nullptr;
            if ((p = GetVarint32Ptr(p, limit, value_length)) == nullptr) return nullptr;
        }

        if (static_cast<uint32_t>(limit - p) < (*key_length + *value_length)) {
            return nullptr;
        }
        return p;
    }

    leveldb::Block::Block(Slice contents)
            : data_(contents.data()), size_(contents.size()) {
        current_ = 0;
    }

    Slice Block::key() {
        return key_;
    }

    Slice Block::value() {
        return value_;
    }

    bool Block::ParseNextKey() {
        const char *p = data_ + current_;
        const char *limit = data_ + size_;

        if (current_ >= size_){
            return false;
        }

        uint32_t key_length, value_length;

        p = DecodeEntry(p, limit, &key_length, &value_length);

        key_   = Slice(p, key_length);
        value_ = Slice(p + key_length, value_length);

        if(current_ <  size_)
            current_ = p + key_length + value_length - data_;

        return true;
    }
}
