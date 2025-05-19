#ifndef PATTERN_SCANNER_H
#define PATTERN_SCANNER_H

#include <string>
#include <filesystem>
#include <vector>
#include <fstream>
#include <functional> // for boyer-moore searcher

#define BUFFER_SIZE 4096 // 4 KB

class PatternScanner {
public:
    /**
     * Constructor initializes the scanner with a directory to scan and a pattern_ file.
     *
     * @param dir_path The root directory to scan.
     * @param pattern_path The file containing the infection pattern_.
     * @throws std::ios_base::failure if the pattern_ file cannot be opened or read.
     */
    PatternScanner(const std::filesystem::path& dir_path, const std::filesystem::path& pattern_path);

    /**
     * Scans a directory recursively for ELF files containing the infection pattern_.
     * Prints elf_infected file paths to standard output.
     *
     * @throws std::ios_base::failure if any file cannot be opened or read.
     */
    void ReportInfectedELFFiles();

/**
 * Checks if a file contains the infection pattern_ and prints its path if a match is found.
 *
 * @param file_path The path to the file to check.
 * @throws std::ios_base::failure if the file cannot be opened or read.
 */
    void ReportInfectedFile(const std::filesystem::path& file_path);

private:
    std::filesystem::path dir_path_;
    std::vector<char> pattern_;
    std::boyer_moore_searcher<std::vector<char>::const_iterator> searcher_;

    // ThreadContext containing pointers to 'this' and to the file path to process.
    struct ThreadContext{
        PatternScanner* instance;
        std::filesystem::path* path;
    };

    /**
     * Validates a directory exists
     * @param path The path to the directory.
     * @return a path object - the given path itself
     * @throws std::invalid_argument if the directory doesn't exists
     */
    static std::filesystem::path ValidateFilePath(const std::filesystem::path &path);

    /**
     * Validates a file exists.
     * @param path The path to the file.
     * @return a path object - the given path itself
     */
    static std::filesystem::path ValidateDirectoryPath(const std::filesystem::path& path);

    /**
     * Reads the entire content of a binary file into memory.
     *
     * @param file_path The path to the file to read.
     * @return A vector containing the raw binary data.
     * @throws std::ios_base::failure If the file cannot be opened or read.
     */
    static std::vector<char> ReadBinaryFile(const std::filesystem::path& file_path);

    /**
     * Checks if a file has the magic number specific to ELF files.
     *
     * @param file_path The path to the file to be checked.
     * @return `true` if the file is an ELF file, otherwise `false`.
     * @throws std::ios_base::failure if the file cannot be opened or read.
     */
    static bool IsELFFile(const std::filesystem::path& file_path) ;

    /**
     * Checks if a file stream contains the pattern_ using the boyer-moore searcher_.
     *
     * @param file The input file stream to search.
     * @return true if the pattern_ is found, false otherwise.
     */
    bool Contains(std::ifstream& file);

    /*
     * Thread entry point wrapper.
     *
     * Member functions can't be passed directly to `dispatch` because they don't match
     * the required `void* (*)(void*)` signature. This static function acts as a bridge,
     * using a `ThreadContext*` to call the actual member function.
     *
     * @param arg Pointer to a ThreadContext containing the target object and data.
    */
    static int ThreadEntry(void* arg);
};

#endif
