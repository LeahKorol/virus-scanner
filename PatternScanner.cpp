#include "PatternScanner.h"
#include "threadpool.h"

#include <iostream>
#include <array>
#include <syncstream>

// parameterized constructor to initialize variables
// Use initialisation list because searcher_ must be pre-initialised, and searcher depends on pattern_.
PatternScanner::PatternScanner(const std::filesystem::path& dir_path, const std::filesystem::path& pattern_path)
        : dir_path_(ValidateDirectoryPath(dir_path)),
          pattern_(ReadBinaryFile(ValidateFilePath(pattern_path))),
          searcher_(pattern_.begin(), pattern_.end())
{
}

std::filesystem::path PatternScanner::ValidateDirectoryPath(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
        throw std::invalid_argument("Path does not exist or is not a directory: " + path.string());
    }
    return path;
}

std::filesystem::path PatternScanner::ValidateFilePath(const std::filesystem::path& path){
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        throw std::invalid_argument("Path does not exist or is not a regular file: " + path.string());
    }
    return path;
}

std::vector<char> PatternScanner::ReadBinaryFile(const std::filesystem::path& file_path){
    std::vector<char> buffer;

    // Stream to read binary data. Seek to the end of the file for getting the file size
    std::ifstream file(file_path, std::ios::binary | std::ios::in | std::ios::ate);
    if (!file) {
        throw std::ios_base::failure("[ReadBinaryFile]: Failed to open file: " + file_path.string());
    }
    std::streampos file_size = file.tellg();

    file.seekg(0, std::ios::beg); // Move to the beginning of the file
    buffer.resize(file_size); // Allocate and initialise memory

    if (!file.read(buffer.data(), file_size)) {
        throw std::ios_base::failure("[ReadBinaryFile]: Failed to read file: " + file_path.string());
    }
    return buffer;
}

int PatternScanner::ThreadEntry(void *arg) {
    auto* ctx = static_cast<PatternScanner::ThreadContext*>(arg);
    std::filesystem::path path = (*ctx->path);
    //Call the function that reports the file
    ctx->instance->ReportInfectedFile(path);

    // Clean up after thread finishes
    delete ctx->path;
    delete ctx;
    return 0;
}

void PatternScanner::ReportInfectedELFFiles() {
    threadpool *tp = create_threadpool(4);
    if (tp == nullptr) throw std::runtime_error("[PatternScanner]: Failed to create threadpool");

    for (const auto &entry: std::filesystem::recursive_directory_iterator(dir_path_)) {
        if (std::filesystem::is_regular_file(entry) && IsELFFile(entry)) {
            // Allocate variables on heap, so the threads use an independent copy of the entry
            auto *entry_pointer = new std::filesystem::path(entry);
            auto* context = new ThreadContext;
            context->instance=this;
            context->path=entry_pointer;
            dispatch(tp, ThreadEntry, (void *) context);
        }
    }
    destroy_threadpool(tp);
}

bool PatternScanner::IsELFFile(const std::filesystem::path& file_path){
    const std::size_t num_bytes = 16;
    const std::array<char, num_bytes> magic_header = {
            0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    std::array<char, num_bytes> buffer{};

    // The stream is initialised with read-only options for binary data
    std::ifstream file(file_path, std::ios::binary | std::ios::in);
    if (!file) {
        throw std::ios_base::failure("[IsELFFile]: Failed to open file: " + file_path.string());
    }
    //read() may fail because the file is smaller than num_bytes. So check for failure using the 'eof' flag
    if (!file.read(buffer.data(), num_bytes) && !file.eof()) {
        throw std::ios_base::failure("[IsELFFile]: Failed to read file: " + file_path.string());
    }
    // The file is smaller than the header -> it isn't an ELF file
    if (file.gcount() < num_bytes) {
        return false;
    }
    return magic_header == buffer;
}

void PatternScanner::ReportInfectedFile(const std::filesystem::path& file_path){
    std::ifstream file(file_path, std::ios::in | std::ios::binary);
    if (!file) {
        throw std::ios_base::failure("[ReportInfectedFile]: Failed to open file: " + file_path.string());
    }
    if (Contains(file)) {
        {//Writes to a buffer so printing of different threads won't be mixed
            std::osyncstream (std::cout) << "File " << file_path << " is infected!" << std::endl;
        } // Emits the contents of the temporary buffer
        return;
    }
    // Check success using the 'eof' flag because read() fails if the last chunk is smaller than pattern_.size().
    if (!file.eof()) {
        throw std::ios_base::failure("[ReportInfectedFile]: Failed to read file: " + file_path.string());
    }
}

bool PatternScanner::Contains(std::ifstream& file){
    /* Look for `pattern_` in the file, even if it’s split between two chunks.
       When reading the next part of the file, keep the last (pattern_.size() - 1) bytes
       from the previous chunk to catch patterns that spam across chunks.*/

    size_t num_bytes;
    std::vector<char> buffer(BUFFER_SIZE + pattern_.size() - 1); // Extra space for overlap
    //in the first read, must read to the beginnig of the buffer to prevent false positives
    char* read_pointer = 0;

    while (file) {
        file.read(read_pointer, BUFFER_SIZE);
        std::streamsize bytes_read = file.gcount();
        num_bytes = pattern_.size() - 1 + bytes_read;

        if (bytes_read == 0) break;

        // Use the class precomputed boyer-moore searcher
        const auto it = std::search(buffer.begin(),
                                    buffer.begin() + num_bytes,
                                    searcher_);
        if (it != buffer.begin() + num_bytes) {
            return true;
        }
        // Move the last `pattern_.size() - 1` bytes to the beginning of the buffer
        std::copy(buffer.end() - (pattern_.size() - 1), buffer.end(), buffer.begin());
        //from the second read and next, read the next data to index pattern_.size()-1 to handle overlaps
        read_pointer = buffer.data() + (pattern_.size() - 1);
    }
    return false;
}
