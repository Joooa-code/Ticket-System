//
// Created by joooa on 2026/6/10.
//

#ifndef TICKET_SYSTEM_MEMORY_RIVER_H
#define TICKET_SYSTEM_MEMORY_RIVER_H

#include <fstream>

using std::string;
using std::fstream;
using std::ifstream;
using std::ofstream;

template<class T, int info_len = 2>
class MemoryRiver {
private:
    /* your code here */
    fstream file;  // 文件流对象
    string file_name;  // 文件名
    int sizeofT = sizeof(T);  // 对象T的大小
public:
    MemoryRiver() = default;

    MemoryRiver(const string& file_name) : file_name(file_name) {}

    void initialise(string FN = "") {
        if (FN != "") file_name = FN;
        file.open(file_name, std::ios::out | fstream::binary);  // 创建文件并向文件中写入
        int tmp = 0;
        for (int i = 0; i < info_len; ++i)
            file.write(reinterpret_cast<char *>(&tmp), sizeof(int));
        file.close();
    }

    //读出第n个int的值赋给tmp，1_base
    void get_info(int &tmp, int n) {
        if (n > info_len) return;
        /* your code here */
        if (n < 1) return;
        file.open(file_name, fstream::in | fstream::binary);
        // 如果没有成功打开
        if (!file) {
            return;
        }

        // 将指针定位到第n个int的位置
        file.seekg((n - 1) * sizeof(int));

        // 读取
        file.read(reinterpret_cast<char *>(&tmp), sizeof(int));

        // 关闭
        file.close();
    }

    //将tmp写入第n个int的位置，1_base
    void write_info(int tmp, int n) {
        if (n > info_len) return;
        /* your code here */
        if (n < 1) return;
        file.open(file_name, fstream::in | fstream::out | fstream::binary);
        // 如果没有成功打开
        if (!file) {
            return;
        }

        // 将指针定位到第n个int的位置
        file.seekp((n - 1) * sizeof(int));

        // 写入
        file.write(reinterpret_cast<char *>(&tmp), sizeof(int));

        // 关闭
        file.close();
    }

    //在文件合适位置写入类对象t，并返回写入的位置索引index
    //位置索引意味着当输入正确的位置索引index，在以下三个函数中都能顺利的找到目标对象进行操作
    //位置索引index可以取为对象写入的起始位置
    int write(T &t) {
        /* your code here */
        file.open(file_name, fstream::in | fstream::out | fstream::binary);

        // 写指针定位到末尾
        file.seekp(0, fstream::end);
        int p = file.tellp();

        // 写入
        file.write(reinterpret_cast<char *>(&t), sizeof(T));

        // 关闭
        file.close();
        return p;
    }

    //用t的值更新位置索引index对应的对象，保证调用的index都是由write函数产生
    void update(T &t, const int index) {
        /* your code here */
        if (index < info_len * sizeof(int)) return;

        file.open(file_name, fstream::in | fstream::out | fstream::binary);

        // 定位写入指针
        file.seekp(index);

        // 写入
        file.write(reinterpret_cast<char *>(&t), sizeof(T));

        // 关闭
        file.close();
    }

    //读出位置索引index对应的T对象的值并赋值给t，保证调用的index都是由write函数产生
    void read(T &t, const int index) {
        /* your code here */
        if (index < info_len * sizeof(int)) return;
        file.open(file_name, fstream::in | fstream::binary);

        file.seekg(index);

        // 读入
        file.read(reinterpret_cast<char *>(&t), sizeof(T));

        // 关闭
        file.close();
    }

    ~MemoryRiver() {
        if (file.is_open()) {
            file.close();
        }
    }
};

#endif //TICKET_SYSTEM_MEMORY_RIVER_H