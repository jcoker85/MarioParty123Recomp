# Linux environment for building the mariopartyrd decomp (lib/mp3-decomp, Mario Party 3).
# The decomp only builds on Linux/macOS; this image lets Windows hosts build it.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils-mips-linux-gnu gcc-mips-linux-gnu \
        build-essential git make python3 python3-pip python3-venv ca-certificates wget \
    && rm -rf /var/lib/apt/lists/*

# Python deps pinned by the decomp's requirements.txt.
COPY lib/mp3-decomp/requirements.txt /tmp/requirements.txt
RUN python3 -m venv /opt/venv && /opt/venv/bin/pip install --no-cache-dir -r /tmp/requirements.txt

# KMC GCC 2.7.2 + binutils (the decomp's `cd tools && make`), fetched once into the image.
COPY lib/mp3-decomp/tools/Makefile /opt/mp3tools/Makefile
RUN cd /opt/mp3tools && make gcc_2.7.2/linux/gcc gcc_2.7.2/linux/as

RUN git config --system --add safe.directory '*'
