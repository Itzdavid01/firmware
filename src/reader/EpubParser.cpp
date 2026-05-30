#include "EpubParser.h"
#include "ChapterCache.h"
#include "main.h"
#include "miniz_zip.h"
#include <SD.h>

namespace reader
{

bool EpubParser::open(const std::string &path)
{
    LOG_INFO("EpubParser: Opening %s", path.c_str());
    bookPath = path;

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_file(&zip_archive, path.c_str(), 0)) {
        LOG_ERROR("mz_zip_reader_init_file() failed!");
        return false;
    }

    size_t file_size;
    void *p = mz_zip_reader_extract_file_to_heap(&zip_archive, "META-INF/container.xml", &file_size, 0);
    if (!p) {
        LOG_ERROR("Failed to find META-INF/container.xml");
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    std::string container_xml((char *)p, file_size);
    free(p);

    size_t pos = container_xml.find("full-path=\"");
    if (pos == std::string::npos) {
        LOG_ERROR("Malformed container.xml");
        mz_zip_reader_end(&zip_archive);
        return false;
    }
    pos += 11;
    size_t end_pos = container_xml.find("\"", pos);
    std::string opf_path = container_xml.substr(pos, end_pos - pos);

    LOG_INFO("Found OPF: %s", opf_path.c_str());

    size_t last_slash = opf_path.find_last_of("/");
    opfFolder = (last_slash == std::string::npos) ? "" : opf_path.substr(0, last_slash + 1);

    p = mz_zip_reader_extract_file_to_heap(&zip_archive, opf_path.c_str(), &file_size, 0);
    if (!p) {
        LOG_ERROR("Failed to extract OPF file");
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    std::string opf_content((char *)p, file_size);
    free(p);

    std::vector<std::pair<std::string, std::string>> manifest;
    size_t item_pos = 0;
    while ((item_pos = opf_content.find("<item ", item_pos)) != std::string::npos) {
        size_t id_pos = opf_content.find("id=\"", item_pos);
        size_t href_pos = opf_content.find("href=\"", item_pos);
        if (id_pos != std::string::npos && href_pos != std::string::npos) {
            id_pos += 4;
            size_t end_id = opf_content.find("\"", id_pos);
            std::string id = opf_content.substr(id_pos, end_id - id_pos);

            href_pos += 6;
            size_t end_href = opf_content.find("\"", href_pos);
            std::string href = opf_content.substr(href_pos, end_href - href_pos);
            manifest.push_back({id, href});
        }
        item_pos += 1;
    }

    chapterPaths.clear();
    size_t spine_pos = opf_content.find("<spine");
    if (spine_pos != std::string::npos) {
        size_t itemref_pos = spine_pos;
        while ((itemref_pos = opf_content.find("<itemref ", itemref_pos)) != std::string::npos) {
            size_t idref_pos = opf_content.find("idref=\"", itemref_pos);
            if (idref_pos != std::string::npos) {
                idref_pos += 7;
                size_t end_idref = opf_content.find("\"", idref_pos);
                std::string idref = opf_content.substr(idref_pos, end_idref - idref_pos);

                for (const auto &item : manifest) {
                    if (item.first == idref) {
                        chapterPaths.push_back(opfFolder + item.second);
                        break;
                    }
                }
            }
            itemref_pos += 1;
            if (itemref_pos > opf_content.find("</spine>", spine_pos))
                break;
        }
    }

    if (chapterPaths.empty()) {
        for (const auto &item : manifest) {
            chapterPaths.push_back(opfFolder + item.second);
        }
    }

    LOG_INFO("Parsed %u chapters in spine order", chapterPaths.size());

    mz_zip_reader_end(&zip_archive);
    return !chapterPaths.empty();
}

std::string EpubParser::getChapter(int index)
{
    if (index < 0 || index >= (int)chapterPaths.size())
        return "";

    std::string cached = getChapterFromCache(index);
    if (!cached.empty()) {
        LOG_INFO("EpubParser: cache hit chapter %d", index);
        return cached;
    }

    LOG_INFO("EpubParser: cache miss chapter %d, extracting from zip", index);
    std::string content = getChapterFromZip(index);
    if (!content.empty()) {
        writeChapterToCache(index, content);
    }
    return content;
}

std::string EpubParser::getChapterFromCache(int index)
{
    std::string path = CacheKey::sectionPath(bookPath, index);
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) {
        return "";
    }
    std::string content;
    while (f.available()) {
        char buf[256];
        int rb = f.read((uint8_t *)buf, sizeof(buf));
        if (rb <= 0)
            break;
        content.append(buf, rb);
    }
    f.close();
    return content;
}

std::string EpubParser::getChapterFromZip(int index)
{
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_reader_init_file(&zip_archive, bookPath.c_str(), 0))
        return "";

    size_t file_size;
    void *p = mz_zip_reader_extract_file_to_heap(&zip_archive, chapterPaths[index].c_str(), &file_size, 0);
    mz_zip_reader_end(&zip_archive);

    if (!p)
        return "";
    std::string content((char *)p, file_size);
    free(p);
    return content;
}

void EpubParser::writeChapterToCache(int index, const std::string &content)
{
    std::string cacheRoot = CacheKey::ensureCacheRootForEpub(bookPath);
    if (cacheRoot.empty()) {
        LOG_WARN("EpubParser: could not create cache root, skipping cache write");
        return;
    }
    std::string path = CacheKey::sectionPath(bookPath, index);
    File f = SD.open(path.c_str(), FILE_WRITE);
    if (!f) {
        LOG_WARN("EpubParser: could not open %s for write", path.c_str());
        return;
    }
    size_t wb = f.write((const uint8_t *)content.c_str(), content.length());
    f.close();
    if (wb != content.length()) {
        LOG_WARN("EpubParser: incomplete write to %s", path.c_str());
    }
}

void EpubParser::invalidateChapterCache()
{
    std::string root = CacheKey::ensureCacheRootForEpub(bookPath);
    if (root.empty())
        return;
    std::string sectionsDir = root + "/sections";
    File rootDir = SD.open(sectionsDir.c_str());
    if (!rootDir || !rootDir.isDirectory())
        return;

    File f;
    while ((f = rootDir.openNextFile())) {
        std::string fname = f.name();
        f.close();
        SD.remove(fname.c_str());
    }
    rootDir.close();
}

bool EpubParser::isSdCardPresent() const
{
    File f = SD.open("/", FILE_READ); // Arduino SD prepends its "/sd" mountpoint
    if (!f)
        return false;
    bool isDir = f.isDirectory();
    f.close();
    return isDir;
}

int EpubParser::getChapterCount() const
{
    return (int)chapterPaths.size();
}

} // namespace reader
