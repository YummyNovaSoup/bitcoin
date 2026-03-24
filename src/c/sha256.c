#include <stdio.h>
#include <string.h>
#include <stdlib.h> // 必须引入这个头文件来进行动态内存分配
#include <stdint.h> // 引入精确宽度的整数类型 (比如 uint64_t)

#define CRSHIFT(x, n) ((x >> n) | (x << (32-n))) // 右循环移位
#define SHR(x, n) (x >> n) // 右逻辑移位
#define sigma0(x) (CRSHIFT(x, 7) ^ CRSHIFT(x, 18) ^ SHR(x, 3))
#define sigma1(x) (CRSHIFT(x, 17) ^ CRSHIFT(x, 19) ^ SHR(x, 10))
#define Ch(x, y, z) ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define Sigma0(x) (CRSHIFT(x, 2) ^ CRSHIFT(x, 13) ^ CRSHIFT(x, 22))
#define Sigma1(x) (CRSHIFT(x, 6) ^ CRSHIFT(x, 11) ^ CRSHIFT(x, 25))
//intialize the hash values (first 32 bits of the fractional parts of the square roots of the first 8 primes 2..19):
uint32_t H[8] = {
    0x6a09e667, // h0
    0xbb67ae85, // h1
    0x3c6ef372, // h2
    0xa54ff53a, // h3
    0x510e527f, // h4
    0x9b05688c, // h5
    0x1f83d9ab, // h6
    0x5be0cd19  // h7
};
uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
}; 
uint32_t a, b, c, d, e, f, g, h;
uint32_t W[64];

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

void W_process(uint32_t *W, const unsigned char *block) {
    // 1. 前 16 个字 (0..15) 直接从数据块中提取（已修复整数提升漏洞）
    for (int i=0; i<16; i++) {
        W[i] = ((uint32_t)block[i*4] << 24) | 
               ((uint32_t)block[i*4 + 1] << 16) | 
               ((uint32_t)block[i*4 + 2] << 8)  | 
               ((uint32_t)block[i*4 + 3]);
    }
    // 2. 后 48 个字 (16..63) 根据前面的字进行扩展
    for (int i=16; i<64; i++) {
        W[i] = sigma1(W[i-2]) + W[i-7] + sigma0(W[i-15]) + W[i-16];
    }
}

void Compress(uint32_t *H, const unsigned char *block) {
    // 1. 准备消息调度数组 W
    W_process(W, block);

    // 2. 初始化工作变量 a..h
    a = H[0]; b = H[1]; c = H[2]; d = H[3];
    e = H[4]; f = H[5]; g = H[6]; h = H[7];

    // 3. 主循环，64轮压缩
    for (int i=0; i<64; i++) {
        uint32_t T1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];
        uint32_t T2 = Sigma0(a) + Maj(a, b, c);
        h = g; g = f; f = e; e = d + T1;
        d = c; c = b; b = a; a = T1 + T2;
    }

    // 4. 更新哈希值
    H[0] += a; H[1] += b; H[2] += c; H[3] += d;
    H[4] += e; H[5] += f; H[6] += g; H[7] += h;

    return ; // 这个函数目前没有实际返回值，未来可以改成 void
}

int main(int argc, char *argv[]) {
    const char *data = "Hello, World! I am learning Bitcoin cryptography from the best professor. This string is definitely long enough to overflow a single 512-bit block!";
    
    unsigned char *output = NULL; 
    size_t processed_len = preprocess(data, &output);

    printf("Original data length: %zu bytes\n", strlen(data));
    printf("Processed data total length: %zu bytes\n", processed_len);
    
    // ================= 关键修复在这里 =================
    // 循环喂入数据块，192 字节正好分 3 次喂入！
    size_t total_blocks = processed_len / 64;
    for (size_t i = 0; i < total_blocks; i++) {
        Compress(H, output + (i * 64)); 
    }
    // =================================================

    printf("Hash values after compression:\n");
    printf("0x");
    for (int i = 0; i < 8; i++) {
        printf("%08x", H[i]);
    }
    printf("\n");

    free(output);
    output = NULL; 

    return 0;
}