## Comskip

Commercial detector
http://www.kaashoek.com/comskip/

![Example Comskip image](https://github.com/essandess/etv-comskip/blob/master/example.png)
*Commercials are marked and skipped using [associated projects](https://github.com/essandess/etv-comskip).*

### C++23 build

The application uses C++23 and FFmpeg's C API. Use CMake 3.25+, a C++23 compiler, and vcpkg.
Google Test is included by the default manifest feature.

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

PowerShell uses `$env:VCPKG_ROOT` in place of `$VCPKG_ROOT`. To reuse packages
already installed in vcpkg's classic mode, add `-DVCPKG_MANIFEST_MODE=OFF`.
The required packages are `ffmpeg`, `argtable2`, `simpleini`, `pugixml`, `rapidcsv`, and `gtest`.
The default tests feature supplies the ffmpeg executable for generated media.
Windows DLLs must be beside the executable or on PATH; the vcpkg toolchain normally copies them.
For the optional SDL interface, add `-DVCPKG_MANIFEST_FEATURES=gui` and
`-DCOMSKIP_BUILD_GUI=ON`.

Source lives in `src/` by responsibility and tests in `tests/`.

### Chapter export filenames

`output_chapters=1` writes frame-boundary chapters to `<output basename>.chap`.
`output_ipodchap=1` also uses `.chap` when enabled alone. When both formats are
enabled, iPod chapters use `<output basename>.ipod.chap` so each file contains
one complete format. Normal analysis and review exports use the same rule.
See `docs/REFACTORING.md` for migration notes.

### Configuration

Detection defaults live in config/defaults.ini and are embedded in the executable.
Use --ini=path/to/settings.ini to override defaults. Regional commercial-length
profiles live in config/profiles/. English and Spanish message catalogs live in
config/locales/. Select a language with `--language=en` or `--language=es`;
see [configuration documentation](config/README.md) for catalog overrides.
