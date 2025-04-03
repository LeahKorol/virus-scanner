#include <iostream>
#include <string>
#include <filesystem>
#include "PatternScanner.h"

/**
 * Parses and validates command-line arguments.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of argument strings.
 * @param dir_path Output parameter. Will be set to the directory path specified in the arguments.
 * @param sig_file_path Output parameter. Will be set to the signature file path specified in the arguments.
 *
 * @throws std::invalid_argument if the number of arguments is incorrect or the given paths are invalid.
 */
void InitArguments(int argc, char* argv[], std::filesystem::path& dir_path, std::filesystem::path& sig_file_path){
    if (argc != 3){
        throw std::invalid_argument("Usage: find_sig <root_dir> <sig_file>");
    }
    dir_path = argv[1];
    sig_file_path = argv[2];

    if (!std::filesystem::is_directory(dir_path)){
        throw std::invalid_argument("[InitArguments]: The root directory " + dir_path.string() + " doesn't exist");
    }
    if (!std::filesystem::is_regular_file(sig_file_path)){
        throw std::invalid_argument("[InitArguments]: The sig file " + sig_file_path.string() +" doesnt exists");
    }
}


int main(int argc, char* argv[]){
    // Expected arguments: path to the root directory and the name of the file contains the signature.
    std::filesystem::path dir_apth, file_path;
    try{
        InitArguments(argc, argv, dir_apth, file_path);
        std::cout << "Scanning started... " << std::endl;
        PatternScanner ps(dir_apth, file_path);
        ps.ReportInfectedELFFiles();
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Exception Message : " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception& e){
        std::cerr << "An expected error occurred" << std::endl;
        std::cerr << "Exception Message: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}
