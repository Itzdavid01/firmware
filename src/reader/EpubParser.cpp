#include "EpubParser.h"
#include "main.h"
#include "miniz_zip.h"

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

    // EPUBs have a META-INF/container.xml that points to the .opf file
    size_t file_size;
    void *p = mz_zip_reader_extract_file_to_heap(&zip_archive, "META-INF/container.xml", &file_size, 0);
    if (!p) {
        LOG_ERROR("Failed to find META-INF/container.xml");
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    std::string container_xml((char *)p, file_size);
    free(p);

    // Simple search for full-path (proper XML parsing is heavy)
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

    // Store folder for resolving relative paths
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

    // 1. Build an ID-to-Path map from the manifest
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

    // 2. Parse the spine to get the correct reading order
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

                // Find this ID in our manifest
                for (const auto &item : manifest) {
                    if (item.first == idref) {
                        chapterPaths.push_back(opfFolder + item.second);
                        break;
                    }
                }
            }
            itemref_pos += 1;
            // Stop if we hit the end of the spine
            if (itemref_pos > opf_content.find("</spine>", spine_pos))
                break;
        }
    }

    // Fallback to manifest order if spine parsing failed
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

int EpubParser::getChapterCount() const
{
    return (int)chapterPaths.size();
}

} // namespace reader
