FROM ubuntu:22.04 as base

ARG NCS_VERSION=v2.5.0

ENV DEBIAN_FRONTEND=noninteractive

SHELL [ "/bin/bash", "-euxo", "pipefail", "-c" ]

RUN <<EOT
    apt-get -y update
    apt-get -y upgrade
    apt-get -y install wget unzip
EOT

# Install Memfault CLI
RUN apt-get -y install python3-pip && pip3 install memfault-cli

# Install toolchain
# Make nrfutil install in a shared location, because when used with GitHub
# Actions, the image will be launched with the home dir mounted from the local
# checkout.
ENV NRFUTIL_HOME=/usr/local/share/nrfutil
RUN <<EOT
    wget -q https://developer.nordicsemi.com/.pc-tools/nrfutil/x64-linux/nrfutil
    mv nrfutil /usr/local/bin
    chmod +x /usr/local/bin/nrfutil
    nrfutil install toolchain-manager
    nrfutil toolchain-manager install --ncs-version ${NCS_VERSION}
    nrfutil toolchain-manager list
EOT

# Prepare image with a ready to use build environment
ADD . /workdir
WORKDIR /workdir
SHELL ["nrfutil","toolchain-manager","launch","/bin/bash","-euxo","pipefail","--","-c"]
RUN <<EOT
    west init -l . && west update --narrow -o=--depth=1
EOT
