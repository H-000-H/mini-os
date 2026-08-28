/*
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @file test_mem.c
 * @brief Host test for the malloc/free model allocator (memory.h / memory.c)
 *
 * Build & run (host, no ARM toolchain needed):
 *   clang -std=c11 -Wall -Wextra -Itest -Iinc -include redef.h test/test_mem.c src/memory.c \
 *         -o build/test_mem.exe && build/test_mem.exe
 *
 * With slab (small-allocation path):
 *   clang -std=c11 -Wall -Wextra -DCONFIG_OPEN_SLAB -DCONFIG_MINI_MALLOC_SLAB_PAGE_SIZE=256 \
 *         -Itest -Iinc -include redef.h test/test_mem.c src/memory.c \
 *         -o build/test_mem_slab.exe && build/test_mem_slab.exe
 *
 * -include redef.h 让 test/redef.h 宿主桩先于任何源文件注入, 靠 REDEF_H 保护宏
 * 屏蔽 inc/redef.h (内含 Cortex-M 内联汇编, 宿主无法编译)。
 */
#include "memory.h"
#include "err.h"
#ifdef CONFIG_OPEN_SLAB
#include "mem_heap.h" /* MINI_OS_SLAB_PAGE_SIZE 等 slab 配置宏 */
#endif

#include <stddef.h>
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

/* slab 构建下 init 会从池首划走固定区, 池空闲上限 = 总量 - slab 区 */
#ifdef CONFIG_OPEN_SLAB
#define TEST_FREE_CAP(p, total) ((total) - (p).slab_size)
#else
#define TEST_FREE_CAP(p, total) (total)
#endif

/* Linker heap symbol stubs: 宿主构建没有链接脚本, 用一段静态缓冲模拟堆区:
 * start 为缓冲首地址, end 紧随其后, MINI_OS_HEAP_SIZE 即缓冲大小。
 * 启动构造器 (优先级 MINI_OS_MEMORY_PRESTRUCTOR) 会在 main 前接管它。 */
char __mini_os_heap_start[8192] __attribute__((aligned(8)));
char __mini_os_heap_end[1];

_Alignas(8) static mini_os_uint8_t pool_mem[1024];
_Alignas(8) static mini_os_uint8_t seg2_mem[700];
_Alignas(8) static mini_os_uint8_t seg3_mem[256];
_Alignas(8) static mini_os_uint8_t seg4_mem[256];
#ifdef CONFIG_OPEN_SLAB
_Alignas(8) static mini_os_uint8_t slab_pool_mem[2048];
#endif

/* ---------------------------------------------------------------------- */
static void test_global_heap(void)
{
    void* p;
    void* q;

    printf("--- global heap (host stub region) ---\n");
    /* 构造器已在 main 前接管桩堆区, 分配应当可用 */
    p = mini_os_malloc(32u);
    CHECK(p != MINI_OS_NULL);
    if (p != MINI_OS_NULL)
    {
        memset(p, 0x5Au, 32u);
        CHECK(((mini_os_uint8_t*)p)[31] == 0x5Au);
    }
    q = mini_os_malloc(64u);
    CHECK(q != MINI_OS_NULL);
    CHECK(q != p);
    CHECK(mini_os_free(p) == MINI_OS_OK);
    CHECK(mini_os_free(q) == MINI_OS_OK);
    CHECK(mini_os_free(MINI_OS_NULL) == MINI_OS_ERR_INVAL);
}

/* ---------------------------------------------------------------------- */
static void test_calloc(void)
{
    mini_os_uint8_t* p;
    mini_os_size_t i;

    printf("--- calloc (global heap) ---\n");
    p = mini_os_calloc(4u, 64u); /* 256 字节清零 */
    CHECK(p != MINI_OS_NULL);
    if (p != MINI_OS_NULL)
    {
        for (i = 0u; i < 256u; i++)
        {
            CHECK(p[i] == 0u);
        }
        p[255] = 0xABu;
        CHECK(mini_os_free(p) == MINI_OS_OK);
    }
    CHECK(mini_os_calloc(0u, 16u) == MINI_OS_NULL); /* 0 尺寸跟随 malloc 语义 */
    CHECK(mini_os_calloc(4u, 0u) == MINI_OS_NULL);
    CHECK(mini_os_calloc((mini_os_size_t)-1, 16u) == MINI_OS_NULL); /* 乘法溢出拒绝 */
}

/* ---------------------------------------------------------------------- */
static void test_init_validation(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "test", pool_mem, sizeof(pool_mem) };

    printf("--- init validation ---\n");
    CHECK(mini_os_memory_init(MINI_OS_NULL, &cfg) == MINI_OS_ERR_INVAL);
    CHECK(mini_os_memory_init(&pool, MINI_OS_NULL) == MINI_OS_ERR_INVAL);

    {
        mini_os_memory_config_t null_mem_cfg = { "bad", MINI_OS_NULL, sizeof(pool_mem) };
        CHECK(mini_os_memory_init(&pool, &null_mem_cfg) == MINI_OS_ERR_INVAL);
    }
    {
        mini_os_memory_config_t tiny_cfg = { "tiny", pool_mem, 32 };
        CHECK(mini_os_memory_init(&pool, &tiny_cfg) == MINI_OS_ERR_INVAL);
    }

    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);
    CHECK(mini_os_memory_size(&pool) == sizeof(pool_mem));
    CHECK(mini_os_memory_used(&pool) == 0u);
    CHECK(mini_os_memory_peak(&pool) == 0u);
    CHECK(mini_os_memory_free_space(&pool) == TEST_FREE_CAP(pool, sizeof(pool_mem)));
    CHECK(strcmp(pool.name, "test") == 0);
    CHECK(mini_os_memory_deinit(&pool) == MINI_OS_OK);
}

/* ---------------------------------------------------------------------- */
static void test_alloc_free_cycle(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "cycle", pool_mem, sizeof(pool_mem) };
    void* a;
    void* b;

    printf("--- alloc/free cycle ---\n");
    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);

    a = mini_os_memory_alloc(&pool, 64u);
    CHECK(a != MINI_OS_NULL);
    if (a != MINI_OS_NULL)
    {
        CHECK((((mini_os_size_t)a) % 8u) == 0u); /* malloc 语义: 8 对齐裸指针 */
        CHECK((mini_os_uint8_t*)a >= pool_mem);
        CHECK((mini_os_uint8_t*)a + 64u <= pool_mem + sizeof(pool_mem));
        CHECK(mini_os_memory_used(&pool) == 1u);
        CHECK(mini_os_memory_peak(&pool) == 1u);
    }

    b = mini_os_memory_alloc(&pool, 64u);
    CHECK(b != MINI_OS_NULL);
    if (b != MINI_OS_NULL)
    {
        CHECK(b != a);
        CHECK(mini_os_memory_used(&pool) == 2u);
        CHECK(mini_os_memory_peak(&pool) == 2u);
        CHECK(mini_os_memory_free_space(&pool) < sizeof(pool_mem));
    }

    CHECK(mini_os_memory_free(&pool, a) == MINI_OS_OK);
    CHECK(mini_os_memory_used(&pool) == 1u);
    CHECK(mini_os_memory_free(&pool, b) == MINI_OS_OK);
    CHECK(mini_os_memory_used(&pool) == 0u);
    /* 两块地址相邻: 释放后合并回整池单块 */
    CHECK(mini_os_memory_free_space(&pool) == TEST_FREE_CAP(pool, sizeof(pool_mem)));
    mini_os_memory_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_data_roundtrip(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "data", pool_mem, sizeof(pool_mem) };
    void* blk;
    mini_os_uint8_t pattern[64];
    mini_os_size_t i;

    printf("--- data roundtrip (no FIFO, caller owns bytes) ---\n");
    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);
    blk = mini_os_memory_alloc(&pool, 64u);
    CHECK(blk != MINI_OS_NULL);
    if (blk == MINI_OS_NULL)
    {
        return;
    }
    /* 直接按裸指针读写, 无块内指针管理 */
    for (i = 0u; i < 64u; i++)
    {
        pattern[i] = (mini_os_uint8_t)(0xA0u + i);
    }
    memcpy(blk, pattern, sizeof(pattern));
    CHECK(memcmp(blk, pattern, sizeof(pattern)) == 0);
    memset(blk, 0x5Au, 64u);
    CHECK(((mini_os_uint8_t*)blk)[0] == 0x5Au);
    CHECK(((mini_os_uint8_t*)blk)[63] == 0x5Au);

    mini_os_memory_free(&pool, blk);
    CHECK(mini_os_memory_used(&pool) == 0u);

    /* 释放后重新分配: 同地址复用 (整池唯一空闲块) */
    blk = mini_os_memory_alloc(&pool, 32u);
    CHECK(blk != MINI_OS_NULL);
    mini_os_memory_free(&pool, blk);
    mini_os_memory_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_peak_and_stats(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "peak", pool_mem, sizeof(pool_mem) };
    void* a;
    void* b;

    printf("--- peak / used tracking ---\n");
    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);
    a = mini_os_memory_alloc(&pool, 32u);
    b = mini_os_memory_alloc(&pool, 32u);
    CHECK(a != MINI_OS_NULL && b != MINI_OS_NULL);
    CHECK(mini_os_memory_used(&pool) == 2u);
    CHECK(mini_os_memory_peak(&pool) == 2u);

    mini_os_memory_free(&pool, a);
    CHECK(mini_os_memory_used(&pool) == 1u);
    CHECK(mini_os_memory_peak(&pool) == 2u); /* 峰值不随释放下降 */

    mini_os_memory_reset_peak(&pool);
    CHECK(mini_os_memory_peak(&pool) == 1u);

    a = mini_os_memory_alloc(&pool, 32u);
    CHECK(a != MINI_OS_NULL);
    CHECK(mini_os_memory_peak(&pool) == 2u); /* 重新攀升 */

    mini_os_memory_free(&pool, a);
    mini_os_memory_free(&pool, b);
    mini_os_memory_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_isr_variants(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "isr", pool_mem, sizeof(pool_mem) };
    void* blk;

    printf("--- ISR variants ---\n");
    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);
    blk = mini_os_memory_alloc_isr(&pool, 64u);
    CHECK(blk != MINI_OS_NULL);
    CHECK(mini_os_memory_used(&pool) == 1u);
    mini_os_memory_free_isr(&pool, blk);
    CHECK(mini_os_memory_used(&pool) == 0u);
    CHECK(mini_os_memory_free_space(&pool) == TEST_FREE_CAP(pool, sizeof(pool_mem)));
    mini_os_memory_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_exhaustion(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "exh", pool_mem, sizeof(pool_mem) };
    void* blocks[32];
    mini_os_size_t count = 0u;
    mini_os_size_t i;
    mini_os_size_t j;

    printf("--- exhaustion ---\n");
    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);
    while (count < 32u)
    {
        void* blk = mini_os_memory_alloc(&pool, 200u);
        if (blk == MINI_OS_NULL)
        {
            break;
        }
        for (j = 0u; j < count; j++)
        {
            CHECK(blocks[j] != blk); /* 无重叠分配 */
        }
        blocks[count++] = blk;
    }
    CHECK(count >= (mini_os_size_t)(TEST_FREE_CAP(pool, sizeof(pool_mem)) / 224u)); /* 空闲区/每块(200+块头, 宿主 64 位块头 24) */
    CHECK(mini_os_memory_used(&pool) == count);
    CHECK(mini_os_memory_peak(&pool) == count);
    CHECK(mini_os_memory_alloc(&pool, 200u) == MINI_OS_NULL); /* 真正耗尽 */

    for (i = 0u; i < count; i++)
    {
        mini_os_memory_free(&pool, blocks[i]);
    }
    CHECK(mini_os_memory_used(&pool) == 0u);
    CHECK(mini_os_memory_free_space(&pool) == TEST_FREE_CAP(pool, sizeof(pool_mem)));
    mini_os_memory_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_expand(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "expand", pool_mem, sizeof(pool_mem) };
    void* blk;

    printf("--- expand ---\n");
    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);
    CHECK(mini_os_memory_alloc(&pool, 1008u) == MINI_OS_NULL); /* 1008+块头 > 1024 池 */
    CHECK(mini_os_memory_expand(&pool, seg2_mem, sizeof(seg2_mem)) == MINI_OS_OK);
    CHECK(mini_os_memory_size(&pool) == sizeof(pool_mem) + sizeof(seg2_mem));

    blk = mini_os_memory_alloc(&pool, 600u);
    CHECK(blk != MINI_OS_NULL);
    if (blk != MINI_OS_NULL)
    {
        memset(blk, 0x5Au, 600u);
        CHECK(((mini_os_uint8_t*)blk)[599] == 0x5Au);
        mini_os_memory_free(&pool, blk);
    }

    /* 段表满: 再加两段后拒绝 */
    CHECK(mini_os_memory_expand(&pool, seg3_mem, sizeof(seg3_mem)) == MINI_OS_OK);
    CHECK(mini_os_memory_expand(&pool, seg4_mem, sizeof(seg4_mem)) == MINI_OS_OK);
    CHECK(mini_os_memory_expand(&pool, seg2_mem, sizeof(seg2_mem)) == MINI_OS_ERR_NOSPC);

    /* 非法扩容入参 */
    CHECK(mini_os_memory_expand(&pool, MINI_OS_NULL, 256u) == MINI_OS_ERR_INVAL);
    CHECK(mini_os_memory_expand(&pool, seg2_mem, 32u) == MINI_OS_ERR_INVAL);
    mini_os_memory_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
static void test_invalid_args(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "args", pool_mem, sizeof(pool_mem) };

    printf("--- invalid args ---\n");
    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);

    CHECK(mini_os_memory_alloc(MINI_OS_NULL, 32u) == MINI_OS_NULL);
    CHECK(mini_os_memory_alloc(&pool, 0u) == MINI_OS_NULL);
    CHECK(mini_os_memory_free(MINI_OS_NULL, MINI_OS_NULL) == MINI_OS_ERR_INVAL);
    CHECK(mini_os_memory_free(&pool, MINI_OS_NULL) == MINI_OS_ERR_INVAL);

    CHECK(mini_os_memory_size(MINI_OS_NULL) == 0u);
    CHECK(mini_os_memory_free_space(MINI_OS_NULL) == 0u);
    CHECK(mini_os_memory_used(MINI_OS_NULL) == 0u);
    CHECK(mini_os_memory_peak(MINI_OS_NULL) == 0u);
    CHECK(mini_os_memory_reset_peak(MINI_OS_NULL) == MINI_OS_ERR_INVAL);
    CHECK(mini_os_memory_deinit(MINI_OS_NULL) == MINI_OS_ERR_INVAL);
    CHECK(mini_os_memory_deinit(&pool) == MINI_OS_OK);
}

/* ---------------------------------------------------------------------- */
static void test_deinit_reinit(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "reinit", pool_mem, sizeof(pool_mem) };
    void* blk;

    printf("--- deinit / reinit ---\n");
    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);
    blk = mini_os_memory_alloc(&pool, 64u);
    CHECK(blk != MINI_OS_NULL);
    mini_os_memory_deinit(&pool);
    CHECK(mini_os_memory_size(&pool) == 0u);
    CHECK(mini_os_memory_used(&pool) == 0u);
    CHECK(mini_os_memory_free_space(&pool) == 0u);

    /* 换名重新初始化; 旧指针已失效 */
    {
        mini_os_memory_config_t renamed = { "R", pool_mem, sizeof(pool_mem) };
        CHECK(mini_os_memory_init(&pool, &renamed) == MINI_OS_OK);
        CHECK(strcmp(pool.name, "R") == 0);
    }
    CHECK(mini_os_memory_free_space(&pool) == TEST_FREE_CAP(pool, sizeof(pool_mem)));
    blk = mini_os_memory_alloc(&pool, 128u);
    CHECK(blk != MINI_OS_NULL);
    mini_os_memory_free(&pool, blk);
    mini_os_memory_deinit(&pool);
}

/* ---------------------------------------------------------------------- */
#ifdef CONFIG_OPEN_SLAB
static void test_slab(void)
{
    mini_os_memory_t pool;
    mini_os_memory_config_t cfg = { "slab", slab_pool_mem, sizeof(slab_pool_mem) };
    void* blocks[32];
    void* overflow;
    mini_os_size_t base_free;
    mini_os_size_t slot_total;
    mini_os_size_t i;
    mini_os_size_t j;

    printf("--- slab small allocation (CONFIG_OPEN_SLAB, fixed zone) ---\n");
    /* 池小于 页大小*比例 (256*4=1KB): 配置了 slab 却划不出 1 页, init 直接失败 */
    {
        _Alignas(8) static mini_os_uint8_t tiny_mem[512];
        mini_os_memory_config_t tiny_cfg = { "tiny", tiny_mem, sizeof(tiny_mem) };

        CHECK(mini_os_memory_init(&pool, &tiny_cfg) == MINI_OS_ERR_NOMEM);
    }
    CHECK(mini_os_memory_init(&pool, &cfg) == MINI_OS_OK);
    /* init 一次性划出固定区: 页数 = min(PAGE_MAX, (2048/4)/256) = 2, 无页头共 32 槽 */
    CHECK(pool.slab_page_count == 2u);
    CHECK(pool.slab_size == 2u * MINI_OS_SLAB_PAGE_SIZE);
    slot_total = pool.slab_size / MINI_OS_SLAB_MINI_BYTES;
    CHECK(slot_total == 32u);
    /* freelist 区从 slab 区之后开始, 空闲字节 = 池总 - slab 区 */
    base_free = mini_os_memory_free_space(&pool);
    CHECK(base_free == sizeof(slab_pool_mem) - pool.slab_size);

    /* 填满固定区全部槽 (首个小分配不再触发切页) */
    for (i = 0u; i < slot_total; i++)
    {
        blocks[i] = mini_os_memory_alloc(&pool, 16u);
        CHECK(blocks[i] != MINI_OS_NULL);
        CHECK((((mini_os_size_t)blocks[i]) % 8u) == 0u);
        for (j = 0u; j < i; j++)
        {
            CHECK(blocks[j] != blocks[i]);
        }
    }
    CHECK(pool.slab_page_count == 2u); /* 全程不再切页 */
    CHECK(mini_os_memory_used(&pool) == slot_total);

    /* 槽满: 下一个小分配回退 freelist */
    overflow = mini_os_memory_alloc(&pool, 16u);
    CHECK(overflow != MINI_OS_NULL);
    for (j = 0u; j < slot_total; j++)
    {
        CHECK(blocks[j] != overflow);
    }
    CHECK(pool.slab_page_count == 2u);

    /* 释放一个槽 -> next 指针重写回空槽链 -> 下次小分配立即复用 (LIFO) */
    mini_os_memory_free(&pool, blocks[7]);
    CHECK(mini_os_memory_used(&pool) == slot_total);
    {
        void* reused = mini_os_memory_alloc(&pool, 16u);
        CHECK(reused == blocks[7]);
        blocks[7] = reused;
    }

    /* 全部归还 */
    for (i = 0u; i < slot_total; i++)
    {
        mini_os_memory_free(&pool, blocks[i]);
    }
    mini_os_memory_free(&pool, overflow);
    CHECK(mini_os_memory_used(&pool) == 0u);
    /* 固定区常驻不归还 freelist: 空闲字节回到 init 后水平 */
    CHECK(mini_os_memory_free_space(&pool) == base_free);
    mini_os_memory_deinit(&pool);
}
#endif /* CONFIG_OPEN_SLAB */

/* ---------------------------------------------------------------------- */
int main(void)
{
    printf("mini-os memory (malloc/free model) host test\n");
#ifdef CONFIG_OPEN_SLAB
    printf("CONFIG_OPEN_SLAB on: page=%u max_pages=%u proportion=%u\n",
           (unsigned)MINI_OS_SLAB_PAGE_SIZE, (unsigned)MINI_OS_SLAB_PAGE_MAX, (unsigned)MINI_OS_SLAB_PROPORTION);
#endif

    test_global_heap();
    test_calloc();
    test_init_validation();
    test_alloc_free_cycle();
    test_data_roundtrip();
    test_peak_and_stats();
    test_isr_variants();
    test_exhaustion();
    test_expand();
    test_invalid_args();
    test_deinit_reinit();
#ifdef CONFIG_OPEN_SLAB
    test_slab();
#endif

    printf("----------------------------------------------\n");
    printf("%d checks, %d failure(s)\n", checks, failures);
    if (failures == 0)
    {
        printf("ALL TESTS PASSED\n");
    }
    return failures == 0 ? 0 : 1;
}
