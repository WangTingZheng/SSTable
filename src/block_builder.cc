#include "block_builder.h"

namespace leveldb{
    void BlockBuilder::Add(const Slice &key, const Slice &value) {
        PutVarint32(&buffer_, key.size());
        PutVarint32(&buffer_, value.size());

        buffer_.append(key.data(), key.size());
        buffer_.append(value.data(), value.size());
    }

    Slice BlockBuilder::Finish() {
        return Slice(buffer_);
    }
}
