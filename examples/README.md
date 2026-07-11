# Examples

Self-contained programs, each compilable against `../include` (or the
single header). Build with `make`, build and run with `make run`; they
also build and run as tests through CMake/CTest.

| File | Shows |
|------|-------|
| [`config.cpp`](config.cpp) | a YAML configuration baked into the binary: compile-time requirement checks, settings as constants, iteration over a sequence of mappings |
| [`iteration.cpp`](iteration.cpp) | brackets and iteration: `doc["key"]`/`seq[1]` chains, uniform views with range-for and `<algorithm>`, a runtime table dump with kind dispatch |
| [`introspection.cpp`](introspection.cpp) | a generic recursive visitor over any document: kind dispatch, compile-time counting, re-serialization to flow style |
