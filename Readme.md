# Virus Scanner
A multithreaded C++ tool to detect ELF binaries infected by a specific byte-pattern (signature).  
It scans a directory recursively, identifies ELF files, and checks each one for the presence of the given signature.

## How It Works
- **Pattern Matching:** Uses the **[Boyer-Moore](https://en.cppreference.com/w/cpp/utility/functional/boyer_moore_searcher)** string search algorithm, for fast and efficient pattern detection. This algorythm is included in the c++ standard library. It is especially well-suited for:
    - **Large file sizes**, where skipping ahead in the search reduces unnecessary comparisons.
    - **Repeated searches**, since the pattern is preprocessed once and reused across all files.

- **Concurrency:** Scanning is parallelized using a **thread pool**, improving performance on I/O-heavy operations like file reading.

## Features
- Scans all ELF files in a directory tree
- Detects infections by searching for a signature pattern
- Reports infected files via standard output
- Uses a 4 KB read buffer and 4 threads by default.

## Output
For every infected file found, the program prints:
`File <path> is infected!`

## Compilation
`g++ -std=c++20 main.cpp PatternScanner.cpp threadpool.c -o find_sig`

## Usage
`./find_sig <directory_path> <signature_path>`

`<directory_path>`: Path to the root directory from which the search should be started

`<signature_path>`: Path to the file which contains the detected signature as a raw data

### Running Example
`./find_sig test_samples test_samples/crypty.sig`

Sample test files are provided in the test_samples/ directory.

## Architecture Notes

- **Thread Pool:** Implemented in C and originally written by me for a systems programming course. It is reused here to parallelize I/O-heavy file scanning. While the rest of the project is written in C++, this thread pool is kept in C to reflect my original work.
- **Boyer-Moore Algorithm:** Chosen for its excellent performance in repeated pattern searches across large files. It preprocesses the search pattern once and reuses it, making it ideal for scanning many files with the same signature.

## Possible Improvements
* Rewrite the thread pool in modern C++.
* Add logging or verbose output for easier debugging.
* Make buffer size and thread count configurable via command-line flags
