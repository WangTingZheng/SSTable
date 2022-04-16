#include "block_builder.h"
#include "block.h"

void test_block(){
    leveldb::BlockBuilder builder;

    builder.Add("key1", "val1");
    builder.Add("key2", "val2");
    builder.Add("key3", "val3");
    builder.Add("key4", "val4");

    leveldb::Block block = leveldb::Block(builder.Finish());

    while (block.ParseNextKey() && block.status().ok()){
        printf("key is [%s], value is [%s]\n", block.key().ToString().c_str(), block.value().ToString().c_str());
    }

    if(!block.status().ok()){
        printf("%s\n", block.status().ToString().c_str());
    }

    // 测试Vaild()，最后一次while循环会执行的ParseNextKey会使得p >= limit成立，current_被设置为了size_ + 1，此时已经失效了
    block.key();
}

int main(int argc, const char *argv[]) {
    test_block();
    return 0;
}

/*
 * 运行结果：
    assertion "Vaild()" failed: file "/cygdrive/b/Project/Clion/SSTable/src/block.cc", line 58, function: leveldb::Slice leveldb::Block::key()
    key is [key1], value is [val1]
    key is [key2], value is [val2]
    key is [key3], value is [val3]
    key is [key4], value is [val4]
 * 第一个报错是因为while循环的最后一次ParseNextKey把current_设置成了size_，是无效的一次读取，所以无法读取到key_了
 */
