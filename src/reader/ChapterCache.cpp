#include "ChapterCache.h"
#include "main.h"
#include <SD.h>

namespace reader
{

std::string CacheKey::ensureCacheRootForEpub(const std::string &epubPath)
{
    uint32_t h = hashEpubPath(epubPath);
    char buf[64];
    snprintf(buf, sizeof(buf), "/.crosspoint/epub_%08x", h);
    std::string root(buf);

    // SD/FAT mkdir does not create intermediate directories, so create the parent
    // ".crosspoint" first — otherwise the epub_* dir (and all cache/progress writes
    // inside it) silently fail and reading progress is never persisted.
    SD.mkdir("/.crosspoint");
    SD.mkdir(root.c_str());
    SD.mkdir((root + "/sections").c_str());

    return root;
}

std::string CacheKey::sectionPath(const std::string &epubPath, int chapterIndex)
{
    uint32_t h = hashEpubPath(epubPath);
    char buf[64];
    snprintf(buf, sizeof(buf), "/.crosspoint/epub_%08x/sections/%d.bin", h, chapterIndex);
    return std::string(buf);
}

std::string CacheKey::progressPath(const std::string &epubPath)
{
    uint32_t h = hashEpubPath(epubPath);
    char buf[64];
    snprintf(buf, sizeof(buf), "/.crosspoint/epub_%08x/progress.bin", h);
    return std::string(buf);
}

std::string CacheKey::bookCachePath(const std::string &epubPath)
{
    uint32_t h = hashEpubPath(epubPath);
    char buf[64];
    snprintf(buf, sizeof(buf), "/.crosspoint/epub_%08x/book.bin", h);
    return std::string(buf);
}

uint32_t CacheKey::hashEpubPath(const std::string &epubPath)
{
    // Fowler-Noll-Vo FNV-1a 32-bit hash — same principle as CrossPoint's
    // std::hash<std::string> is implementation-defined; FNV is deterministic.
    uint32_t hash = 2166136261u;
    for (unsigned char c : epubPath) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

} // namespace reader
