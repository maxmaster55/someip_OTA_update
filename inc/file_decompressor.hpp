#pragma once

#include <string>
#include <vector>

class Decompressor {
public:
    Decompressor() = default;
    ~Decompressor() = default;

    // Detect compression format from filename
    static bool detectFormat(const std::string& filename, std::string& format);

    // Decompress bzip2 file
    static bool decompressBzip2(
        const std::string& inputPath,
        const std::string& outputPath,
        std::string& errorMsg
    );

    // Decompress gzip file
    static bool decompressGzip(
        const std::string& inputPath,
        const std::string& outputPath,
        std::string& errorMsg
    );

    // Generic decompress (auto-detects format)
    static bool decompress(
        const std::string& inputPath,
        const std::string& outputDir,
        const std::string& filename,
        std::string& errorMsg
    );

private:
    static constexpr size_t BUFFER_SIZE = 65536; // 64KB buffer
};
