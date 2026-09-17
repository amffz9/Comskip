# Windows testing

Use the CMake test target instead of launching test executables individually:

```powershell
$env:SDL_VIDEODRIVER = "dummy"
$env:SDL_AUDIODRIVER = "dummy"
$env:ASAN_OPTIONS = "abort_on_error=1:halt_on_error=1:log_path=asan"
$env:UBSAN_OPTIONS = "halt_on_error=1:print_stacktrace=1:log_path=ubsan"
cmake --build bin/build23-sanitize --target comskip-check -j 3
```

`comskip-check` now depends on the test executables, so a fresh sanitizer
configuration builds them before CTest runs. CMake also applies the dummy SDL
drivers and sanitizer variables to every registered test. CTest writes the
test summary to `bin/<build>/test-results.log`; sanitizer reports are written
beside it with the `asan` or `ubsan` prefix when a finding occurs.

The current Windows Clang ASan runtime still aborts with `0xc0000005` in a
small set of expected-failure/unwind paths before Comskip can construct its
typed diagnostic. This is tracked as B110 in `docs/BUGS.md`; it is a runtime
limitation, not a reason to disable sanitizer reporting or hide failures.

The Windows CMake setup copies the vcpkg GoogleTest runtime DLLs beside the
tests. This prevents the missing-`gtest_main.dll` loader dialog that occurs
when a test executable is launched without the vcpkg runtime directory on
`PATH`. Keep test execution under CTest so failures remain in the log.
