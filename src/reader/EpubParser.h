#pragma once

#include <string>
#include <vector>

namespace reader
{

class EpubParser
{
  public:
    EpubParser() = default;
    bool open(const std::string &path);
    std::string getChapter(int index);
    int getChapterCount() const;
    const std::string &getBookPath() const { return bookPath; }
    void invalidateChapterCache();
    bool isSdCardPresent() const;

  private:
    std::string bookPath;
    std::string opfFolder;
    std::vector<std::string> chapterPaths;
    std::string getChapterFromCache(int index);
    std::string getChapterFromZip(int index);
    void writeChapterToCache(int index, const std::string &content);
};

} // namespace reader
