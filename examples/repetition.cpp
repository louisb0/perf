#include "../perf.hpp"

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

namespace fs = std::filesystem;

std::string read_direct(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    std::string str;

    file.seekg(0, std::ios::end);
    str.resize(file.tellg());
    file.seekg(0, std::ios::beg);

    file.read(str.data(), str.size());

    return str;
}

std::string read_direct_large_pages(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    size_t file_size = fs::file_size(path);

    const size_t HUGE_PAGE_SIZE = 2 * 1024 * 1024;
    size_t aligned_size = (file_size + HUGE_PAGE_SIZE - 1) & ~(HUGE_PAGE_SIZE - 1);

    void *addr = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_HUGETLB | MAP_ANONYMOUS, -1, 0);

    file.read(static_cast<char *>(addr), file_size);

    std::string str(static_cast<char *>(addr), file_size);
    munmap(addr, aligned_size);
    return str;
}

std::string read_direct_map_populate(const fs::path &path) {
    int fd = open(path.c_str(), O_RDONLY);
    size_t file_size = fs::file_size(path);

    void *addr = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);

    std::string str(static_cast<char *>(addr), file_size);
    munmap(addr, file_size);
    close(fd);
    return str;
}

std::string read_direct_map_populate_huge_pages(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    size_t file_size = fs::file_size(path);

    const size_t HUGE_PAGE_SIZE = 2 * 1024 * 1024;
    size_t aligned_size = (file_size + HUGE_PAGE_SIZE - 1) & ~(HUGE_PAGE_SIZE - 1);

    void *addr = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_HUGETLB | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

    file.read(static_cast<char *>(addr), file_size);

    std::string str(static_cast<char *>(addr), file_size);
    munmap(addr, aligned_size);
    return str;
}

void test_file_reads(const fs::path &path) {
    uintmax_t size = fs::file_size(path);

    {
        printf("\n--- Testing direct reading ---\n");
        perf::repetition::tester tester;
        tester.start_test_wave(size, 10);

        while (tester.is_testing()) {
            tester.begin_time();
            std::string result = read_direct(path);
            tester.end_time();

            if (result.size() == size) {
                tester.count_bytes(result.size());
            }
        }
    }

    {
        printf("\n--- Testing direct reading + huge pages ---\n");
        perf::repetition::tester tester;
        tester.start_test_wave(size, 10);

        while (tester.is_testing()) {
            tester.begin_time();
            std::string result = read_direct_large_pages(path);
            tester.end_time();

            if (result.size() == size) {
                tester.count_bytes(result.size());
            }
        }
    }

    {
        printf("\n--- Testing direct reading + MAP_POPULATE ---\n");
        perf::repetition::tester tester;
        tester.start_test_wave(size, 10);

        while (tester.is_testing()) {
            tester.begin_time();
            std::string result = read_direct_map_populate(path);
            tester.end_time();

            if (result.size() == size) {
                tester.count_bytes(result.size());
            }
        }
    }

    {
        printf("\n--- Testing direct reading + MAP_POPULATE + huge pages ---\n");
        perf::repetition::tester tester;
        tester.start_test_wave(size, 10);

        while (tester.is_testing()) {
            tester.begin_time();
            std::string result = read_direct_map_populate_huge_pages(path);
            tester.end_time();

            if (result.size() == size) {
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
