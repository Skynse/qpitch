# Debian 12 fixes the Linux compatibility baseline at glibc 2.36 / GCC 12.
# Building on a newer host distro can otherwise produce plugins that DAW
# sandboxes (including Bitwig's) cannot load.
FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates pkg-config \
    libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
    libfreetype6-dev libfontconfig1-dev xvfb xauth \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /build
