#include <iostream>
#include <emmintrin.h>
#include <ctime>

void *memcpy_1(void *dst, void const *src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        *((char *) dst + i) = *((char *) src + i);
    }
    return dst;
}

void *memcpy_8(void *dst, const void *src, size_t n) {
    for (int i = 0; i < n; i++) {
        size_t tmp;
        __asm__ volatile("mov\t" "(%1), %0 \n"
                "mov\t" "%0,(%2)\n"
        :"=r"(tmp)
        :"r"((const char *) src + i), "r"((char *) dst + i)
        :"memory");
    }
    return dst;
}

void *memcpy_16unaligned(void *dst, const void *src, size_t n) {
    for (size_t i = 0; i < n; i += 16) {
        __m128i tmp;

        __asm__ volatile("movdqu\t (%1), %0\n"
                "movntdq\t %0, (%2)\n"
        :"=x"(tmp)
        :"r"((char *) src + i), "r"((char *) dst + i)
        :"memory");
    }
    return dst;
}

void *memcpy_16aligned(void *dst, const void *src, size_t n) {
    size_t offset = 0;
    while (((size_t) dst + offset) % 16 != 0) {
        *((char *) dst + offset) = *((char *) src + offset);
        offset++;
    }
    size_t tail = (n - offset) % 16;
    for (size_t i = n - tail; i < n; i++) {
        *((char *) dst + i) = *((char *) src + i);
    }
    for (size_t i = offset; i < n - tail; i += 16) {
        __m128i tmp;
        __asm__ volatile("movdqa\t (%1), %0\n"
                "movntdq\t %0, (%2)\n"
        :"=x"(tmp)
        :"r"((char *) src + i), "r"((char *) dst + i)
        :"memory");
    }
    return dst;
}

const int N = 1 << 30;

int main() {
    char *src = new char[N];
    char *dst = new char[N];
    std::clock_t start, time1 = 0, time2 = 0, time3 = 0, time4 = 0;
    start = std::clock();
//    memcpy_1(dst, src, N);
//    time1 = std::clock() - start;
//    start = std::clock();
//    memcpy_8(dst, src, N);
//    time2 = std::clock() - start;
//    start = std::clock();
//    memcpy_16aligned(dst, src, N);
//    time3 = std::clock() - start;
    memcpy_16aligned(dst + 3, src + 3, N - 10);
    time4 = std::clock() - start;
//    std::cout << "memcpy_1 average:" << time1 << std::endl;
//    std::cout << "memcpy_8 average:" << time2 << std::endl;
//    std::cout << "memcpy_16aligned average:" << time3 << std::endl;
    std::cout << "memcpy_16unaligned:" << time4 << std::endl;
    return 0;
}