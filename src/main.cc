#include "block_builder.h"
#include "block.h"

void test_block(){
    leveldb::BlockBuilder builder;

    builder.Add("key1", "val1");
    builder.Add("key2", "val2");
    builder.Add("key3", "val3");
    builder.Add("key4", "val4");

    leveldb::Block block = leveldb::Block(builder.Finish());

    while (block.ParseNextKey()){
        printf("key is [%s], value is [%s]\n", block.key().ToString().c_str(), block.value().ToString().c_str());
    }

}

int main(int argc, const char *argv[]) {
    test_block();
    return 0;
}

/*
    key is [key1], value is [val1]
    key is [key2], value is [val2]
    key is [key3], value is [val3]
    key is [key4], value is [val4]
 */