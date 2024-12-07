#include "../perf.hpp"

#include <thread>
#include <vector>

uint64_t fibonacci(int n) {
    PROFILE_FUNCTION();

    if (n <= 1)
        return n;

    return fibonacci(n - 1) + fibonacci(n - 2);
}

void process_data(const std::vector<uint8_t> &data) {
    PROFILE_BANDWIDTH(__func__, data.size());

    volatile uint64_t sum = 0;
    for (auto byte : data) {
        sum += byte;
    }
}

void nested_work() {
    PROFILE_FUNCTION();

    {
        for (int i = 0; i < 3; i++) {
            {
                PROFILE_BLOCK("inner_work_1");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            {
                PROFILE_BLOCK("inner_work_2");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }
}

void run() {
    PROFILE_FUNCTION();

    std::vector<uint8_t> data(1024 * 1024);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = i & 0xFF;
    }

    nested_work();
    process_data(data);
    fibonacci(20);
}

int main() {
    perf::profiler::start_profile();
    run();
    perf::profiler::end_and_print_profile();
}
