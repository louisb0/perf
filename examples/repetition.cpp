#include "../perf.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

std::string read_iterator_reserved(const fs::path &path) {
    std::ifstream file(path);
    std::string str;

    file.seekg(0, std::ios::end);
    str.reserve(file.tellg());
    file.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return str;
}

std::string read_stringstream(const fs::path &path) {
    std::ifstream file(path);
    std::stringstream buffer;

    buffer << file.rdbuf();

    return buffer.str();
}

std::string read_direct(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    std::string str;

    file.seekg(0, std::ios::end);
    str.resize(file.tellg());
    file.seekg(0, std::ios::beg);

    file.read(str.data(), str.size());

    return str;
}

void test_file_reads(const fs::path &path) {
    uintmax_t file_size = fs::file_size(path);

    {
        printf("\n--- Testing reserved string iterator-based reading ---\n");
        perf::repetition::tester tester;
        tester.start_test_wave(file_size, 10);

        while (tester.is_testing()) {
            tester.begin_time();
            std::string result = read_iterator_reserved(path);
            tester.end_time();

            if (result.size() == file_size) {
                tester.count_bytes(result.size());
            }
        }
    }

    {
        printf("\n--- Testing stringstream reading ---\n");
        perf::repetition::tester tester;
        tester.start_test_wave(file_size, 10);

        while (tester.is_testing()) {
            tester.begin_time();
            std::string result = read_stringstream(path);
            tester.end_time();

            if (result.size() == file_size) {
                tester.count_bytes(result.size());
            }
        }
    }

    {
        printf("\n--- Testing direct reading ---\n");
        perf::repetition::tester tester;
        tester.start_test_wave(file_size, 10);

        while (tester.is_testing()) {
            tester.begin_time();
            std::string result = read_direct(path);
            tester.end_time();

            if (result.size() == file_size) {
                tester.count_bytes(result.size());
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }

    fs::path path = argv[1];
    if (!fs::exists(path)) {
        std::cerr << "File doesn't exist: " << path << '\n';
        return 1;
    }

    std::cout << "Testing file: " << path << " (" << fs::file_size(path) << " bytes)\n";

    test_file_reads(path);

    return 0;
}
