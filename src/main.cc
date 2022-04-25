#include <iostream>
#include <random>
#include "block_builder.h"
#include "block.h"
#include "snappy.h"
#include "table.h"
#include "string"

#define OS "Linux"
#define KV_NUM 160 * 160

std::string path;
std::vector<std::string> test_case;

leveldb::Options options;
leveldb::ReadOptions readOptions;

leveldb::Env *env;
leveldb::Status s;

leveldb::WritableFile *file = nullptr;
leveldb::RandomAccessFile *randomAccessFile = nullptr;

std::string key = "key";
std::string value = "value";

int get_quene[KV_NUM];

// 如果有报错，就显示出来
void check_status(const leveldb::Status &s) {
    if (!s.ok()) {
        std::cout << s.ToString() << std::endl;
        exit(0);
    }
}

// 配置一些参数
void init_config() {
    options = leveldb::Options();
    readOptions = leveldb::ReadOptions();
    readOptions.verify_checksums = true;
    env = leveldb::Env::Default();
}

// 根据系统选取地址
leveldb::Status init_path() {
    leveldb::Status s;
    if (OS == "WIN") {
        path = "B:/Project/Clion/SSTable/test_case.sst";
        s = leveldb::Status::OK();
    } else if (OS == "Linux") {
        path = "/home/wangtingzheng/source/test_case.sst";
        s = leveldb::Status::OK();
    } else {
        s = leveldb::Status::Corruption("Can not find OS", OS);
    }
    return s;
}

// 生成num对符合comparator的KV对
std::vector<std::string> generate(int num) {
    std::vector<std::string> dst;

    for (int i = 1; i <= num; i++) {
        std::string temp;
        for (int j = 0; j < i / 26; ++j) {
            temp.append("z");
        }

        if (i % 26 != 0) {
            temp += char(96 + i % 26);
        }
        dst.push_back(temp);
    }

    return dst;
}

// 打乱[0, KV_NUM - 1]
void init_get_quene(){
    for (int i = 0; i < KV_NUM; i++) {
        get_quene[i] = i;
    }

    std::shuffle(get_quene, get_quene + KV_NUM, std::mt19937(std::random_device()()));
}

// 生成KV_NUM对KV对
void init_test_case() {
    assert(KV_NUM > 0);
    test_case = generate(KV_NUM);
}

// 初始化
void init() {
    init_path();
    init_get_quene();
    init_config();
    init_test_case();
}

// 根据index取test_case中取数，追加到head后面
leveldb::Slice add_number_to_slice(std::string head, int index) {
    return leveldb::Slice(head.append(test_case[index]));
}

// 使用comparator对test_case进行检验，不然无法Add进Block
void test_test_case() {
    assert(!test_case.empty());

    for (int i = 0; i < KV_NUM - 1; i++) {
        assert(options.comparator->Compare(add_number_to_slice("key", i + 1), add_number_to_slice("key", i)) > 0);
    }
}

void test_block_write() {
    s = env->NewWritableFile(path, &file);
    check_status(s);

    check_status(s);

    leveldb::TableBuilder tableBuilder = leveldb::TableBuilder(options, file);

    // 把test_case的所有KV写入SSTable
    for (int i = 0; i < KV_NUM; ++i) {
        tableBuilder.Add(add_number_to_slice(key, i), add_number_to_slice(value, i));
    }

    // 构建SSTable
    s = tableBuilder.Finish();
    check_status(s);

    // 持久化到磁盘中，光调用Flush()不行
    // Flush()之后，数据可能会留在文件系统、SSD的内存里
    s = tableBuilder.Sync();
    check_status(s);
}

// 查询到KV对后的处理函数
void kv_handler(const leveldb::Slice &key, const leveldb::Slice &value) {
    // 去除前缀
    std::string key_number = key.ToString().replace(0, 3, "");
    std::string value_number = value.ToString().replace(0, 5, "");
    // 比较序号，对应的kv对的序号应该是相同的
    assert(key_number == value_number);
}

// 之所以分开两个函数，是因为读写同一个文件似乎不能写在一个函数中
void test_block_read() {
    s = env->NewRandomAccessFile(path, &randomAccessFile);
    check_status(s);

    leveldb::Table *table = nullptr;

    uint64_t size;
    env->GetFileSize(path, &size);
    // 读取SSTable数据到table
    s = leveldb::Table::Open(options, randomAccessFile, size, &table);
    check_status(s);

    for (int i = 0; i < KV_NUM; i++) {
        // 从随机序列中取得一个序号，取得字符串与key合并，查询这个key，使用kv_handler检查kv对是否对应
        table->InternalGet(readOptions, add_number_to_slice("key", get_quene[i]), kv_handler);
    }

    printf("All test passed\n");
}

int main(int argc, const char *argv[]) {
    init();
    test_test_case();
    test_block_write();
    test_block_read();

    return 0;
}