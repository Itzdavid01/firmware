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

  private:
    std::string bookPath;
    std::string opfFolder;
    std::vector<std::string> chapterPaths;
};

} // namespace reader
