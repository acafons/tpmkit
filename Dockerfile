# syntax=docker/dockerfile:1
FROM ubuntu:26.04

LABEL maintainer="Anderson Fonseca <cf.anderson@gmail.com>"
LABEL description="Dockerfile for building a container with IBM's Software TPM 2.0, TPM2-TSS, TPM2-ABRM, and TPM2-Tools"
LABEL version="1.0"

# Define the arguments for the versions of the software to be installed
ARG openssl_version=openssl-3.5.0
ARG ibmtpm_git=master
ARG tpmtss_version=4.1.3
ARG tpmabrm_version=3.0.0
ARG tpmtools_version=5.7

#RUN apt update
RUN apt-get update && apt-get install -y \ 
    autoconf \
    autoconf-archive \
    automake \
    libarchive-dev \
    build-essential \
    clang-format \
    g++ \
    gcc \
    git \
    libssl-dev \
    libtool \
    m4 \
    net-tools \
    pkg-config \
    libxml2-dev \
    git \
    doxygen \
    libjson-c-dev \
    libcurl4-openssl-dev \
    dbus-x11 \
    dbus \
    sudo \
    libdbus-glib-1-dev \
    libcmocka0 \
    libcmocka-dev \
    libtool \
    liburiparser-dev \
    uthash-dev \
    libsqlite3-dev \
    wget \
    procps \
    iproute2 \
    gdb \
    libfmt-dev \
    libspdlog-dev \
    valgrind

# Download and install IBM's Software TPM 2.0
WORKDIR /tmp

RUN git clone https://github.com/kgoldman/ibmswtpm2.git ibmtpm

WORKDIR /tmp/ibmtpm/src

RUN make && \ 
    make install

# Download and install OpenSSL
WORKDIR /tmp

RUN git clone https://github.com/openssl/openssl.git

WORKDIR /tmp/openssl

RUN git checkout $openssl_version && \ 
    ./config -d \
        --prefix=/usr \
        --openssldir=/usr/local/ssl \
        shared \
        zlib-dynamic \
        no-docs && \
    make -j5 && \
    make install

RUN ldconfig

# Download and install TPM2-TSS
WORKDIR /tmp

RUN git clone https://github.com/tpm2-software/tpm2-tss.git

WORKDIR /tmp/tpm2-tss

RUN git checkout $tpmtss_version && \ 
    ./bootstrap && \
    ./configure \
        --enable-debug \
        --disable-doxygen-doc \
        --prefix=/usr && \
    make -j5 && \
    make install

RUN ldconfig

# Download and install TPM2-ABRM
WORKDIR /tmp

RUN git clone https://github.com/tpm2-software/tpm2-abrmd.git

WORKDIR /tmp/tpm2-abrmd

RUN git checkout $tpmabrm_version && \ 
    ./bootstrap && \
    ./configure \
        --enable-debug \
        --with-dbuspolicydir=/etc/dbus-1/system.d \
        --with-udevrulesdir=/usr/lib/udev/rules.d \
        --with-systemdsystemunitdir=/usr/lib/systemd/system \
        --libdir=/usr/lib \
        --prefix=/usr && \
    make -j5 && \
    make install

RUN ldconfig

# Download and install TPM2-Tools
WORKDIR /tmp

RUN git clone https://github.com/tpm2-software/tpm2-tools.git

WORKDIR /tmp/tpm2-tools

RUN git checkout $tpmtools_version && \ 
    ./bootstrap && \
    ./configure \
        --enable-debug \
        --prefix=/usr && \
    make -j5 && \
    make install

RUN ldconfig

# Copy D-Bus policy for tpm2-abrmd (paths mirror the image filesystem)
COPY docker/etc/dbus-1/system.d/ /etc/dbus-1/system.d/

# TPM stack helper scripts (TPM state/logs: TPMKIT_STATE_DIR, default /home/app/.tpm-state)
# --chmod avoids non-executable copies from the build context (e.g. git filemode 644).
COPY --chmod=755 scripts/tpm-start.sh scripts/tpm-stop.sh /usr/local/bin/

# Create a new group called 'machinedata'
# RUN groupadd machinedata -> Dune does not have this group. So tss will be used

# Create a new user called 'app'
RUN useradd --system --create-home --shell /bin/bash app && \ 
    usermod -aG sudo app

# Allow passwordless sudo for the 'app' user
RUN echo "app ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Pre-create .gnupg directory so VS Code GPG forwarding doesn't crash (ubuntu:26.04 dd bs=0 bug)
RUN mkdir -p /home/app/.gnupg && \ 
    chmod 700 /home/app/.gnupg && \
    touch /home/app/.gnupg/pubring.gpg && \
    chmod 600 /home/app/.gnupg/pubring.gpg && \
    chown -R app:app /home/app/.gnupg

# Switch to the 'app' user
USER app

WORKDIR /home/app

# Bind-mount a host directory here (see scripts/run-tpmkit-docker.sh) to persist logs and NVChip.
ENV TPMKIT_STATE_DIR=/home/app/.tpm-state

CMD ["tail", "-f", "/dev/null"]
