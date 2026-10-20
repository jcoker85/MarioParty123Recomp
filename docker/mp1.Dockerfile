# Linux environment for building the mariopartyrd decomp (lib/mp1-decomp).
# The decomp only builds on Linux/macOS; this image lets Windows hosts build it.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils-mips-linux-gnu gcc-mips-linux-gnu \
        build-essential git make python3 python3-pip python3-venv ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Python deps pinned by the decomp's requirements.txt
COPY lib/mp1-decomp/requirements.txt /tmp/requirements.txt
RUN python3 -m venv /opt/venv && /opt/venv/bin/pip install --no-cache-dir -r /tmp/requirements.txt

RUN git config --system --add safe.directory '*'
