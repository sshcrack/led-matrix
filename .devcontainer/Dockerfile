FROM mcr.microsoft.com/devcontainers/cpp:1-debian-12

# Install required packages (excluding mono and nuget-related setup)
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      python3-jinja2 pkg-config autoconf automake libtool python3 linux-libc-dev curl libltdl-dev libx11-dev libxft-dev libxext-dev libwayland-dev libxkbcommon-dev libegl1-mesa-dev libibus-1.0-dev libxrandr-dev libxrandr2 wayland-protocols extra-cmake-modules xorg-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev libgtk-3-dev libayatana-appindicator3-dev ca-certificates && \
    update-ca-certificates && \
    curl -sSL https://github.com/Kitware/CMake/releases/download/v3.25.0/cmake-3.25.0-linux-x86_64.sh -o /tmp/cmake-install.sh && \
    chmod +x /tmp/cmake-install.sh && \
    /tmp/cmake-install.sh --skip-license --prefix=/usr/local && \
    rm /tmp/cmake-install.sh

# Download and extract cross-compile toolchain
ENV CROSS_COMPILE_ROOT=/opt/cross-compile
RUN mkdir -p ${CROSS_COMPILE_ROOT} && \
    curl -L -o ${CROSS_COMPILE_ROOT}/cross-compile.tar.xz "https://github.com/sshcrack/led-matrix/releases/download/v0.0.1-beta/cross-compile.tar.xz" && \
    tar -xvf ${CROSS_COMPILE_ROOT}/cross-compile.tar.xz -C ${CROSS_COMPILE_ROOT} && \
    rm ${CROSS_COMPILE_ROOT}/cross-compile.tar.xz

# Set environment variable for cross-compile toolchain
ENV PATH="${CROSS_COMPILE_ROOT}:$PATH"
