#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sys/resource.h>
#include <vector>
#include <x86intrin.h>

namespace perf {

using u32 = uint32_t;
using u64 = uint64_t;

namespace time {
    inline u64 read_cpu_timer() { return __rdtsc(); }

    inline u64 estimate_cpu_timer_freq() {
        constexpr auto MEASUREMENT_PERIOD = std::chrono::milliseconds(100);
        constexpr u64 MICROSECONDS_PER_SECOND = 1'000'000ULL;

        u64 cpu_start = __rdtsc();
        auto os_start = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - os_start < MEASUREMENT_PERIOD) {
        }

        u64 cpu_ticks = __rdtsc() - cpu_start;
        auto os_duration = std::chrono::steady_clock::now() - os_start;
        auto microseconds =
            std::chrono::duration_cast<std::chrono::microseconds>(os_duration).count();

        return MICROSECONDS_PER_SECOND * cpu_ticks / microseconds;
    }

} // namespace time

namespace convert {
    inline double ticks_to_seconds(u64 ticks, u64 freq) { return static_cast<double>(ticks) / freq; }

    inline double ticks_to_ms(u64 ticks, u64 freq) { return ticks_to_seconds(ticks, freq) * 1000.0; }

    inline double bytes_per_sec_to_gbps(u64 bytes, double seconds) {
        return (bytes / seconds) / (1024.0 * 1024.0 * 1024.0);
    }
} // namespace convert

namespace counters {
    inline u64 soft_page_faults() {
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            std::cerr << "ERROR: counters::soft_page_faults(): " << strerror(errno) << "\n";
            return 0;
        }

        return usage.ru_minflt;
    }
} // namespace counters

namespace profiler {
    struct profile_anchor {
        const char *name = nullptr;
        u64 hits = 0;
        u64 tsc_elapsed_exclusive = 0;
        u64 tsc_elapsed_inclusive = 0;
        u64 processed_byte_count = 0;
    };

    struct profiler {
        u64 start_tsc = 0;
        u64 end_tsc = 0;
    };

    inline profiler global_profiler;
    inline profile_anchor anchors[4096] = {};
    inline u32 global_profiler_parent_index = 0;

    class profile_block {
    public:
        profile_block(const char *name, u32 anchor_index, u64 processed_byte_count) {
            m_name = name;
            m_anchor_index = anchor_index;
            m_parent_index = global_profiler_parent_index;

            profile_anchor *anchor = anchors + anchor_index;
            m_old_tsc_elapsed_inclusive = anchor->tsc_elapsed_inclusive;
            anchor->processed_byte_count += processed_byte_count;

            global_profiler_parent_index = m_anchor_index;
            m_start_tsc = time::read_cpu_timer();
        }

        ~profile_block() {
            u64 elapsed = time::read_cpu_timer() - m_start_tsc;
            global_profiler_parent_index = m_parent_index;

            profile_anchor *parent = anchors + m_parent_index;
            profile_anchor *anchor = anchors + m_anchor_index;

            parent->tsc_elapsed_exclusive -= elapsed;
            anchor->tsc_elapsed_exclusive += elapsed;
            anchor->tsc_elapsed_inclusive = m_old_tsc_elapsed_inclusive + elapsed;

            anchor->hits++;
            anchor->name = m_name;
        }

        profile_block(const profile_block &) = delete;
        profile_block &operator=(const profile_block &) = delete;
        profile_block(profile_block &&) = delete;
        profile_block &operator=(profile_block &&) = delete;

    private:
        const char *m_name;
        u32 m_anchor_index;
        u32 m_parent_index;
        u64 m_old_tsc_elapsed_inclusive;
        u64 m_start_tsc;
    };

    inline void start_profile() { global_profiler.start_tsc = time::read_cpu_timer(); }

    inline void end_and_print_profile() {
        global_profiler.end_tsc = time::read_cpu_timer();

        u64 cpu_freq = time::estimate_cpu_timer_freq();
        u64 cpu_elapsed = global_profiler.end_tsc - global_profiler.start_tsc;

        std::cout << "\nTotal time: " << convert::ticks_to_ms(cpu_elapsed, cpu_freq)
                  << "ms (CPU freq " << cpu_freq << ")\n\n";

        std::vector<size_t> anchor_indices;
        for (size_t i = 0; i < 4096; ++i) {
            if (anchors[i].tsc_elapsed_inclusive) {
                anchor_indices.push_back(i);
            }
        }

        std::sort(anchor_indices.begin(), anchor_indices.end(), [](size_t a, size_t b) {
            return anchors[a].tsc_elapsed_exclusive > anchors[b].tsc_elapsed_exclusive;
        });

        // clang-format off
        std::cout << std::left << std::setw(20) << "Function" 
                  << std::right << std::setw(8) << "Calls"
                  << std::setw(12) << "Time(ms)" 
                  << std::setw(12) << "Self(%)" 
                  << std::setw(12) << "Total(%)"
                  << std::setw(12) << "GB/s"
                  << '\n'
                  << std::string(76, '-') << '\n';
        // clang-format on

        for (size_t idx : anchor_indices) {
            const auto &anchor = anchors[idx];

            double seconds = convert::ticks_to_seconds(anchor.tsc_elapsed_exclusive, cpu_freq);
            double self_percent = 100.0 * anchor.tsc_elapsed_exclusive / cpu_elapsed;
            double total_percent = 100.0 * anchor.tsc_elapsed_inclusive / cpu_elapsed;

            double throughput = 0.0;
            if (seconds > 0 && anchor.processed_byte_count > 0) {
                throughput = convert::bytes_per_sec_to_gbps(anchor.processed_byte_count, seconds);
            }

            // clang-format off
            std::cout << std::left << std::setw(20) << anchor.name 
                      << std::right << std::setw(8) << anchor.hits 
                      << std::fixed << std::setprecision(4) 
                      << std::setw(12) << 1000 * seconds
                      << std::setw(11) << self_percent << '%'
                      << std::setw(12)
                      << (anchor.tsc_elapsed_inclusive != anchor.tsc_elapsed_exclusive
                              ? std::to_string(total_percent) + "%"
                              : "-")
                      << std::setw(12)
                      << (throughput > 0 ? std::to_string(throughput) : "-")
                      << '\n';
            // clang-format on
        }
    }

    // NOTE(louis): no collision resolution
    constexpr u32 hash(const char *str) {
        u32 hash = 5381;
        while (*str) {
            hash = ((hash << 5) + hash) + *str++;
        }
        return (hash % 4095) + 1;
    }

#define CONCAT2(A, B) A##B
#define CONCAT(A, B) CONCAT2(A, B)

#define PROFILE_BANDWIDTH(name, bytes)                                                              \
    perf::profiler::profile_block CONCAT(block, __LINE__)(name, perf::profiler::hash(name), bytes)
#define PROFILE_BLOCK(name) PROFILE_BANDWIDTH(name, 0)
#define PROFILE_FUNCTION() PROFILE_BLOCK(__func__)

} // namespace profiler

// TODO(louis): rewrite this. super clunky, not a fan at all
namespace repetition {
    enum class state {
        uninitialised,
        testing,
        completed,
        error,
    };

    enum metric {
        test_count,
        cpu_timer,
        page_faults,
        byte_count,

        num_metrics,
    };

    struct metric_values {
        u64 values[metric::num_metrics] = {};
    };

    struct results {
        metric_values total;
        metric_values min;
        metric_values max;
    };

    using metric_reader = u64 (*)();

    class tester {
    public:
        tester() {
            m_readers[metric::cpu_timer] = time::read_cpu_timer;
            m_readers[metric::page_faults] = counters::soft_page_faults;

            for (int i = 0; i < metric::num_metrics; i++) {
                m_results.min.values[i] = ~0ull;
            }
        }

        void start_test_wave(u64 target_processed_byte_count, u32 seconds_to_try) {
            if (m_state == state::uninitialised) {
                m_state = state::testing;
                m_target_byte_count = target_processed_byte_count;
            } else if (m_state == state::completed) {
                m_state = state::testing;

                if (target_processed_byte_count != m_target_byte_count) {
                    error("target_processed_byte_count changed between waves");
                }
            }

            m_try_for_time = time::estimate_cpu_timer_freq() * seconds_to_try;
            m_tests_started_at = time::read_cpu_timer();
        }

        void begin_time() {
            m_open_block_count++;

            for (int i = 0; i < metric::num_metrics; i++) {
                if (m_readers[i]) {
                    m_current.values[i] -= m_readers[i]();
                }
            }
        }

        void end_time() {
            for (int i = 0; i < metric::num_metrics; i++) {
                if (m_readers[i]) {
                    m_current.values[i] += m_readers[i]();
                }
            }

            m_close_block_count++;
        }

        void count_bytes(u64 byte_count) { m_current.values[metric::byte_count] += byte_count; }

        bool is_testing() {
            if (m_state != state::testing) {
                return false;
            }

            u64 current_time = time::read_cpu_timer();

            if (m_open_block_count) {
                if (m_open_block_count != m_close_block_count) {
                    error("imbalanced begin_time() and end_time()");
                    return false;
                }

                if (m_current.values[metric::byte_count] != m_target_byte_count &&
                    m_target_byte_count != 0) {
                    error("mismatch between accumulated and target processed byte count");
                }

                if (m_state == state::testing) {
                    m_current.values[metric::test_count] = 1;

                    for (int i = 0; i < metric::num_metrics; i++) {
                        m_results.total.values[i] += m_current.values[i];
                        m_results.max.values[i] =
                            std::max(m_results.max.values[i], m_current.values[i]);
                        m_results.min.values[i] =
                            std::min(m_results.min.values[i], m_current.values[i]);
                    }

                    if (m_current.values[metric::cpu_timer] ==
                        m_results.min.values[metric::cpu_timer]) {
                        m_tests_started_at = current_time;
                    }

                    m_open_block_count = 0;
                    m_close_block_count = 0;
                    m_current = {};
                }
            }

            if ((current_time - m_tests_started_at) >= m_try_for_time) {
                m_state = state::completed;
                print_results();
            }

            return m_state == state::testing;
        }

        const results &get_results() const { return m_results; }

    private:
        void error(const char *message) {
            std::cerr << "ERROR: " << message << "\n";
            m_state = state::error;
        }

        void print_metric_value(const char *label, metric_values value) {
            double seconds = convert::ticks_to_seconds(value.values[metric::cpu_timer],
                                                       time::estimate_cpu_timer_freq());

            std::cout << label << ": " << std::fixed << std::setprecision(6) << (1000.0 * seconds)
                      << "ms";

            if (value.values[metric::byte_count] > 0) {
                std::cout << " " << std::setprecision(6)
                          << convert::bytes_per_sec_to_gbps(value.values[metric::byte_count],
                                                            seconds)
                          << "GB/s";
            }

            if (value.values[metric::page_faults] > 0) {
                std::cout << " PF: " << std::setprecision(4)
                          << (double)value.values[metric::page_faults];

                if (value.values[metric::byte_count] > 0) {
                    std::cout << " (" << std::setprecision(4)
                              << (double)value.values[metric::byte_count] /
                                     (value.values[metric::page_faults] * 1024.0)
                              << "k/fault)";
                }
            }
        }

        void print_results() {
            std::cout << "\n";
            print_metric_value("min", m_results.min);
            std::cout << "\n";
            print_metric_value("max", m_results.max);
            std::cout << "\n";

            if (m_results.total.values[metric::test_count] > 0) {
                metric_values avg = m_results.total;
                for (int i = 0; i < metric::num_metrics; i++) {
                    avg.values[i] /= m_results.total.values[metric::test_count];
                }
                print_metric_value("avg", avg);
                std::cout << "\n";
            }
        }

        state m_state = state::uninitialised;

        u64 m_target_byte_count = 0;
        u64 m_try_for_time = 0;
        u64 m_tests_started_at = 0;

        u32 m_open_block_count = 0;
        u32 m_close_block_count = 0;

        metric_reader m_readers[metric::num_metrics] = {};
        metric_values m_current = {};
        results m_results = {};
    };

}; // namespace repetition

} // namespace perf
