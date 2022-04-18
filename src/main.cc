#include "block_builder.h"
#include "block.h"

void test_block(){
    leveldb::Options options = leveldb::Options();
    leveldb::BlockBuilder builder = leveldb::BlockBuilder(&options);

    builder.Add("key1", "val1");
    builder.Add("key2", "val2");
    builder.Add("key3", "val3");
    builder.Add("key4", "val4");
    builder.Add("key5", "val5");
    builder.Add("key6", "val6");
    builder.Add("key7", "val7");
    builder.Add("key8", "val8");
    builder.Add("key9", "val9");

    builder.Add("key91", "val91");
    builder.Add("key92", "val92");
    builder.Add("key93", "val93");
    builder.Add("key94", "val94");
    builder.Add("key95", "val95");
    builder.Add("key96", "val96");
    builder.Add("key97", "val97");

    leveldb::Block block = leveldb::Block(builder.Finish());
    leveldb::Iterator *iterator = block.NewIterator(options.comparator);

    printf("[Seek to key96]: ");
    iterator->Seek("key96");
    assert(iterator->value() == leveldb::Slice("val96"));
    printf("Passed\n");

    printf("[Move to prev Entry]: ");
    iterator->Prev();
    assert(iterator->value() == leveldb::Slice("val95"));
    printf("Passed\n");

    printf("[Go to next and next Entry]: ");
    iterator->Next();
    iterator->Next();
    assert(iterator->value() == leveldb::Slice("val97"));
    printf("Passed\n");

    printf("[Seek to first]: ");
    iterator->SeekToFirst();
    assert(iterator->value() == leveldb::Slice("val1"));
    printf("Passed\n");

    printf("[Seek to last]: ");
    iterator->SeekToLast();
    assert(iterator->value() == leveldb::Slice("val97"));
    printf("Passed\n");

}

int main(int argc, const char *argv[]) {
    test_block();
    return 0;
}

/*
[Seek to key96]: Passed
[Move to prev Entry]: Passed
[Go to next and next Entry]: Passed
[Seek to first]: Passed
[Seek to last]: Passed
 */