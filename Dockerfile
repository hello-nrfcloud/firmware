FROM ubuntu:22.04 as base

ARG NCS_VERSION=v2.4.0

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -y update && \
    apt-get -y upgrade && \
    apt-get -y install \
        wget

RUN wget https://developer.nordicsemi.com/.pc-tools/nrfutil/x64-linux/nrfutil && mv nrfutil /usr/local/bin && chmod +x /usr/local/bin/nrfutil

RUN nrfutil install toolchain-manager

RUN nrfutil toolchain-manager install --ncs-version ${NCS_VERSION}

ADD . /workdir

WORKDIR /workdir

# Prepare image with a ready to use build environment
RUN nrfutil toolchain-manager launch /bin/bash -- -c 'west init -l . && west update'
