# OOB on target test

Test docker image: ghcr.io/dematteisgiacomo/ubuntu-jlink-nrfutil:latest

Base ubuntu image with jlink and nrfutil device installed (see Dockerfile)

pytest is used

## Test description

To be added.

## Run test docker image locally
```shell
docker run --rm -it \
  --privileged \
  -v /dev:/dev:rw \
  -v /run/udev:/run/udev \
  ghcr.io/hello-nrfcloud/firmware:latest
```

## Test docker image version control

JLINK_VERSION=V794e
