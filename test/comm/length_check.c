/*
 * Copyright 2021 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdint.h>


static uint8_t count_table_u8[256];

static void generate_table() {
    printf("static uint8_t count_table_u8[256] = {");
    for (int i = 0; i < 256; ++i) {
        int k = i;
        if ((i & 0xf) == 0) {
            printf("\n    ");
        }
        while (k) {
            if (k & 1) {
                ++count_table_u8[i];
            }
            k >>= 1;
        }
        printf("%d, ", count_table_u8[i]);
    }
    printf("\n};\n\n");
}

static inline uint8_t count_u16(uint16_t x) {
    return count_table_u8[x & 0xff] + count_table_u8[(x >> 8) & 0xff];
}

static void print_x8_table(uint8_t const * tbl) {
    printf("static uint8_t crc_table[] = {\n");
    for (int i = 0; i < 256; ++i) {
        if ((i & 0xf) == 0) {
            printf("\n    ");
        }
        printf("0x%02x, ", tbl[i]);
    }
    printf("\n};\n");
}

static uint8_t hamming_distance(uint8_t const * arry) {
    uint8_t hd = 8;
    uint8_t hd_this;

    uint16_t a16;
    uint16_t b16;
    uint8_t * a8 = (uint8_t *) &a16;
    uint8_t * b8 = (uint8_t *) &b16;

    for (int a = 0; a < 255; ++a) {
        a8[0] = (uint8_t) a;
        a8[1] = arry[a];
        for (int b = a + 1; b < 256; ++b) {
            b8[0] = (uint8_t) b;
            b8[1] = arry[b];
            hd_this = count_u16(a16 ^ b16);
            if (hd_this < hd) {
                hd = hd_this;
            }
        }
    }
    return hd;
}

static uint8_t hd_threshold = 6;

static uint8_t hamming_distance_prior(uint8_t const * arry, int depth) {
    uint8_t hd = 8;
    uint8_t hd_this;

    uint16_t a16;
    uint16_t b16;
    uint8_t * a8 = (uint8_t *) &a16;
    uint8_t * b8 = (uint8_t *) &b16;
    b8[0] = (uint8_t) depth;
    b8[1] = arry[depth];

    for (int a = 0; a < depth; ++a) {
        a8[0] = (uint8_t) a;
        a8[1] = arry[a];
        hd_this = count_u16(a16 ^ b16);
        if (hd_this < hd_threshold) {
            return hd_this;
        } else if (hd_this < hd) {
            hd = hd_this;
        }
    }
    return hd;
}

static uint8_t hd_search_k(uint8_t * assign, int depth) {
    uint8_t hd = 0;
    uint8_t hd_this;
    if (depth == 255) {
        hd = hamming_distance(assign);
        if (hd >= hd_threshold) {
            printf("Found hd %d:\n", (int) hd);
            print_x8_table(assign);
        }
        return hd;
    }

    uint8_t src = assign[depth];
    uint8_t tgt;
    for (int i = depth; i < 256; ++i) {
        if (depth == 1) {
            printf("%d\n", i);
        }
        tgt = assign[i];
        assign[depth] = tgt;
        assign[i] = src;
        hd_this = hamming_distance_prior(assign, i);
        if (hd_this >= hd_threshold) {
            hd_this = hd_search_k(assign, depth + 1);
            if (hd_this > hd) {
                hd = hd_this;
            }
        }
        assign[i] = tgt;
    }
    assign[depth] = src;
    return hd;
}

static void hd_search() {
    printf("\n");
    printf("Can we do better than CRC-8 poly 0x1d7?\n");
    printf("Perform an exhaustive table search for maximum Hamming distance.\n");
    printf("Search is factorial time, but with shortcuts.  Be patient.\n");
    uint8_t assign[256];
    for (int i = 0; i < 256; ++i) {
        assign[i] = (uint8_t) (i);
    }
    uint8_t hd = hd_search_k(assign, 1);
    printf("Best Hamming distance: %d\n", (int) hd);
}

int main(void) {
    generate_table();
    hd_search();
    return 0;
}
