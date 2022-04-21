#include <iostream>
#include "block_builder.h"
#include "block.h"
#include "snappy.h"

leveldb::BlockHandle blockHandle;
std::string linux_path = "./test.sst";
std::string windows_path = "B:/Project/Clion/SSTable/test.sst";


void check_status(leveldb::Status s){
    if(!s.ok()){
        std::cout << s.ToString() << std::endl;
        exit(0);
    }
}

void test_block_write(){
    leveldb::Status s;
    leveldb::Options options = leveldb::Options();
    std::string test_fname = std::string(linux_path);
    leveldb::Env *env = leveldb::Env::Default();
    leveldb::WritableFile *file = nullptr;

    s = env->NewWritableFile(test_fname, &file);
    check_status(s);

    leveldb::TableBuilder tableBuilder = leveldb::TableBuilder(options, file);

    tableBuilder.Add("key1", "val1");
    tableBuilder.Add("key2", "val2");
    tableBuilder.Add("key3", "val3");
    tableBuilder.Add("key4", "val4");
    tableBuilder.Add("key5", "val5");
    tableBuilder.Add("key6", "val6");
    tableBuilder.Add("key7", "val7");
    tableBuilder.Add("key8", "val8");
    tableBuilder.Add("key9", "val9");

    tableBuilder.Add("key91", "val91");
    tableBuilder.Add("key92", "val92");
    tableBuilder.Add("key93", "val93");
    tableBuilder.Add("key94", "val94");
    tableBuilder.Add("key95", "val95");
    tableBuilder.Add("key96", "val96");
    tableBuilder.Add("key97", "val97");

    tableBuilder.Flush();

    blockHandle = tableBuilder.ReturnBlockHandle();
}

// 之所以分开两个函数，是因为读写同一个文件似乎不能写在一个函数中
void test_block_read(){
    leveldb::Status s;
    std::string test_fname = std::string(linux_path);
    leveldb::Env *env = leveldb::Env::Default();

    leveldb::RandomAccessFile *randomAccessFile = nullptr;
    s = env->NewRandomAccessFile(test_fname, &randomAccessFile);
    check_status(s);

    leveldb::Options options = leveldb::Options();
    leveldb::ReadOptions readOptions = leveldb::ReadOptions();
    readOptions.verify_checksums = true;

    leveldb::BlockContents blockContents;
    s = leveldb::ReadBlock(randomAccessFile, readOptions, blockHandle, &blockContents);
    check_status(s);

    leveldb::Block block = leveldb::Block(blockContents);
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
    test_block_write();
    test_block_read();
    return 0;
}



/*
[Seek to key96]: Passed
[Move to prev Entry]: Passed
[Go to next and next Entry]: Passed
[Seek to first]: Passed
[Seek to last]: Passed
 */