# 9xOS build container: pins the host tools so every build uses the same ones.
#   docker build -t 9xos-build tools/docker
#   docker run --rm -it -v "$PWD":/work -w /work 9xos-build ./build.sh
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        bc bison build-essential ca-certificates cmake cpio file flex git \
        libncurses-dev libssl-dev python3 rsync unzip wget \
        shellcheck \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m builder
USER builder
ENV NINEXOS_CACHE=/home/builder/.cache/9xos
