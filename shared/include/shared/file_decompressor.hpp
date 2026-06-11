#pragma once

#include <string>

class Decompressor {
public:
    Decompressor() = default;
    ~Decompressor() = default;

    static bool detectFormat(const std::string& filename, std::string& format);

    static bool decompressBzip2(
        const std::string& inputPath,
        const std::string& outputPath,
        std::string& errorMsg
    );

    static bool decompressGzip(
        const std::string& inputPath,
        const std::string& outputPath,
        std::string& errorMsg
    );

    static bool decompress(
        const std::string& inputPath,
        const std::string& outputDir,
        const std::string& filename,
        std::string& errorMsg
    );

private:
    static constexpr size_t BUFFER_SIZE = 65536;
};

