#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unity.h>

// Mirrors ProgressBin struct to test round-trip without depending on SD/reader code.
struct ProgressBin {
    int32_t chapterIndex;
    int32_t pageOffset;
    uint32_t timestamp;
};

void test_progress_bin_size()
{
    // Must match the sizeof(ProgressBin) written by ProgressStore.cpp.
    TEST_ASSERT_EQUAL_UINT32(sizeof(ProgressBin), 12u);
}

void test_progress_bin_roundtrip()
{
    ProgressBin orig;
    orig.chapterIndex = 3;
    orig.pageOffset = 4200;
    orig.timestamp = 1747000000u;

    // Serialize like ProgressStore does.
    uint8_t buf[sizeof(ProgressBin)];
    memcpy(buf, &orig, sizeof(ProgressBin));

    // Deserialize.
    ProgressBin restored;
    memcpy(&restored, buf, sizeof(ProgressBin));

    TEST_ASSERT_EQUAL_INT32(orig.chapterIndex, restored.chapterIndex);
    TEST_ASSERT_EQUAL_INT32(orig.pageOffset, restored.pageOffset);
    TEST_ASSERT_EQUAL_UINT32(orig.timestamp, restored.timestamp);
}

void test_progress_bin_zero_values()
{
    ProgressBin orig = {0, 0, 0};
    uint8_t buf[sizeof(ProgressBin)];
    memcpy(buf, &orig, sizeof(ProgressBin));
    ProgressBin restored;
    memcpy(&restored, buf, sizeof(ProgressBin));

    TEST_ASSERT_EQUAL_INT32(0, restored.chapterIndex);
    TEST_ASSERT_EQUAL_INT32(0, restored.pageOffset);
    TEST_ASSERT_EQUAL_UINT32(0, restored.timestamp);
}

void test_progress_bin_max_values()
{
    ProgressBin orig;
    orig.chapterIndex = 2147483647;
    orig.pageOffset = 2147483647;
    orig.timestamp = 0xFFFFFFFFu;

    uint8_t buf[sizeof(ProgressBin)];
    memcpy(buf, &orig, sizeof(ProgressBin));
    ProgressBin restored;
    memcpy(&restored, buf, sizeof(ProgressBin));

    TEST_ASSERT_EQUAL_INT32(orig.chapterIndex, restored.chapterIndex);
    TEST_ASSERT_EQUAL_INT32(orig.pageOffset, restored.pageOffset);
    TEST_ASSERT_EQUAL_UINT32(orig.timestamp, restored.timestamp);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_progress_bin_size);
    RUN_TEST(test_progress_bin_roundtrip);
    RUN_TEST(test_progress_bin_zero_values);
    RUN_TEST(test_progress_bin_max_values);
    exit(UNITY_END());
}

void loop() {}
