/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file test_mem.c
 * @brief Host test for the static buffer pool (mem.h / mem.c)
 *
 * Build & run (host, no ARM toolchain needed):
 *   clang -std=c11 -Wall -Wextra -Itest -Iinc test/test_mem.c src/mem.c \
 *         -o build/test_mem.exe && build/test_mem.exe
 *
 * test/redef.h shadows the real inc/redef.h (see that file for why).
 */
#include "mem.h"
#include "err.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks++;                                                       \
        if (cond) {                                                     \
            printf("ok   %-58s (%s:%d)\n", #cond, __FILE__, __LINE__);  \
        } else {                                                        \
            printf("FAIL %-58s (%s:%d)\n", #cond, __FILE__, __LINE__);  \
            failures++;                                                 \
        }                                                               \
    } while (0)

_Alignas(8) static mini_uint8_t pool_mem[1024];
_Alignas(8) static mini_uint8_t seg2_mem[700];
_Alignas(8) static mini_uint8_t seg3_mem[256];
_Alignas(8) static mini_uint8_t seg4_mem[256];

/* ---------------------------------------------------------------------- */
static void test_init_validation(void)
{
    buffer_pool_t pool;
    buffer_pool_config_t cfg = { "test", pool_mem, sizeof(pool_mem) };

    printf("--- init validation ---\n");
    CHECK(buffer_pool_init(MINI_NULL, &cfg) == MINI_ERR_INVAL);
    CHECK(buffer_pool_init(&pool, MINI_NULL) == MINI_ERR_INVAL);

    {
        buffer_pool_config_t null_mem_cfg = { "bad", MINI_NULL, sizeof(pool_mem) };
        CHECK(buffer_pool_init(&pool, &null_mem_cfg) == MINI_ERR_INVAL);
    }
    {
        buffer_pool_config_t tiny_cfg = { "tiny", pool_mem, 32 };
        CHECK(buffer_pool_init(&pool, &tiny_cfg) == MINI_ERR_INVAL);
    }

    CHECK(buffer_pool_init(&pool, &cfg) == MINI_OK);
    CHECK(buffer_pool_size(&pool) == sizeof(pool_mem));
    CHECK(buffer_pool_used(&pool) == 0u);
    CHECK(buffer_pool_peak(&pool) == 0u);
    CHECK(buffer_pool_free_space(&pool) == sizeof(pool_mem));
    CHECK(strcmp(pool.name, "test") == 0);
    buffer_pool_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_simple_alloc_free_cycle(void)
{
    buffer_pool_t pool;
    buffer_pool_config_t cfg = { "cycle", pool_mem, sizeof(pool_mem) };
    buffer_block_t* a;
    buffer_block_t* b;

    printf("--- alloc/free cycle ---\n");
    CHECK(buffer_pool_init(&pool, &cfg) == MINI_OK);

    a = buffer_pool_alloc(&pool, 64);
    CHECK(a != MINI_NULL);
    if (a != MINI_NULL)
    {
        CHECK(a->capacity >= 64u);
        CHECK((mini_size_t)((mini_uint8_t*)a->data - pool_mem) < sizeof(pool_mem));
        CHECK((mini_size_t)((mini_uint8_t*)a->raw - pool_mem) < sizeof(pool_mem));
        CHECK(((mini_size_t)a->data % 8u) == 0u); /* BUFF_POOL_ALIGN_SIZE */
        CHECK(buffer_pool_used(&pool) == 1u);
        CHECK(buffer_pool_peak(&pool) == 1u);
    }

    b = buffer_pool_alloc(&pool, 64);
    CHECK(b != MINI_NULL);
    if (b != MINI_NULL)
    {
        CHECK(b != a);
        CHECK(buffer_pool_used(&pool) == 2u);
        CHECK(buffer_pool_peak(&pool) == 2u);
        CHECK(buffer_pool_free_space(&pool) < sizeof(pool_mem));
    }

    buffer_pool_free(&pool, a);
    CHECK(buffer_pool_used(&pool) == 1u);
    buffer_pool_free(&pool, b);
    CHECK(buffer_pool_used(&pool) == 0u);
    /* both blocks were adjacent: coalesced back into one whole pool block */
    CHECK(buffer_pool_free_space(&pool) == sizeof(pool_mem));
    buffer_pool_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_block_io(void)
{
    buffer_pool_t pool;
    buffer_pool_config_t cfg = { "io", pool_mem, sizeof(pool_mem) };
    buffer_block_t* blk;
    mini_uint8_t wbuf[64];
    mini_uint8_t rbuf[64];
    size_t actual = 0u;
    size_t cap;
    size_t i;

    printf("--- block write/read ---\n");
    CHECK(buffer_pool_init(&pool, &cfg) == MINI_OK);
    blk = buffer_pool_alloc(&pool, 64);
    CHECK(blk != MINI_NULL);
    if (blk == MINI_NULL)
        return;
    cap = blk->capacity;

    /* small write / read roundtrip */
    for (i = 0u; i < 40u; i++)
        wbuf[i] = (mini_uint8_t)i;
    CHECK(buffer_block_write(blk, wbuf, 40u, &actual) == MINI_OK);
    CHECK(actual == 40u);
    CHECK(buffer_block_used(blk) == 40u);
    CHECK(buffer_block_space(blk) == cap - 40u);
    actual = 0u;
    memset(rbuf, 0, sizeof(rbuf));
    CHECK(buffer_block_read(blk, rbuf, 40u, &actual) == MINI_OK);
    CHECK(actual == 40u);
    CHECK(memcmp(rbuf, wbuf, 40u) == 0);
    CHECK(buffer_block_used(blk) == 0u);

    /* write past the wrap boundary, then a truncated second write */
    for (i = 0u; i < 60u; i++)
        wbuf[i] = (mini_uint8_t)(0xA0u + i);
    CHECK(buffer_block_write(blk, wbuf, 60u, &actual) == MINI_OK);
    CHECK(actual == 60u);
    CHECK(buffer_block_write(blk, wbuf, 60u, &actual) == MINI_OK);
    CHECK(actual == cap - 60u); /* truncated to remaining space */
    CHECK(buffer_block_used(blk) == cap);
    actual = 0u;
    memset(rbuf, 0, sizeof(rbuf));
    CHECK(buffer_block_read(blk, rbuf, cap, &actual) == MINI_OK);
    CHECK(actual == cap);
    CHECK(memcmp(rbuf, wbuf, 60u) == 0);
    CHECK(memcmp(rbuf + 60u, wbuf, cap - 60u) == 0);

    /* reset clears the ring */
    buffer_block_reset(blk);
    CHECK(buffer_block_used(blk) == 0u);
    CHECK(buffer_block_space(blk) == cap);

    /* wrap-read: tail pushed across the boundary, a read crosses it */
    for (i = 0u; i < 60u; i++)
        wbuf[i] = (mini_uint8_t)(0x10u + i);
    CHECK(buffer_block_write(blk, wbuf, 60u, &actual) == MINI_OK);
    CHECK(actual == 60u);
    actual = 0u;
    memset(rbuf, 0, sizeof(rbuf));
    CHECK(buffer_block_read(blk, rbuf, 60u, &actual) == MINI_OK);
    CHECK(actual == 60u);
    CHECK(memcmp(rbuf, wbuf, 60u) == 0);
    CHECK(buffer_block_used(blk) == 0u);

    buffer_pool_free(&pool, blk);
    buffer_pool_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_peak_and_stats(void)
{
    buffer_pool_t pool;
    buffer_pool_config_t cfg = { "peak", pool_mem, sizeof(pool_mem) };
    buffer_block_t* a;
    buffer_block_t* b;

    printf("--- peak / used tracking ---\n");
    CHECK(buffer_pool_init(&pool, &cfg) == MINI_OK);
    a = buffer_pool_alloc(&pool, 32);
    b = buffer_pool_alloc(&pool, 32);
    CHECK(a != MINI_NULL && b != MINI_NULL);
    CHECK(buffer_pool_used(&pool) == 2u);
    CHECK(buffer_pool_peak(&pool) == 2u);

    buffer_pool_free(&pool, a);
    CHECK(buffer_pool_used(&pool) == 1u);
    CHECK(buffer_pool_peak(&pool) == 2u); /* peak does not drop */

    buffer_pool_reset_peak(&pool);
    CHECK(buffer_pool_peak(&pool) == 1u);

    a = buffer_pool_alloc(&pool, 32);
    CHECK(a != MINI_NULL);
    CHECK(buffer_pool_peak(&pool) == 2u); /* climbed again */

    buffer_pool_free(&pool, a);
    buffer_pool_free(&pool, b);
    buffer_pool_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_isr_variants(void)
{
    buffer_pool_t pool;
    buffer_pool_config_t cfg = { "isr", pool_mem, sizeof(pool_mem) };
    buffer_block_t* blk;

    printf("--- ISR variants ---\n");
    CHECK(buffer_pool_init(&pool, &cfg) == MINI_OK);
    blk = buffer_pool_alloc_isr(&pool, 64);
    CHECK(blk != MINI_NULL);
    CHECK(buffer_pool_used(&pool) == 1u);
    buffer_pool_free_isr(&pool, blk);
    CHECK(buffer_pool_used(&pool) == 0u);
    CHECK(buffer_pool_free_space(&pool) == sizeof(pool_mem));
    buffer_pool_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_exhaustion(void)
{
    buffer_pool_t pool;
    buffer_pool_config_t cfg = { "exh", pool_mem, sizeof(pool_mem) };
    buffer_block_t* blocks[32];
    mini_size_t count = 0u;
    mini_size_t i;
    mini_size_t j;

    printf("--- exhaustion ---\n");
    CHECK(buffer_pool_init(&pool, &cfg) == MINI_OK);
    while (count < 32u)
    {
        buffer_block_t* blk = buffer_pool_alloc(&pool, 200u);
        if (blk == MINI_NULL)
            break;
        for (j = 0u; j < count; j++)
            CHECK(blocks[j] != blk); /* no aliasing */
        blocks[count++] = blk;
    }
    CHECK(count >= 4u); /* 1024 bytes / ~240 per 200-byte block */
    CHECK(buffer_pool_used(&pool) == count);
    CHECK(buffer_pool_peak(&pool) == count);
    CHECK(buffer_pool_alloc(&pool, 200u) == MINI_NULL); /* truly exhausted */

    for (i = 0u; i < count; i++)
        buffer_pool_free(&pool, blocks[i]);
    CHECK(buffer_pool_used(&pool) == 0u);
    CHECK(buffer_pool_free_space(&pool) == sizeof(pool_mem));
    buffer_pool_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_expand(void)
{
    buffer_pool_t pool;
    buffer_pool_config_t cfg = { "expand", pool_mem, sizeof(pool_mem) };
    buffer_block_t* blk;

    printf("--- expand ---\n");
    CHECK(buffer_pool_init(&pool, &cfg) == MINI_OK);
    CHECK(buffer_pool_alloc(&pool, 1000u) == MINI_NULL); /* larger than the 1024 pool */
    CHECK(buffer_pool_expand(&pool, seg2_mem, sizeof(seg2_mem)) == MINI_OK);
    CHECK(buffer_pool_size(&pool) == sizeof(pool_mem) + sizeof(seg2_mem));

    blk = buffer_pool_alloc(&pool, 600u);
    CHECK(blk != MINI_NULL);
    if (blk != MINI_NULL)
    {
        mini_uint8_t wbuf[600];
        mini_uint8_t rbuf[600];
        size_t actual = 0u;
        memset(wbuf, 0x5Au, sizeof(wbuf));
        CHECK(buffer_block_write(blk, wbuf, sizeof(wbuf), &actual) == MINI_OK);
        CHECK(actual == sizeof(wbuf));
        actual = 0u;
        CHECK(buffer_block_read(blk, rbuf, sizeof(rbuf), &actual) == MINI_OK);
        CHECK(memcmp(rbuf, wbuf, sizeof(wbuf)) == 0);
        buffer_pool_free(&pool, blk);
    }

    /* segment table full: pool_mem + seg2_mem + 2 more, then refuse */
    CHECK(buffer_pool_expand(&pool, seg3_mem, sizeof(seg3_mem)) == MINI_OK);
    CHECK(buffer_pool_expand(&pool, seg4_mem, sizeof(seg4_mem)) == MINI_OK);
    CHECK(buffer_pool_expand(&pool, seg2_mem, sizeof(seg2_mem)) == MINI_ERR_NOSPC);

    /* invalid expansion args */
    CHECK(buffer_pool_expand(&pool, MINI_NULL, 256u) == MINI_ERR_INVAL);
    CHECK(buffer_pool_expand(&pool, seg2_mem, 32u) == MINI_ERR_INVAL);
    buffer_pool_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_invalid_args(void)
{
    buffer_pool_t pool;
    buffer_pool_config_t cfg = { "args", pool_mem, sizeof(pool_mem) };
    buffer_block_t* blk;
    size_t actual = 0u;
    mini_uint8_t one = 1u;

    printf("--- invalid args ---\n");
    CHECK(buffer_pool_init(&pool, &cfg) == MINI_OK);

    CHECK(buffer_pool_alloc(MINI_NULL, 32u) == MINI_NULL);
    CHECK(buffer_pool_alloc(&pool, 0u) == MINI_NULL);
    buffer_pool_free(MINI_NULL, MINI_NULL); /* must not crash */
    buffer_pool_free(&pool, MINI_NULL);     /* must not crash */

    blk = buffer_pool_alloc(&pool, 32u);
    CHECK(blk != MINI_NULL);
    if (blk != MINI_NULL)
    {
        CHECK(buffer_block_write(MINI_NULL, &one, 1u, &actual) == MINI_ERR_INVAL);
        CHECK(buffer_block_write(blk, MINI_NULL, 1u, &actual) == MINI_ERR_INVAL);
        CHECK(buffer_block_read(MINI_NULL, &one, 1u, &actual) == MINI_ERR_INVAL);
        CHECK(buffer_block_read(blk, MINI_NULL, 1u, &actual) == MINI_ERR_INVAL);
        CHECK(buffer_block_used(MINI_NULL) == 0u);
        CHECK(buffer_block_space(MINI_NULL) == 0u);
        buffer_block_reset(MINI_NULL); /* must not crash */
        buffer_pool_free(&pool, blk);
    }

    CHECK(buffer_pool_size(MINI_NULL) == 0u);
    CHECK(buffer_pool_free_space(MINI_NULL) == 0u);
    CHECK(buffer_pool_used(MINI_NULL) == 0u);
    CHECK(buffer_pool_peak(MINI_NULL) == 0u);
    buffer_pool_reset_peak(MINI_NULL); /* must not crash */
    buffer_pool_deinit(MINI_NULL);     /* must not crash */
    buffer_pool_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_deinit_reinit(void)
{
    buffer_pool_t pool;
    buffer_pool_config_t cfg = { "reinit", pool_mem, sizeof(pool_mem) };
    buffer_block_t* blk;

    printf("--- deinit / reinit ---\n");
    CHECK(buffer_pool_init(&pool, &cfg) == MINI_OK);
    blk = buffer_pool_alloc(&pool, 64u);
    CHECK(blk != MINI_NULL);
    buffer_pool_deinit(&pool);
    CHECK(buffer_pool_size(&pool) == 0u);
    CHECK(buffer_pool_used(&pool) == 0u);
    CHECK(buffer_pool_free_space(&pool) == 0u);

    /* re-init with a different name works; old block handle is dead */
    {
        buffer_pool_config_t renamed = { "R", pool_mem, sizeof(pool_mem) };
        CHECK(buffer_pool_init(&pool, &renamed) == MINI_OK);
        CHECK(strcmp(pool.name, "R") == 0);
    }
    CHECK(buffer_pool_free_space(&pool) == sizeof(pool_mem));
    blk = buffer_pool_alloc(&pool, 128u);
    CHECK(blk != MINI_NULL);
    buffer_pool_free(&pool, blk);
    buffer_pool_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
int main(void)
{
    printf("mini-os buffer pool host test\n");
    printf("pool_mem=%zu bytes, seg2_mem=%zu bytes\n", sizeof(pool_mem), sizeof(seg2_mem));

    test_init_validation();
    test_simple_alloc_free_cycle();
    test_block_io();
    test_peak_and_stats();
    test_isr_variants();
    test_exhaustion();
    test_expand();
    test_invalid_args();
    test_deinit_reinit();

    printf("----------------------------------------------\n");
    printf("%d checks, %d failure(s)\n", checks, failures);
    if (failures == 0)
        printf("ALL TESTS PASSED\n");
    return failures == 0 ? 0 : 1;
}
