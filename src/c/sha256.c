#include <stdio.h>
#include <string.h>
#include <stdlib.h> // 必须引入这个头文件来进行动态内存分配
#include <stdint.h> // 引入精确宽度的整数类型 (比如 uint64_t)

// 注意参数的变化：我们需要通过双重指针 **output_ptr 来修改函数外部的指针，
// 并且函数返回的是处理后的【总字节数】。
size_t preprocess(const char *data, unsigned char **output_ptr) {
    size_t len = strlen(data);
    
    // 强制转换为 64 位无符号整数，防止极大数据导致乘 8 时发生整型溢出
    uint64_t bit_len = (uint64_t)len * 8; 

    // ---------------------------------------------------------
    // 核心数学公式：计算到底需要多少个 64 字节（512比特）的车厢
    // ---------------------------------------------------------
    // 原始长度 len + 1字节(即0x80) + 8字节(即最后的64位长度标签)
    // 除以 64 然后加 1，就能极其精确地算出无论输入多长，都需要几个完整的块。
    size_t total_blocks = (len + 8) / 64 + 1;
    size_t total_bytes = total_blocks * 64;

    // ---------------------------------------------------------
    // 动态分配刚好足够的内存！
    // 技巧：使用 calloc 而不是 malloc。
    // calloc 在分配内存的同时，会把里面所有的比特自动清零 (填满 '0')！
    // 这意味着我们直接省去了烦人的 memset 疯狂补 0 的步骤，极其优雅！
    // ---------------------------------------------------------
    unsigned char *output = (unsigned char *)calloc(total_bytes, 1);
    if (output == NULL) {
        printf("致命错误：内存分配失败！\n");
        exit(1); // 真实工程中这里应该返回错误码
    }

    // 1. 拷贝原始数据到开头
    memcpy(output, data, len);

    // 2. 补上那神奇的分隔符 '1' (即 0x80)
    output[len] = 0x80;

    // 3. 疯狂补 '0' 这一步，已经被上面的 calloc 自动完成了！是不是很爽？

    // 4. 在最后 8 个字节的预留空间，用大端序 (Big-Endian) 写入原始比特长度
    size_t pos = total_bytes - 8;
    for (int i = 0; i < 8; i++) {
        // 右移操作，逐个字节提取 bit_len，并按大端序塞进最后 8 个字节里
        output[pos + 7 - i] = (unsigned char)(bit_len >> (i * 8));
    }

    // 把在堆区 (Heap) 开辟好的安全内存地址，交还给函数外面的指针
    *output_ptr = output;

    // 返回最终膨胀后的总长度，好让外面知道该读取多少
    return total_bytes;
}

int main() {
    // 这一次，我们故意用一个超长的字符串，长度远远超过 55 字节，来强制它开辟两个数据块！
    const char *data = "Hello, World! I am learning Bitcoin cryptography from the best professor. This string is definitely long enough to overflow a single 512-bit block!";
    
    // 声明一个空指针，等待接收动态分配的内存
    unsigned char *output = NULL; 
    
    // 传入指针的地址 (&output)
    size_t processed_len = preprocess(data, &output);

    printf("Original data length: %zu bytes\n", strlen(data));
    printf("Processed data total length: %zu bytes\n", processed_len);
    printf("Preprocessed data (Hex):\n");
    
    // 打印出来检查
    for (size_t i = 0; i < processed_len; i++) {
        printf("%02x ", output[i]);
        if ((i + 1) % 16 == 0) printf("\n"); // 每打印 16 个字节换行，像专业的十六进制编辑器一样
    }
    printf("\n");

    // =========================================================
    // ！！极其重要：C 语言程序员的自我修养！！
    // 凡是用 malloc 或 calloc 借来的内存，用完必须手动还给操作系统。
    // 否则就会造成内存泄漏 (Memory Leak)，导致服务器死机。
    // =========================================================
    free(output);
    output = NULL; // 释放后将指针置空，防止悬挂指针 (Dangling Pointer)

    return 0;
}