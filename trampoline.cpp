#include <unistd.h>
#include <sys/mman.h>
#include <cstdio>
#include <algorithm>
#include <iostream>
#include <cassert>


template<typename ... Args>
struct args;

template<>
struct args<> {
    static const int INT = 0;
    static const int SSE = 0;
};

template<typename T, typename ... Args>
struct args<T, Args ...> {
    static const int INT = args<Args ...>::INT + 1;
    static const int SSE = args<Args ...>::SSE;
};

template<typename T, typename ... Args>
struct args<T *, Args ...> {
    static const int INT = args<Args ...>::INT + 1;
    static const int SSE = args<Args ...>::SSE;
};

template<typename ... Args>
struct args<char, Args ...> {
    static const int INT = args<Args ...>::INT + 1;
    static const int SSE = args<Args ...>::SSE;
};

template<typename ... Args>
struct args<short, Args ...> {
    static const int INT = args<Args ...>::INT + 1;
    static const int SSE = args<Args ...>::SSE;
};
template<typename ... Args>
struct args<int, Args ...> {
    static const int INT = args<Args ...>::INT + 1;
    static const int SSE = args<Args ...>::SSE;
};

template<typename ... Args>
struct args<long, Args ...> {
    static const int INT = args<Args ...>::INT + 1;
    static const int SSE = args<Args ...>::SSE;
};

template<typename ... Args>
struct args<long long, Args ...> {
    static const int INT = args<Args ...>::INT + 1;
    static const int SSE = args<Args ...>::SSE;
};

template<typename ... Args>
struct args<double, Args ...> {
    static const int INT = args<Args ...>::INT;
    static const int SSE = args<Args ...>::SSE + 1;
};

template<typename ... Args>
struct args<float, Args ...> {
    static const int INT = args<Args ...>::INT;
    static const int SSE = args<Args ...>::SSE + 1;
};

class slab_allocator {

    static const size_t TRAMPOLINE_SIZE = 64; //56 actually
    static void **memory_ptr;

public:

    static void *malloc() {
        if (memory_ptr == nullptr) {
            auto *page_ptr = (char *) mmap(nullptr, 4096, PROT_EXEC | PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            memory_ptr = (void **) page_ptr;
            char *cur = nullptr;
            for (size_t i = TRAMPOLINE_SIZE; i < 4096; i += TRAMPOLINE_SIZE) {
                cur = page_ptr + i;
                *(void **) (cur - TRAMPOLINE_SIZE) = cur;
            }
            *(void **) cur = nullptr;
        }
        void *ptr = memory_ptr;
        memory_ptr = (void **) *memory_ptr;
        return ptr;
    }

    static void free(void *ptr) {
        *(void **) ptr = memory_ptr;
        memory_ptr = (void **) ptr;
    }
};

void **slab_allocator::memory_ptr = nullptr;

static slab_allocator allocator;
template<typename T>
struct trampoline;

template<typename R, typename... Args>
struct trampoline<R(Args...)> {
    template<typename F>
    trampoline(F const &func)
            : func_obj(new F(func)), caller(&do_call < F > ), deleter(do_delete < F > ) {
        code = allocator.malloc();
        pcode = (unsigned char *) code;
        int int_args = args<Args...>::INT;
        if (int_args < 6) {
            //shift registers
            for (int i = int_args - 1; i >= 0; i--) {
                for (int j = 0; j < 3; ++j) {
                    *(pcode++) = register_shift[i][j];
                }
            }
            *pcode++ = 0x48;
            *pcode++ = 0x89;
            *pcode++ = 0xfe;
            // mov rdi, imm
            *(pcode++) = 0x48;
            *(pcode++) = 0xBF;
            *(void **) pcode = func_obj;
            pcode += 8;

            //mov rax, imm
            *(pcode++) = 0x48;
            *(pcode++) = 0xB8;
            *(void **) pcode = (void *) &do_call<F>;
            pcode += 8;

            //jmp rax
            *(pcode++) = 0xFF;
            *(pcode++) = 0xE0;
        } else {
            //5 int args and 8 sse args in registers
            int size = (int_args - 5 + std::max(0, args<Args...>::SSE - 8)) * 8;

            //mov r11 [rsp] save rsp
            *(pcode++) = 0x4C;
            *(pcode++) = 0x8B;
            *(pcode++) = 0x1C;
            *(pcode++) = 0x24;

            // push r9 push 6th int arg to stack
            *(pcode++) = 0x41;
            *(pcode++) = 0x51;

            //shift registers
            for (int i = 4; i >= 0; i--) {
                for (int j = 0; j < 3; ++j) {
                    *(pcode++) = register_shift[i][j];
                }
            }

            //mov rax, rsp
            *(pcode++) = 0x48;
            *(pcode++) = 0x89;
            *(pcode++) = 0xE0;

            //add rsp, $8 for args shifting
            *(pcode++) = 0x48;
            *(pcode++) = 0x81;
            *(pcode++) = 0xC4;
            *(int32_t *) pcode = 8;
            pcode += 4;

            //add rax, size put end address in rax
            *(pcode++) = 0x48;
            *(pcode++) = 0x05;
            *(int32_t *) pcode = size;
            pcode += 4;



            unsigned char *shifting_label = pcode;

            //cmp rax, rsp check if all args are shifted
            *(pcode++) = 0x48;
            *(pcode++) = 0x39;
            *(pcode++) = 0xE0;

            //je next_label
            *(pcode++) = 0x74;

            unsigned char *next_label = pcode;
            ++pcode;

            //add rsp, $8; set rsp to current arg
            *(pcode++) = 0x48;
            *(pcode++) = 0x81;
            *(pcode++) = 0xC4;
            *(int32_t *) pcode = 8;
            pcode += 4;

            //mov rdi, [rsp] save arg to shift
            *(pcode++) = 0x48;
            *(pcode++) = 0x8B;
            *(pcode++) = 0x3C;
            *(pcode++) = 0x24;

            //mov [rsp - 8], rdi place arg to shift
            *(pcode++) = 0x48;
            *(pcode++) = 0x89;
            *(pcode++) = 0x7C;
            *(pcode++) = 0x24;
            *(pcode++) = 0xF8;

            //jmp shifting_label
            *(pcode++) = 0xEB;

            *pcode = shifting_label - pcode - 1;
            ++pcode;

            *next_label = pcode - next_label - 1;

            //mov [rsp], r11 return rsp
            *(pcode++) = 0x4C;
            *(pcode++) = 0x89;
            *(pcode++) = 0x1C;
            *(pcode++) = 0x24;

            //sub rsp, size move rsp to stack top
            *(pcode++) = 0x48;
            *(pcode++) = 0x81;
            *(pcode++) = 0xEC;
            *(int32_t *) pcode = size;
            pcode += 4;

            //calling function
            //mov rdi, imm
            *(pcode++) = 0x48;
            *(pcode++) = 0xBF;
            *(void **) pcode = func_obj;
            pcode += 8;

            //mov rax, imm
            *(pcode++) = 0x48;
            *(pcode++) = 0xB8;
            *(void **) pcode = (void *) &do_call<F>;
            pcode += 8;

            //call rax
            *(pcode++) = 0xFF;
            *(pcode++) = 0xD0;

            //clean stack
            //pop r9
            *(pcode++) = 0x41;
            *(pcode++) = 0x59;

            //mov r11, [rsp + size - 8]
            *(pcode++) = 0x4C;
            *(pcode++) = 0x8B;
            *(pcode++) = 0x9C;
            *(pcode++) = 0x24;
            *(int32_t *) pcode = size - 8;
            pcode += 4;

            //mov [rsp], r11
            *(pcode++) = 0x4C;
            *(pcode++) = 0x89;
            *(pcode++) = 0x1C;
            *(pcode++) = 0x24;

            //ret
            *(pcode++) = 0xC3;

        }
        pcode = nullptr;
    }

    ~trampoline() {
        if (func_obj != nullptr) {
            deleter(func_obj);
        }
        allocator.free(code);
    }

    template<typename F>
    static R do_call(void *obj, Args... args) {
        return (*(F *) obj)(args...);
    }

    template<typename F>
    static void do_delete(void *obj) {
        delete ((F *) obj);
    }

    R (*get() const )(Args... args) {
        return (R (*)(Args... args)) code;
    }

    trampoline &operator=(const trampoline &other) = delete;

private:
    const unsigned char register_shift[5][3] = {
            {0x48, 0x89, 0xFE}, //mov rsi, rdi
            {0x48, 0x89, 0xF2}, //mov rdx, rsi
            {0x48, 0x89, 0xD1}, //mov rcx, rdz
            {0x49, 0x89, 0xC8}, //mov r8, rcx
            {0x4D, 0x89, 0xC1}, //mov r9, r8
    };
    void *func_obj;
    void *code;
    unsigned char *pcode;

    R (*caller)(void *obj, Args... args);

    void (*deleter)(void *);
};

int main() {
    std::cout << sizeof(trampoline<void()>) << std::endl;
    int d = 124;
    trampoline<int(int, int, int)> tr([&](int a, int b, int c) { return printf("%d %d %d %d\n", a, b, c, d); });
    auto p = tr.get();

    assert(10 == p(6, 5, 3));

    trampoline<double(char, short, int, long, long long, double, float)> t2(
            [&](char x0, short x1, int x2, long x3, long long x4, double x5, float x6) {
                return x0 + x1 + x2 + x3 + x4 + x5 + x6;
            });
    assert(1 + 2 + 3 + 4 + 5 + 2.5 + 3.5
           == t2.get()(1, 2, 3, 4, 5, 2.5, 3.5));
    trampoline<double(char, short, int, long, long long, long long, double, float, float, float, float, float, float)> t3(
            [&](char x0, short x1, int x2, long x3, long long x4, long long x5,
                double x6, float x7, float x8, float x9, float x10, float x11, float x12) {
                return x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10 + x11 + x12;
            });

    assert(1 + 2 + 3 + 4 + 5 + 6 + 2.5 + 3.5 + 3.5 + 3.5 + 3.5 + 3.5 + 3.5
           == t3.get()(1, 2, 3, 4, 5, 6, 2.5, 3.5, 3.5, 3.5, 3.5, 3.5, 3.5));
}