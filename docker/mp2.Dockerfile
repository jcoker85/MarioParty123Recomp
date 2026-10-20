# Linux environment for building the mariopartyrd decomp (lib/mp2-decomp, Mario Party 2).
# The decomp only builds on Linux/macOS; this image lets Windows hosts build it.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils-mips-linux-gnu gcc-mips-linux-gnu \
        build-essential git make python3 python3-pip python3-venv ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Python deps pinned by the decomp's requirements.txt. Its splat fork is installed without
# splat's [mips] extras, so add the ones splat imports at startup.
COPY lib/mp2-decomp/requirements.txt /tmp/requirements.txt
RUN python3 -m venv /opt/venv && /opt/venv/bin/pip install --no-cache-dir -r /tmp/requirements.txt \
    && /opt/venv/bin/pip install --no-cache-dir n64img pygfxd crunch64 rabbitizer

RUN git config --system --add safe.directory '*'
