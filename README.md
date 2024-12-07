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

--- Testing reserved string iterator-based reading ---

min: 349.454461ms 0.028058GB/s
max: 467.705251ms 0.020964GB/s
avg: 416.844633ms 0.023522GB/s

--- Testing stringstream reading ---

min: 14.470717ms 0.677577GB/s
max: 26.306175ms 0.372727GB/s
avg: 19.902128ms 0.492662GB/s

--- Testing direct reading ---

min: 0.969490ms 10.113594GB/s
max: 6.355256ms 1.542822GB/s
avg: 1.231268ms 7.963362GB/s
```

## Installation

Header-only library - just include the header in your project:

`#include "perf.hpp"`

## Dependencies

- C++17 or later
- x86 CPU with TSC support
