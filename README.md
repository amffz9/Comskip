## Comskip

Commercial detector
http://www.kaashoek.com/comskip/

![Example Comskip image](https://github.com/essandess/etv-comskip/blob/master/example.png)
*Commercials are marked and skipped using [associated projects](https://github.com/essandess/etv-comskip).*

### C++23 build

The application is being migrated to C++23. FFmpeg and the bundled caption
library remain C dependencies. Use CMake 3.25+, a C++23 compiler, and vcpkg.
Google Test is included by the default manifest feature.

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

PowerShell uses `$env:VCPKG_ROOT` in place of `$VCPKG_ROOT`. To reuse packages
already installed in vcpkg's classic mode, add `-DVCPKG_MANIFEST_MODE=OFF`.
The required packages are `ffmpeg`, `argtable2`, `simpleini`, `pugixml`, and `gtest`.
The default tests feature supplies the ffmpeg executable for generated media.
Windows DLLs must be beside the executable or on PATH; the vcpkg toolchain normally copies them.
For the optional SDL interface, add `-DVCPKG_MANIFEST_FEATURES=gui` and
`-DCOMSKIP_BUILD_GUI=ON`.

Source lives in `src/` by responsibility, tests in `tests/`, and bundled C
caption code in `third_party/`. See `docs/REFACTORING.md` for migration notes.

### Configuration

Detection defaults live in config/defaults.ini and are embedded in the executable.
Use --ini=path/to/settings.ini to override defaults. Regional commercial-length
profiles live in config/profiles/. English and Spanish message catalogs live in
config/locales/. Select a language with `--language=en` or `--language=es`;
see [configuration documentation](config/README.md) for catalog overrides.
