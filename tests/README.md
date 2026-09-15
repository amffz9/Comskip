# Portable detection tests

These tests exercise the production commercial-length module without FFmpeg,
SDL, platform headers, or media fixtures. They preserve the existing detector's
frame rounding, timing correction, strict/optional lengths, tolerance clamps,
override precedence, and minimum-show-length boundary. Both the default and
`CHINESE_SIZE_TABLE` builds are tested.

With a configured Autotools build, run `make check`.

To test the module independently using Clang (GCC also works):

```sh
mkdir -p bin
clang -std=c11 -Wall -Wextra -Werror -I. commercial_length.c tests/commercial_length_test.c -o bin/commercial-length-test.exe
./bin/commercial-length-test.exe
clang -std=c11 -Wall -Wextra -Werror -DCHINESE_SIZE_TABLE -I. commercial_length.c tests/commercial_length_test.c -o bin/commercial-length-chinese-test.exe
./bin/commercial-length-chinese-test.exe
```

On PowerShell use `New-Item -ItemType Directory -Force bin` to create the output
directory. A compiler and its platform development libraries must be installed.
The portable-unit-test workflow runs these commands on Windows, Linux, and macOS.

This suite does not yet cover decoding, complete commercial detection, or output
files. Those need integration fixtures and expected results before larger changes.
