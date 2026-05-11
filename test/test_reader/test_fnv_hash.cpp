#include <cstdint>
#include <cstring>
#include <unity.h>

static uint32_t fnv1a32(const char *s)
{
    uint32_t hash = 2166136261u;
    while (*s) {
        hash ^= (uint8_t)*s++;
        hash *= 16777619u;
    }
    return hash;
}

void test_fnv_hash_deterministic()
{
    uint32_t h1 = fnv1a32("/sd/Test.epub");
    uint32_t h2 = fnv1a32("/sd/Test.epub");
    TEST_ASSERT_EQUAL_UINT32(h1, h2);
}

void test_fnv_hash_different_paths_different_hashes()
{
    uint32_t h1 = fnv1a32("/sd/Book1.epub");
    uint32_t h2 = fnv1a32("/sd/Book2.epub");
    TEST_ASSERT_NOT_EQUAL(h1, h2);
}

void test_fnv_hash_known_values()
{
    TEST_ASSERT_EQUAL_UINT32(0xef9e4f91u, fnv1a32("a"));
    TEST_ASSERT_EQUAL_UINT32(0x2f9a9c91u, fnv1a32("abc"));
    TEST_ASSERT_EQUAL_UINT32(0x8e1c56e8u, fnv1a32("message"));
}

void test_fnv_hash_path_collision_spotcheck()
{
    uint32_t h1 = fnv1a32("/sd/book.epub");
    uint32_t h2 = fnv1a32("/sd/book1.epub");
    TEST_ASSERT_NOT_EQUAL(h1, h2);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_fnv_hash_deterministic);
    RUN_TEST(test_fnv_hash_different_paths_different_hashes);
    RUN_TEST(test_fnv_hash_known_values);
    RUN_TEST(test_fnv_hash_path_collision_spotcheck);
    exit(UNITY_END());
}

void loop() {}
