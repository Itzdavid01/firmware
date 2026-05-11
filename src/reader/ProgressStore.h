#pragma once

#include <cstdint>
#include <string>

namespace reader
{

struct ReadingProgress {
    int32_t chapterIndex = 0;
    int32_t pageOffset = 0;
    uint32_t timestamp = 0;
};

bool loadReadingProgress(const std::string &epubPath, ReadingProgress &out);
bool saveReadingProgress(const std::string &epubPath, const ReadingProgress &progress);

} // namespace reader
