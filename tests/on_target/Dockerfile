FROM ubuntu:24.04 as base

ARG JLINK_VERSION=V794e

ENV DEBIAN_FRONTEND=noninteractive

RUN <<EOT
    apt-get -y update
    apt-get -y upgrade
    apt-get -y install wget libusb-1.0-0 python3-pip
EOT

WORKDIR /work

# Install Segger J-Link software
COPY JLink_Linux_${JLINK_VERSION}_x86_64.tgz /work/
ENV LD_LIBRARY_PATH=/opt/SEGGER/JLink/JLink_Linux_${JLINK_VERSION}_x86_64
RUN <<EOT
    mkdir -p /opt/SEGGER/JLink
    tar -xvf JLink_Linux_${JLINK_VERSION}_x86_64.tgz -C /opt/SEGGER/JLink
    rm JLink_Linux_${JLINK_VERSION}_x86_64.tgz
    ldconfig
EOT

# Install nrfutil device
# Make nrfutil install in a shared location, because when used with GitHub
# Actions, the image will be launched with the home dir mounted from the local
# checkout.
ENV NRFUTIL_HOME=/usr/local/share/nrfutil
RUN <<EOT
    wget -q https://developer.nordicsemi.com/.pc-tools/nrfutil/x64-linux/nrfutil
    mv nrfutil /usr/local/bin
    chmod +x /usr/local/bin/nrfutil
    nrfutil install device
EOT
