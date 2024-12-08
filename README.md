# perf

A lightweight header-only C++ library for code profiling and performance measurement.

## Profiler Usage
Track performance metrics of code blocks with automatic scope handling:

```
PROFILE_FUNCTION();              // Profile current function
PROFILE_BLOCK("operation");      // Profile arbitrary block
PROFILE_BANDWIDTH("read", size); // Profile with throughput
```

See [examples/profiler.cpp](examples/profiler.cpp) for more concrete examples.

```
Total time: 57.4838ms (CPU freq 3294364920)
Function               Calls    Time(ms)     Self(%)    Total(%)        GB/s
----------------------------------------------------------------------------
inner_work_1               3     30.3510    52.7991%           -           -
inner_work_2               3     15.5443    27.0412%           -           -
process_data               1      6.0661    10.5528%           -    0.160986
run                        1      4.7746     8.3060%  99.988499%           -
fibonacci              21891      0.7357     1.2799%           -           -
nested_work                1      0.0054     0.0095%  79.849785%           -
```

## Repetition Tester Usage

Measure best-case performance by repeatedly running operations with warm caches:

```
perf::repetition::tester tester(work_size, seconds_to_test);
while(tester.is_testing()) {
    tester.begin_time();
    // operation to measure
    tester.end_time();
}
```

See [examples/repetition.cpp](examples/repetition.cpp) for more concrete examples.

```
Testing file: "../json/examples/data/haversine_p100000_a23008294008.828533.json" (10528071 bytes)

--- Testing direct reading ---

min: 1.910468ms 5.132267GB/s
max: 13.461418ms 0.728380GB/s PF: 2572.0000 (3.9974k/fault)
avg: 2.705943ms 3.623516GB/s

--- Testing direct reading + huge pages ---

min: 1.586541ms 6.180131GB/s PF: 6.0000 (1713.5532k/fault)
max: 5.727954ms 1.711786GB/s PF: 6.0000 (1713.5532k/fault)
avg: 2.782507ms 3.523811GB/s PF: 6.0000 (1713.5532k/fault)

--- Testing direct reading + MAP_POPULATE ---

min: 1.106001ms 8.865296GB/s PF: 161.0000 (63.8591k/fault)
max: 4.070727ms 2.408668GB/s PF: 162.0000 (63.4649k/fault)
avg: 1.331197ms 7.365573GB/s PF: 161.0000 (63.8591k/fault)

--- Testing direct reading + MAP_POPULATE + huge pages ---

min: 1.579272ms 6.208578GB/s PF: 6.0000 (1713.5532k/fault)
max: 5.179668ms 1.892984GB/s PF: 6.0000 (1713.5532k/fault)
avg: 1.884944ms 5.201762GB/s PF: 6.0000 (1713.5532k/fault)
```

## Installation

Header-only library - just include the header in your project:

`#include "perf.hpp"`

## Dependencies

- C++17 or later
- x86 CPU with TSC support
