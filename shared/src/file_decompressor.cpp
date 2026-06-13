#include "shared/file_decompressor.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <bzlib.h>
#include <zlib.h>
#include <archive.h>
#include <archive_entry.h>
#include <filesystem>

bool Decompressor::detectFormat(const std::string& filename, std::string& format) {
    std::string lower;
    lower.reserve(filename.size());
    for (char c : filename) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lower.find(".tar.bz2") != std::string::npos ||
        lower.find(".tbz2") != std::string::npos ||
        lower.find(".tbz") != std::string::npos) {
        format = "tarbzip2";
        return true;
    }
    if (lower.find(".tar.gz") != std::string::npos ||
        lower.find(".tgz") != std::string::npos) {
        format = "targzip";
        return true;
    }
    if (lower.find(".bz2") != std::string::npos) {
        format = "bzip2";
        return true;
    }
    if (lower.find(".gz") != std::string::npos) {
        format = "gzip";
        return true;
    }
    format = "unknown";
    return false;
}

bool Decompressor::decompressBzip2(
    const std::string& inputPath,
    const std::string& outputPath,
    std::string& errorMsg
) {
    try {
        FILE* inFile = fopen(inputPath.c_str(), "rb");
        if (!inFile) {
            errorMsg = "Cannot open input file: " + inputPath;
            return false;
        }

        FILE* outFile = fopen(outputPath.c_str(), "wb");
        if (!outFile) {
            fclose(inFile);
            errorMsg = "Cannot create output file: " + outputPath;
            return false;
        }

        unsigned int verbosity = 0;
        int blockSize = 9;
        int workFactor = 30;

        BZFILE* bzFile = BZ2_bzReadOpen(NULL, inFile, verbosity, 0, NULL, 0);
        if (!bzFile) {
            fclose(inFile);
            fclose(outFile);
            errorMsg = "Failed to initialize bzip2 decompression";
            return false;
        }

        unsigned char buffer[BUFFER_SIZE];
        int bzerror = BZ_OK;

        while (bzerror == BZ_OK) {
            int bytesRead = BZ2_bzRead(&bzerror, bzFile, buffer, BUFFER_SIZE);

            if (bytesRead > 0) {
                size_t written = fwrite(buffer, 1, bytesRead, outFile);
                if (written != static_cast<size_t>(bytesRead)) {
                    errorMsg = "Error writing to output file";
                    BZ2_bzReadClose(NULL, bzFile);
                    fclose(inFile);
                    fclose(outFile);
                    return false;
                }
            }
        }

        if (bzerror != BZ_STREAM_END) {
            errorMsg = "Bzip2 decompression error: " + std::to_string(bzerror);
            BZ2_bzReadClose(NULL, bzFile);
            fclose(inFile);
            fclose(outFile);
            return false;
        }

        BZ2_bzReadClose(NULL, bzFile);
        fclose(inFile);
        fclose(outFile);

        std::cout << "Successfully decompressed bzip2: " << inputPath << " -> " << outputPath << std::endl;
        return true;
    } catch (const std::exception& e) {
        errorMsg = "Exception during bzip2 decompression: " + std::string(e.what());
        return false;
    }
}

bool Decompressor::decompressGzip(
    const std::string& inputPath,
    const std::string& outputPath,
    std::string& errorMsg
) {
    try {
        gzFile inFile = gzopen(inputPath.c_str(), "rb");
        if (!inFile) {
            errorMsg = "Cannot open gzip file: " + inputPath;
            return false;
        }

        FILE* outFile = fopen(outputPath.c_str(), "wb");
        if (!outFile) {
            gzclose(inFile);
            errorMsg = "Cannot create output file: " + outputPath;
            return false;
        }

        unsigned char buffer[BUFFER_SIZE];
        int bytesRead;

        while ((bytesRead = gzread(inFile, buffer, BUFFER_SIZE)) > 0) {
            size_t written = fwrite(buffer, 1, bytesRead, outFile);
            if (written != static_cast<size_t>(bytesRead)) {
                errorMsg = "Error writing to output file";
                gzclose(inFile);
                fclose(outFile);
                return false;
            }
        }

        if (bytesRead < 0) {
            int errnum = 0;
            const char* errstr = gzerror(inFile, &errnum);
            errorMsg = "Gzip decompression error: " + std::string(errstr);
            gzclose(inFile);
            fclose(outFile);
            return false;
        }

        gzclose(inFile);
        fclose(outFile);

        std::cout << "Successfully decompressed gzip: " << inputPath << " -> " << outputPath << std::endl;
        return true;
    } catch (const std::exception& e) {
        errorMsg = "Exception during gzip decompression: " + std::string(e.what());
        return false;
    }
}

bool Decompressor::extractTar(
    const std::string& tarPath,
    const std::string& outputDir,
    std::string& errorMsg
) {
    try {
        struct archive* a = archive_read_new();
        archive_read_support_format_tar(a);
        archive_read_support_format_gnutar(a);

        int r = archive_read_open_filename(a, tarPath.c_str(), BUFFER_SIZE);
        if (r != ARCHIVE_OK) {
            errorMsg = "Cannot open tar file: " + std::string(archive_error_string(a));
            archive_read_free(a);
            return false;
        }

        struct archive_entry* entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            const char* entryName = archive_entry_pathname(entry);
            std::string destPath = outputDir + "/" + entryName;

            if (archive_entry_filetype(entry) == AE_IFDIR) {
                std::filesystem::create_directories(destPath);
                continue;
            }

            std::filesystem::path parentDir = std::filesystem::path(destPath).parent_path();
            std::filesystem::create_directories(parentDir);

            if (archive_entry_filetype(entry) == AE_IFREG) {
                FILE* outFile = fopen(destPath.c_str(), "wb");
                if (!outFile) {
                    errorMsg = "Cannot create file: " + destPath;
                    archive_read_close(a);
                    archive_read_free(a);
                    return false;
                }

                char buf[BUFFER_SIZE];
                la_ssize_t bytesRead;
                while ((bytesRead = archive_read_data(a, buf, BUFFER_SIZE)) > 0) {
                    fwrite(buf, 1, bytesRead, outFile);
                }
                fclose(outFile);

                if (bytesRead < 0) {
                    errorMsg = "Error reading tar data for: " + destPath;
                    archive_read_close(a);
                    archive_read_free(a);
                    return false;
                }

                std::filesystem::perms perms = static_cast<std::filesystem::perms>(archive_entry_mode(entry) & 0777);
                std::filesystem::permissions(destPath, perms, std::filesystem::perm_options::replace);
            } else if (archive_entry_filetype(entry) == AE_IFLNK) {
                const char* linkTarget = archive_entry_symlink(entry);
                std::filesystem::remove(destPath);
                std::filesystem::create_symlink(linkTarget, destPath);
            }
        }

        archive_read_close(a);
        archive_read_free(a);
        return true;
    } catch (const std::exception& e) {
        errorMsg = "Exception during tar extraction: " + std::string(e.what());
        return false;
    }
}

bool Decompressor::isTarArchive(const std::string& path) {
    struct archive* a = archive_read_new();
    archive_read_support_format_tar(a);
    archive_read_support_format_gnutar(a);
    int r = archive_read_open_filename(a, path.c_str(), BUFFER_SIZE);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }
    struct archive_entry* entry;
    r = archive_read_next_header(a, &entry);
    archive_read_close(a);
    archive_read_free(a);
    return r == ARCHIVE_OK;
}

bool Decompressor::decompressAndExtractTar(
    const std::string& inputPath,
    const std::string& outputDir,
    const std::string& filename,
    std::string& errorMsg
) {
    std::string format;
    if (!detectFormat(filename, format)) {
        errorMsg = "Unknown format for: " + filename;
        return false;
    }

    bool isTarBzip2 = (format == "tarbzip2");
    bool isTarGzip = (format == "targzip");

    if (!isTarBzip2 && !isTarGzip) {
        errorMsg = "Not a tar archive format: " + format;
        return false;
    }

    std::string tarPath = outputDir + "/" + "_temp_extract_" + std::filesystem::path(filename).stem().string() + ".tar";

    bool decompressOk = false;
    if (isTarBzip2) {
        decompressOk = decompressBzip2(inputPath, tarPath, errorMsg);
    } else {
        decompressOk = decompressGzip(inputPath, tarPath, errorMsg);
    }

    if (!decompressOk) {
        std::filesystem::remove(tarPath);
        return false;
    }

    bool extractOk = extractTar(tarPath, outputDir, errorMsg);

    std::filesystem::remove(tarPath);

    if (!extractOk) {
        return false;
    }

    std::cout << "Successfully extracted tar archive: " << inputPath << " -> " << outputDir << "/" << std::endl;
    return true;
}

bool Decompressor::decompress(
    const std::string& inputPath,
    const std::string& outputDir,
    const std::string& filename,
    std::string& errorMsg
) {
    std::string format;
    if (!detectFormat(filename, format)) {
        errorMsg = "Unknown compression format for: " + filename;
        return false;
    }

    std::string baseName = filename;
    if (format == "bzip2") {
        if (baseName.length() > 4) {
            baseName = baseName.substr(0, baseName.length() - 4);
        }
    } else if (format == "gzip") {
        if (baseName.length() > 3) {
            baseName = baseName.substr(0, baseName.length() - 3);
        }
    } else if (format == "tarbzip2") {
        if (baseName.length() > 8) {
            baseName = baseName.substr(0, baseName.length() - 8);
        }
    } else if (format == "targzip") {
        if (baseName.length() > 7) {
            baseName = baseName.substr(0, baseName.length() - 7);
        }
    }

    std::string outputPath = outputDir + "/" + baseName;
    std::string tempPath = outputPath + ".tmp";

    std::cout << "Decompressing " << format << ": " << filename << std::endl;

    bool decompressOk = false;
    if (format == "bzip2" || format == "tarbzip2") {
        decompressOk = decompressBzip2(inputPath, tempPath, errorMsg);
    } else if (format == "gzip" || format == "targzip") {
        decompressOk = decompressGzip(inputPath, tempPath, errorMsg);
    } else {
        errorMsg = "Unsupported compression format: " + format;
        return false;
    }

    if (!decompressOk) {
        std::filesystem::remove(tempPath);
        return false;
    }

    if (isTarArchive(tempPath)) {
        std::cout << "Detected tar archive, extracting to: " << outputDir << std::endl;
        bool extractOk = extractTar(tempPath, outputDir, errorMsg);
        std::filesystem::remove(tempPath);
        if (!extractOk) return false;
        std::cout << "Successfully extracted: " << inputPath << " -> " << outputDir << "/" << std::endl;
    } else {
        std::error_code ec;
        std::filesystem::rename(tempPath, outputPath, ec);
        if (ec) {
            errorMsg = "Cannot rename temp file: " + ec.message();
            std::filesystem::remove(tempPath);
            return false;
        }
        std::cout << "Successfully decompressed: " << inputPath << " -> " << outputPath << std::endl;
    }
    return true;
}

