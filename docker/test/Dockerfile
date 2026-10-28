# syntax=docker/dockerfile:1
# Test image: Comskip built against FFmpeg compiled from source, twice, with and
# without --enable-gray, so one binary can be timed against both by switching
# LD_LIBRARY_PATH. VAAPI drivers for Intel (iHD) and AMD (radeonsi) are included.
# The fingerprint tool links a static Chromaprint built from source (the version
# vcpkg ships); Debian's fpcalc is the reference its codes are compared with.
# Its store links Debian's SQLite.
#
# Build from the repository root:
#   docker build -f docker/test/Dockerfile -t comskip-test .
ARG DEBIAN=trixie

FROM debian:${DEBIAN} AS build
ARG FFMPEG_VERSION=8.0
ARG CHROMAPRINT_VERSION=1.6.0
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build nasm pkg-config curl ca-certificates xz-utils \
        libva-dev libx264-dev zlib1g-dev libargtable2-dev libsimpleini-dev libpugixml-dev nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
RUN curl -fsSL https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz | tar xJ
RUN cd ffmpeg-${FFMPEG_VERSION} && for v in gray nogray; do \
        ./configure --prefix=/opt/ffmpeg-$v --enable-gpl --enable-libx264 --enable-vaapi \
            --enable-shared --disable-static --disable-doc --disable-debug --disable-ffplay \
            $( [ $v = gray ] && echo --enable-gray ) \
        && make -j"$(nproc)" && make install && make distclean || exit 1; \
    done
RUN curl -fsSL https://github.com/acoustid/chromaprint/archive/refs/tags/v${CHROMAPRINT_VERSION}.tar.gz | tar xz \
    && PKG_CONFIG_PATH=/opt/ffmpeg-gray/lib/pkgconfig cmake -S chromaprint-${CHROMAPRINT_VERSION} -B chromaprint-build \
        -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_TOOLS=OFF -DBUILD_TESTS=OFF \
        -DFFT_LIB=avtx -DCMAKE_PREFIX_PATH=/opt/ffmpeg-gray -DCMAKE_INSTALL_PREFIX=/opt/chromaprint \
    && cmake --build chromaprint-build --target install
# The fingerprint store is SQLite (its own layer, so the FFmpeg builds stay cached).
RUN apt-get update && apt-get install -y --no-install-recommends libsqlite3-dev sqlite3 && rm -rf /var/lib/apt/lists/*
COPY bin/linux-rapidcsv-private-62664db /src/rapidcsv
COPY CMakeLists.txt vcpkg.json /src/comskip/
COPY cmake /src/comskip/cmake
COPY config /src/comskip/config
COPY assets /src/comskip/assets
COPY src /src/comskip/src
COPY tests /src/comskip/tests
RUN cmake -S /src/comskip -B /src/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF -DCOMSKIP_BUILD_GUI=OFF \
        -Drapidcsv_DIR=/src/rapidcsv/share/rapidcsv -DCMAKE_PREFIX_PATH="/opt/ffmpeg-gray;/opt/chromaprint" \
    && cmake --build /src/build --target comskip comskip-fingerprint

FROM debian:${DEBIAN}-slim
RUN sed -i 's/^Components: main$/Components: main non-free non-free-firmware/' /etc/apt/sources.list.d/debian.sources \
    && apt-get update && apt-get install -y --no-install-recommends \
        libva-dev libx264-dev zlib1g-dev libargtable2-dev libsimpleini-dev libpugixml-dev \
        intel-media-va-driver-non-free mesa-va-drivers vainfo time util-linux python3 libchromaprint-tools libsqlite3-0 sqlite3 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /opt /opt
COPY --from=build /src/build/comskip /usr/local/bin/comskip
COPY --from=build /src/build/comskip-fingerprint /usr/local/bin/comskip-fingerprint
COPY docker/test/bench.sh /usr/local/bin/bench
RUN sed -i 's/\r$//' /usr/local/bin/bench && chmod +x /usr/local/bin/bench
ENV LD_LIBRARY_PATH=/opt/ffmpeg-nogray/lib PATH=/opt/ffmpeg-nogray/bin:$PATH
ENTRYPOINT ["bench"]
