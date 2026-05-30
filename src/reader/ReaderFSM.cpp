#include "ReaderFSM.h"
#include "gps/RTC.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace reader
{

namespace
{
// Case-insensitive check whether `kw` (lowercase) matches s at pos.
bool matchKw(const std::string &s, size_t pos, const char *kw)
{
    for (size_t k = 0; kw[k]; ++k) {
        if (pos + k >= s.size())
            return false;
        char c = s[pos + k];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + 32);
        if (c != kw[k])
            return false;
    }
    return true;
}

// Convert a chapter's (X)HTML into readable plain text:
//  - skips <head>/<style>/<script> blocks and comments entirely
//  - turns block-level tags into newlines, drops all other tags
//  - decodes the common HTML entities, collapses runs of whitespace
std::string htmlToText(const std::string &html)
{
    std::string out;
    out.reserve(html.size());
    size_t i = 0, n = html.size();
    bool lastSpace = true; // collapse leading whitespace

    auto findCloseCI = [&](size_t from, const char *closeLower) -> size_t {
        for (size_t p = from; p < n; ++p)
            if (matchKw(html, p, closeLower))
                return p;
        return std::string::npos;
    };

    while (i < n) {
        char ch = html[i];
        if (ch == '<') {
            if (matchKw(html, i, "<!--")) {
                size_t e = html.find("-->", i + 4);
                i = (e == std::string::npos) ? n : e + 3;
                continue;
            }
            static const char *openTags[] = {"<style", "<script", "<head"};
            static const char *closeTags[] = {"</style>", "</script>", "</head>"};
            bool skipped = false;
            for (int b = 0; b < 3; ++b) {
                if (matchKw(html, i, openTags[b])) {
                    size_t e = findCloseCI(i, closeTags[b]);
                    i = (e == std::string::npos) ? n : e + strlen(closeTags[b]);
                    skipped = true;
                    break;
                }
            }
            if (skipped)
                continue;
            bool brk = matchKw(html, i, "</p") || matchKw(html, i, "<br") || matchKw(html, i, "</div") ||
                       matchKw(html, i, "</h1") || matchKw(html, i, "</h2") || matchKw(html, i, "</h3") ||
                       matchKw(html, i, "</li") || matchKw(html, i, "</tr");
            size_t e = html.find('>', i);
            i = (e == std::string::npos) ? n : e + 1;
            if (brk && !out.empty() && out.back() != '\n') {
                out.push_back('\n');
                lastSpace = true;
            }
            continue;
        }
        if (ch == '&') {
            size_t sc = html.find(';', i);
            if (sc != std::string::npos && sc - i <= 10) {
                std::string ent = html.substr(i + 1, sc - i - 1);
                std::string rep;
                if (ent == "amp")
                    rep = "&";
                else if (ent == "lt")
                    rep = "<";
                else if (ent == "gt")
                    rep = ">";
                else if (ent == "quot")
                    rep = "\"";
                else if (ent == "apos")
                    rep = "'";
                else if (ent == "nbsp")
                    rep = " ";
                else if (!ent.empty() && ent[0] == '#') {
                    long cp = (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')) ? strtol(ent.c_str() + 2, nullptr, 16)
                                                                                   : strtol(ent.c_str() + 1, nullptr, 10);
                    if (cp == 8217 || cp == 8216 || cp == 39)
                        rep = "'";
                    else if (cp == 8220 || cp == 8221)
                        rep = "\"";
                    else if (cp == 8211 || cp == 8212)
                        rep = "-";
                    else if (cp == 8230)
                        rep = "...";
                    else if (cp > 0 && cp < 128)
                        rep = std::string(1, (char)cp);
                }
                out += rep;
                i = sc + 1;
                lastSpace = (rep == " ");
                continue;
            }
        }
        // Normalize common non-ASCII UTF-8 punctuation to ASCII, since the e-paper
        // font has no glyphs for them (otherwise they render as garbage).
        unsigned char uc = (unsigned char)ch;
        if (uc >= 0x80) {
            if (uc == 0xE2 && i + 2 < n && (unsigned char)html[i + 1] == 0x80) {
                unsigned char c3 = (unsigned char)html[i + 2];
                const char *rep = nullptr;
                if (c3 == 0x98 || c3 == 0x99)
                    rep = "'"; // ‘ ’
                else if (c3 == 0x9C || c3 == 0x9D)
                    rep = "\""; // “ ”
                else if (c3 == 0x93 || c3 == 0x94)
                    rep = "-"; // – —
                else if (c3 == 0xA6)
                    rep = "..."; // …
                if (rep) {
                    out += rep;
                    i += 3;
                    lastSpace = false;
                    continue;
                }
            }
            if (uc == 0xC2 && i + 1 < n && (unsigned char)html[i + 1] == 0xA0) { // nbsp
                i += 2;
                if (!lastSpace) {
                    out.push_back(' ');
                    lastSpace = true;
                }
                continue;
            }
            // Unknown high byte: pass through (font may map it via its own table).
            out.push_back(ch);
            ++i;
            lastSpace = false;
            continue;
        }

        ++i;
        if (ch == '\r')
            continue;
        if (ch == '\n' || ch == '\t')
            ch = ' ';
        if (ch == ' ') {
            if (!lastSpace) {
                out.push_back(' ');
                lastSpace = true;
            }
        } else {
            out.push_back(ch);
            lastSpace = false;
        }
    }
    return out;
}
} // namespace

ReaderController::ReaderController() = default;

ReaderController &ReaderController::instance()
{
    static ReaderController inst;
    return inst;
}

// ── Book list ─────────────────────────────────────────

void ReaderController::scanForBooks()
{
    bookFiles.clear();
    selectedBookIndex = 0;
#ifdef HAS_SDCARD
    DIR *dir = opendir("/sd");
    if (!dir)
        return;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string filename = ent->d_name;
        if ((ent->d_type == DT_REG || ent->d_type == DT_UNKNOWN) && filename.length() > 5) {
            std::string ext = filename.substr(filename.length() - 5);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".epub") {
                bookFiles.push_back("/sd/" + filename);
            }
        }
    }
    closedir(dir);
#endif
}

const std::vector<std::string> &ReaderController::getBookFiles() const
{
    return bookFiles;
}
int ReaderController::getSelectedBookIndex() const
{
    return selectedBookIndex;
}

void ReaderController::selectNextBook()
{
    if (!bookFiles.empty())
        selectedBookIndex = (selectedBookIndex + 1) % (int)bookFiles.size();
}

void ReaderController::selectPrevBook()
{
    if (!bookFiles.empty())
        selectedBookIndex = (selectedBookIndex + (int)bookFiles.size() - 1) % (int)bookFiles.size();
}

// ── Reading ──────────────────────────────────────────

bool ReaderController::openBook(size_t index)
{
    if (index >= bookFiles.size())
        return false;
    openBookPath = bookFiles[index];
    if (!parser.open(openBookPath))
        return false;
    currentChapter = 0;
    currentPageOffset = 0;
    refreshChapterText();
    loadProgress();
    return true;
}

void ReaderController::closeBook()
{
    saveProgress();
    openBookPath.clear();
    currentChapterText.clear();
    parser.invalidateChapterCache();
}

bool ReaderController::isBookOpen() const
{
    return !openBookPath.empty();
}
const std::string &ReaderController::getOpenBookPath() const
{
    return openBookPath;
}
int ReaderController::getCurrentChapter() const
{
    return currentChapter;
}
int ReaderController::getCurrentPageOffset() const
{
    return currentPageOffset;
}
const std::string &ReaderController::getCurrentChapterText() const
{
    return currentChapterText;
}
int ReaderController::getChapterCount() const
{
    return parser.getChapterCount();
}

void ReaderController::refreshChapterText()
{
    if (!openBookPath.empty())
        currentChapterText = htmlToText(parser.getChapter(currentChapter));
}

// ── Navigation ────────────────────────────────────────

void ReaderController::nextPage()
{
    if (!isBookOpen())
        return;
    int textLen = (int)currentChapterText.length();
    int step = 500;

    if (currentPageOffset + step >= textLen) {
        if (currentChapter < parser.getChapterCount() - 1) {
            currentChapter++;
            currentPageOffset = 0;
            refreshChapterText();
        } else {
            currentPageOffset = std::max(0, textLen - 100);
        }
    } else {
        int target = currentPageOffset + step;
        while (target < textLen && currentChapterText[target] != ' ' && currentChapterText[target] != '\n')
            target++;
        currentPageOffset = target;
    }
}

void ReaderController::prevPage()
{
    if (!isBookOpen())
        return;
    int step = 500;

    if (currentPageOffset <= step) {
        if (currentChapter > 0) {
            currentChapter--;
            refreshChapterText();
            currentPageOffset = std::max(0, (int)currentChapterText.length() - step);
        } else {
            currentPageOffset = 0;
        }
    } else {
        int target = currentPageOffset - step;
        while (target > 0 && currentChapterText[target - 1] != ' ' && currentChapterText[target - 1] != '\n')
            target--;
        currentPageOffset = target;
    }
}

// ── Progress persistence ─────────────────────────────────

ReaderController::Progress ReaderController::getProgress() const
{
    return {currentChapter, currentPageOffset, (uint32_t)getTime()};
}

void ReaderController::setProgress(const Progress &p)
{
    currentChapter = p.chapterIndex;
    currentPageOffset = p.pageOffset;
    if (openBookPath.empty())
        return;

    uint32_t now = millis();
    if (now - lastProgressSaveMs < PROGRESS_SAVE_DEBOUNCE_MS && p.chapterIndex == currentChapter)
        return;
    lastProgressSaveMs = now;

    saveReadingProgress(openBookPath, {p.chapterIndex, p.pageOffset, p.timestamp});
}

void ReaderController::loadProgress()
{
    if (openBookPath.empty())
        return;
    ReadingProgress p;
    if (loadReadingProgress(openBookPath, p)) {
        if (p.chapterIndex >= 0 && p.chapterIndex < parser.getChapterCount()) {
            currentChapter = p.chapterIndex;
            refreshChapterText();
            // Clamp restored offset to the current (HTML-stripped) chapter length so a
            // stale offset never lands past the end and shows a blank page.
            currentPageOffset = std::max<int32_t>(0, std::min<int32_t>(p.pageOffset, (int32_t)currentChapterText.length()));
        }
    }
}

void ReaderController::saveProgress()
{
    if (openBookPath.empty())
        return;
    auto p = getProgress();
    saveReadingProgress(openBookPath, {p.chapterIndex, p.pageOffset, p.timestamp});
}

} // namespace reader