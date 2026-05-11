# T5S3 E-Reader Future Improvements Roadmap

## Objective

This document tracks the implementation of major performance and usability improvements for the T5S3 E-Reader POC.

## 1. Font Size & UI Scaling (COMPLETED)

- **Problem:** The default `ArialMT_Plain_10` font is far too small for comfortable reading on a 960x540 panel.
- **Implementation:** Increased the default reading font to `ArialMT_Plain_16`.

## 2. EPUB "Spine" Parsing (COMPLETED)

- **Problem:** The `EpubParser` previously extracted chapters in the arbitrary order they appeared in the EPUB `<manifest>`.
- **Implementation:** Implemented parsing of the `<spine>` tag in the `.opf` file to determine the correct reading order.

## 3. Smart Pagination

- **Problem:** "Next Page" blindly jumps exactly 500 characters, leading to overlapping text or skipped paragraphs due to dynamic word wrapping.
- **Implementation:** Calculate the exact number of characters that fit on the screen in `DocumentRenderer`. Iterate through words, measuring width with `display->getStringWidth()` and tracking line counts up to `EPD_HEIGHT`.

## 4. Progress Saving (COMPLETED)

- **Problem:** Exiting the app or rebooting loses the current book, chapter, and page offset.
- **Implementation:** `ReadingProgress` struct (chapterIndex, pageOffset, timestamp) persisted to SD card via `ProgressStore`. Cache root: `/sd/.crosspoint/epub_<FNV-1a-hash>/progress.bin`. Progress auto-saved on book close and debounced (2 s) during reading; restored on book open. Cache-first chapter loading via `ChapterCache`. See `docs/T5S3_CROSSPOINT_INSPIRED_PLAN.md` for design details.

## 5. Memory-Safe Chunking

- **Problem:** `miniz` extracts the entire chapter file into RAM at once. A massive 1MB monolithic HTML file would cause an Out-Of-Memory (OOM) crash on the ESP32.
- **Implementation:** Implement chunked extraction where only the portion of the file currently being rendered is decompressed using low-level streaming `miniz` APIs instead of `mz_zip_reader_extract_file_to_heap`.
