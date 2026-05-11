#include "ProgressStore.h"
#include "ChapterCache.h"
#include "main.h"
#include <SD.h>

namespace reader
{

struct ProgressBin {
    int32_t chapterIndex;
    int32_t pageOffset;
    uint32_t timestamp;
};

bool loadReadingProgress(const std::string &epubPath, ReadingProgress &out)
{
    std::string path = CacheKey::progressPath(epubPath);
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) {
        return false;
    }
    ProgressBin bin;
    int rb = f.read((uint8_t *)&bin, sizeof(bin));
    f.close();
    if (rb != sizeof(bin)) {
        return false;
    }
    out.chapterIndex = bin.chapterIndex;
    out.pageOffset = bin.pageOffset;
    out.timestamp = bin.timestamp;
    return true;
}

bool saveReadingProgress(const std::string &epubPath, const ReadingProgress &progress)
{
    std::string root = CacheKey::ensureCacheRootForEpub(epubPath);
    if (root.empty()) {
        return false;
    }
    std::string path = CacheKey::progressPath(epubPath);
    File f = SD.open(path.c_str(), FILE_WRITE);
    if (!f) {
        return false;
    }
    ProgressBin bin;
    bin.chapterIndex = progress.chapterIndex;
    bin.pageOffset = progress.pageOffset;
    bin.timestamp = progress.timestamp;
    size_t wb = f.write((const uint8_t *)&bin, sizeof(bin));
    f.close();
    return wb == sizeof(bin);
}

} // namespace reader
