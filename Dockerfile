FROM ubuntu:22.04 as base

ARG NCS_VERSION=v2.4.0

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -y update && \
    apt-get -y upgrade && \
    apt-get -y install \
        wget unzip

# Install toolchain
RUN wget -q https://developer.nordicsemi.com/.pc-tools/nrfutil/x64-linux/nrfutil && \
    mv nrfutil /usr/local/bin && \
    chmod +x /usr/local/bin/nrfutil && \
    nrfutil install toolchain-manager && \
    nrfutil toolchain-manager install --ncs-version ${NCS_VERSION} && \
    nrfutil toolchain-manager list

# Prepare image with a ready to use build environment
ADD . /workdir
WORKDIR /workdir
RUN nrfutil toolchain-manager launch /bin/bash -- -c 'west init -l . && west update --narrow -o=--depth=1'

# Install Memfault CLI
RUN apt-get -y install python3-pip && pip3 install memfault-cli

# Launch into build environment
# Currently this is not supported in GitHub Actions
# See https://github.com/actions/runner/issues/1964
# ENTRYPOINT [ "nrfutil", "toolchain-manager", "launch", "/bin/bash" ]