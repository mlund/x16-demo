/**
 * Plasma effect sourced from cc65/samples/cbm
 * - 2001 by groepaz
 * - Cleanup and porting by Ullrich von Bassewitz.
 * - 2023 cx16/llvm-mos c++ adaptation (wombat)
 *   Clangs `#pragma unroll` gives a significant speedup when using the -Os optimization level.
 */

#include <cstdint>
#include <array>
#include <initializer_list>
#include "cx16mod.h"

#define CHARSET_ADDRESS 0x3000
#define POKE(X, Y) (*(volatile unsigned char*)(X)) = Y

static const uint8_t sinustable[256] = {
//static const std::array<uint8_t, 256> sinustable = {{
    0x80, 0x7d, 0x7a, 0x77, 0x74, 0x70, 0x6d, 0x6a, 0x67, 0x64, 0x61, 0x5e, 0x5b, 0x58, 0x55, 0x52, 0x4f, 0x4d, 0x4a,
    0x47, 0x44, 0x41, 0x3f, 0x3c, 0x39, 0x37, 0x34, 0x32, 0x2f, 0x2d, 0x2b, 0x28, 0x26, 0x24, 0x22, 0x20, 0x1e, 0x1c,
    0x1a, 0x18, 0x16, 0x15, 0x13, 0x11, 0x10, 0x0f, 0x0d, 0x0c, 0x0b, 0x0a, 0x08, 0x07, 0x06, 0x06, 0x05, 0x04, 0x03,
    0x03, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06,
    0x06, 0x07, 0x08, 0x0a, 0x0b, 0x0c, 0x0d, 0x0f, 0x10, 0x11, 0x13, 0x15, 0x16, 0x18, 0x1a, 0x1c, 0x1e, 0x20, 0x22,
    0x24, 0x26, 0x28, 0x2b, 0x2d, 0x2f, 0x32, 0x34, 0x37, 0x39, 0x3c, 0x3f, 0x41, 0x44, 0x47, 0x4a, 0x4d, 0x4f, 0x52,
    0x55, 0x58, 0x5b, 0x5e, 0x61, 0x64, 0x67, 0x6a, 0x6d, 0x70, 0x74, 0x77, 0x7a, 0x7d, 0x80, 0x83, 0x86, 0x89, 0x8c,
    0x90, 0x93, 0x96, 0x99, 0x9c, 0x9f, 0xa2, 0xa5, 0xa8, 0xab, 0xae, 0xb1, 0xb3, 0xb6, 0xb9, 0xbc, 0xbf, 0xc1, 0xc4,
    0xc7, 0xc9, 0xcc, 0xce, 0xd1, 0xd3, 0xd5, 0xd8, 0xda, 0xdc, 0xde, 0xe0, 0xe2, 0xe4, 0xe6, 0xe8, 0xea, 0xeb, 0xed,
    0xef, 0xf0, 0xf1, 0xf3, 0xf4, 0xf5, 0xf6, 0xf8, 0xf9, 0xfa, 0xfa, 0xfb, 0xfc, 0xfd, 0xfd, 0xfe, 0xfe, 0xfe, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xfd, 0xfd, 0xfc, 0xfb, 0xfa, 0xfa, 0xf9, 0xf8, 0xf6, 0xf5,
    0xf4, 0xf3, 0xf1, 0xf0, 0xef, 0xed, 0xeb, 0xea, 0xe8, 0xe6, 0xe4, 0xe2, 0xe0, 0xde, 0xdc, 0xda, 0xd8, 0xd5, 0xd3,
    0xd1, 0xce, 0xcc, 0xc9, 0xc7, 0xc4, 0xc1, 0xbf, 0xbc, 0xb9, 0xb6, 0xb3, 0xb1, 0xae, 0xab, 0xa8, 0xa5, 0xa2, 0x9f,
    0x9c, 0x99, 0x96, 0x93, 0x90, 0x8c, 0x89, 0x86, 0x83};

/**
 * @brief Simple pseudo-random number generator
 *
 * See https://en.wikipedia.org/wiki/Xorshift
 */
class xorshift_rng {
  private:
    uint32_t state = 7; //!< Random number state
  public:
    inline uint8_t rand8() { return (uint8_t)(rand32() & 0xff); }
    inline uint32_t rand32() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
};

// Global instance of random number generator
xorshift_rng rng;

void make_charset() {
    // Lambda function to generate a single 8x8 bit character
    auto make_char = [](const uint8_t sine) {
        uint8_t char_pattern = 0;
        const uint8_t bits[8] = {1, 2, 4, 8, 16, 32, 64, 128};
        for (const auto bit : bits) {
            if (rng.rand8() > sine) {
                char_pattern |= bit;
            }
        }
        return char_pattern;
    };

    uint16_t dst = CHARSET_ADDRESS;
    for (const auto sine : sinustable) {
        for (int i = 0; i < 8; ++i) {
            POKE(dst++, make_char(sine));
        }
    }
}

// See here for information about the VERA screen memory region:
// https://github.com/mwiedmann/cx16CodingInC/tree/main/Chapter07-MapBase
template <unsigned short COLS, unsigned short ROWS> void draw() {
    static std::array<uint8_t, COLS> xbuffer;
    static std::array<uint8_t, ROWS> ybuffer;
    static uint8_t c1A = 0;
    static uint8_t c1B = 0;
    static uint8_t c2A = 0;
    static uint8_t c2B = 0;

    uint8_t c2a = c2A;
    uint8_t c2b = c2B;
    uint8_t c1a = c1A;
    uint8_t c1b = c1B;
    for (auto& y : ybuffer) {
        y = sinustable[c1a] + sinustable[c1b];
        c1a += 4;
        c1b += 9;
    }
    c1A += 3;
    c1B -= 5;
    c2a = c2A;
    c2b = c2B;
    for (auto& x : xbuffer) {
        x = sinustable[c2a] + sinustable[c2b];
        c2a += 3;
        c2b += 7;
    }
    c2A += 2;
    c2B -= 3;

    // Set a 2 byte stride (one text, one color) after each write
    VERA.address_hi = VERA_INC_2 | 1;
    VERA.control = 0;
    unsigned short row_cnt = 0;
    for (const auto y : ybuffer) {
        VERA.address = 0xb000 + (2 * 128 * row_cnt++);
#pragma unroll
        for (const auto x : xbuffer) {
            VERA.data0 = x + y;
        }
    }
}

int main() {
    make_charset();
    cx16_k_screen_set_charset(0, (uint8_t*)CHARSET_ADDRESS);
    while (true) {
        draw<80, 60>();
    }
    return 0;
}
