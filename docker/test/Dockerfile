# syntax=docker/dockerfile:1
# Test image: Comskip built against FFmpeg compiled from source, twice, with and
# without --enable-gray, so one binary can be timed against both by switching
# LD_LIBRARY_PATH. VAAPI drivers for Intel (iHD) and AMD (radeonsi) are included.
#
# Build from the repository root:
#   docker build -f docker/test/Dockerfile -t comskip-test .
ARG DEBIAN=trixie

FROM debian:${DEBIAN} AS build
ARG FFMPEG_VERSION=8.0
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build nasm pkg-config curl ca-certificates xz-utils \
        libva-dev libx264-dev zlib1g-dev libargtable2-dev libsimpleini-dev libpugixml-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
RUN curl -fsSL https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz | tar xJ
RUN cd ffmpeg-${FFMPEG_VERSION} && for v in gray nogray; do \
        ./configure --prefix=/opt/ffmpeg-$v --enable-gpl --enable-libx264 --enable-vaapi \
            --enable-shared --disable-static --disable-doc --disable-debug --disable-ffplay \
            $( [ $v = gray ] && echo --enable-gray ) \
        && make -j"$(nproc)" && make install && make distclean || exit 1; \
    done
COPY bin/linux-rapidcsv-private-62664db /src/rapidcsv
COPY CMakeLists.txt vcpkg.json /src/comskip/
COPY cmake /src/comskip/cmake
COPY config /src/comskip/config
COPY assets /src/comskip/assets
COPY src /src/comskip/src
COPY tests /src/comskip/tests
RUN cmake -S /src/comskip -B /src/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF -DCOMSKIP_BUILD_GUI=OFF \
        -Drapidcsv_DIR=/src/rapidcsv/share/rapidcsv -DCMAKE_PREFIX_PATH=/opt/ffmpeg-gray \
    && cmake --build /src/build --target comskip

FROM debian:${DEBIAN}-slim
RUN sed -i 's/^Components: main$/Components: main non-free non-free-firmware/' /etc/apt/sources.list.d/debian.sources \
    && apt-get update && apt-get install -y --no-install-recommends \
        libva-dev libx264-dev zlib1g-dev libargtable2-dev libsimpleini-dev libpugixml-dev \
        intel-media-va-driver-non-free mesa-va-drivers vainfo time util-linux python3 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /opt /opt
COPY --from=build /src/build/comskip /usr/local/bin/comskip
COPY docker/test/bench.sh /usr/local/bin/bench
RUN sed -i 's/\r$//' /usr/local/bin/bench && chmod +x /usr/local/bin/bench
ENV LD_LIBRARY_PATH=/opt/ffmpeg-nogray/lib PATH=/opt/ffmpeg-nogray/bin:$PATH
ENTRYPOINT ["bench"]
