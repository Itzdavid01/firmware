// Cache key and path derivation for SD chapter cache.
// https://github.com/crosspoint-reader/crosspoint-reader

#pragma once

#include <string>

namespace reader
{

class CacheKey
{
  public:
    // Returns the cache root directory for a given EPUB filepath.
    static std::string ensureCacheRootForEpub(const std::string &epubPath);

    static std::string sectionPath(const std::string &epubPath, int chapterIndex);
    static std::string progressPath(const std::string &epubPath);
    static std::string bookCachePath(const std::string &epubPath);

  private:
    static uint32_t hashEpubPath(const std::string &epubPath);
};

} // namespace reader
