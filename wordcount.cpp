#include <iostream>
#include <emmintrin.h>
#include <tmmintrin.h>
#include <assert.h>

const __m128i SPACE_MASK = _mm_set_epi8(32, 32, 32, 32, 32, 32, 32, 32,
                                        32, 32, 32, 32, 32, 32, 32, 32);
const __m128i ONE_MASK = _mm_set_epi8(1, 1, 1, 1, 1, 1, 1, 1,
                                      1, 1, 1, 1, 1, 1, 1, 1);

size_t word_count_naive(std::string &str) {
    size_t ans = 0;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] != ' ' && (i == 0 || str[i - 1] == ' ')) {
            ans++;
        }
    }
    return ans;
}

size_t calc_mask(__m128i mask) {
    size_t res = 0;
    for (size_t i = 0; i < 16; ++i) {
        res += (int) *((uint8_t *) &mask + i);
    }
    return res;
}

size_t word_count(std::string &s) {
    char const *a = s.c_str();
    size_t n = s.length();
    size_t ans = 0;
    size_t ind = 0;
    __m128i res_mask = _mm_setzero_si128();
    bool is_space = true;
    while (ind == 0 || (size_t) (a + ind) % 16 != 0) {
        if (!is_space && a[ind] == ' ') {
            ans++;
        }
        is_space = a[ind] == ' ';
        ind++;
    }
    size_t tail = n - (n - ind) % 16 - 16;
    int cnt = 0;
    for (size_t i = ind; i < tail; i += 16) {
        __m128i mask = _mm_cmpeq_epi8(_mm_loadu_si128((__m128i *) (a + i)), SPACE_MASK);
        __m128i shifted_mask = _mm_cmpeq_epi8(_mm_loadu_si128((__m128i *) (a + i - 1)), SPACE_MASK);
//        __m128i shifted_mask = _mm_alignr_epi8(mask, mask, 1);
        __m128i count_mask = _mm_and_si128(_mm_andnot_si128(shifted_mask, mask), ONE_MASK);
        res_mask = _mm_add_epi8(res_mask, count_mask);
        cnt++;
        if (cnt == 127) {
            ans += calc_mask(res_mask);
            res_mask = _mm_setzero_si128();
            cnt = 0;
        }
    }
    ans += calc_mask(res_mask);
    is_space = a[tail - 1] == ' ';
    for (size_t i = tail; i < n; i++) {
        if (a[i] == ' ' && !is_space) {
            ans++;
        }
        is_space = a[i] == ' ';
    }
    return ans + !is_space;
}


std::string gen_string(int len) {
    std::string s;
    static const char alphanum[] =
            " abc";

    for (int i = 0; i < len; ++i) {
        int rand_ind = rand() % (sizeof(alphanum) - 1);
        s += alphanum[rand_ind];
    }
    return s;
}

int main() {
    for (int t = 0; t < 10; ++t) {
        std::string s = gen_string(1 << 26);
        assert(word_count_naive(s) == word_count(s));
    }
}
