// Cache key and path derivation for CrossPoint-inspired SD chapter cache.
//
// Cache layout:
//   /sd/.crosspoint/epub_<hash>/          ← cache root per book
//   /sd/.crosspoint/epub_<hash>/book.bin  ← (future) spine+TOC binary cache
//   /sd/.crosspoint/epub_<hash>/progress.bin ← reading position
//   /sd/.crosspoint/epub_<hash>/sections/  ← pre-computed chapter binaries
//       /sd/.crosspoint/epub_<hash>/sections/0.bin
//       /sd/.crosspoint/epub_<hash>/sections/1.bin
//
// Cache strategy inspired by CrossPoint Reader (MIT).
// https://github.com/crosspoint-reader/crosspoint-reader

#pragma once

#include <string>

namespace reader
{

class CacheKey
{
  public:
    // Returns the cache root directory for a given EPUB filepath.
    // Creates the directory tree (/sd/.crosspoint/epub_<hash>/) on first call.
    // Returns empty string on failure.
    static std::string ensureCacheRootForEpub(const std::string &epubPath);

    // Returns the path to a cached chapter section file.
    // e.g. "/sd/.crosspoint/epub_12345678/sections/0.bin"
    static std::string sectionPath(const std::string &epubPath, int chapterIndex);

    // Returns the path to the reading progress file.
    // e.g. "/sd/.crosspoint/epub_12345678/progress.bin"
    static std::string progressPath(const std::string &epubPath);

    // Returns the path to the book metadata cache.
    // e.g. "/sd/.crosspoint/epub_12345678/book.bin"
    static std::string bookCachePath(const std::string &epubPath);

  private:
    // Compute a deterministic cache key from the EPUB path.
    static uint32_t hashEpubPath(const std::string &epubPath);
};

} // namespace reader
