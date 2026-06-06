#include "file_decompressor.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <bzlib.h>
#include <zlib.h>

bool Decompressor::detectFormat(const std::string& filename, std::string& format) {
    if (filename.find(".bz2") != std::string::npos) {
        format = "bzip2";
        return true;
    }
    if (filename.find(".gz") != std::string::npos) {
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
        // Open input file
        FILE* inFile = fopen(inputPath.c_str(), "rb");
        if (!inFile) {
            errorMsg = "Cannot open input file: " + inputPath;
            return false;
        }

        // Open output file
        FILE* outFile = fopen(outputPath.c_str(), "wb");
        if (!outFile) {
            fclose(inFile);
            errorMsg = "Cannot create output file: " + outputPath;
            return false;
        }

        // Initialize bzip2
        unsigned int verbosity = 0;
        int blockSize = 9;        // 900KB
        int workFactor = 30;
        
        BZFILE* bzFile = BZ2_bzReadOpen(NULL, inFile, verbosity, 0, NULL, 0);
        if (!bzFile) {
            fclose(inFile);
            fclose(outFile);
            errorMsg = "Failed to initialize bzip2 decompression";
            return false;
        }

        // Decompress
        unsigned char buffer[BUFFER_SIZE];
        int bzerror = BZ_OK;
        
        while (bzerror == BZ_OK) {
            int bytesRead = BZ2_bzRead(&bzerror, bzFile, buffer, BUFFER_SIZE);
            
            if (bytesRead > 0) {
                size_t written = fwrite(buffer, 1, bytesRead, outFile);
                if (written != bytesRead) {
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
        // Open input file
        gzFile inFile = gzopen(inputPath.c_str(), "rb");
        if (!inFile) {
            errorMsg = "Cannot open gzip file: " + inputPath;
            return false;
        }

        // Open output file
        FILE* outFile = fopen(outputPath.c_str(), "wb");
        if (!outFile) {
            gzclose(inFile);
            errorMsg = "Cannot create output file: " + outputPath;
            return false;
        }

        // Decompress
        unsigned char buffer[BUFFER_SIZE];
        int bytesRead;

        while ((bytesRead = gzread(inFile, buffer, BUFFER_SIZE)) > 0) {
            size_t written = fwrite(buffer, 1, bytesRead, outFile);
            if (written != bytesRead) {
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

    // Output filename without extension
    std::string baseName = filename;
    if (format == "bzip2") {
        if (baseName.length() > 4) {
            baseName = baseName.substr(0, baseName.length() - 4); // Remove .bz2
        }
    } else if (format == "gzip") {
        if (baseName.length() > 3) {
            baseName = baseName.substr(0, baseName.length() - 3); // Remove .gz
        }
    }

    std::string outputPath = outputDir + "/" + baseName;

    std::cout << "Decompressing " << format << ": " << filename << " -> " << baseName << std::endl;

    if (format == "bzip2") {
        return decompressBzip2(inputPath, outputPath, errorMsg);
    } else if (format == "gzip") {
        return decompressGzip(inputPath, outputPath, errorMsg);
    }

    errorMsg = "Unsupported compression format: " + format;
    return false;
}
