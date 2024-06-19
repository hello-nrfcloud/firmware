# Thingy:91 X Out-of-Box Experience Application

#### Oncommit:
[![Build](https://github.com/hello-nrfcloud/firmware/actions/workflows/build.yml/badge.svg)](https://github.com/hello-nrfcloud/firmware/actions/workflows/build.yml)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=hello-nrfcloud_firmware&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=hello-nrfcloud_firmware)

#### Nightly:
[![Target_tests](https://github.com/hello-nrfcloud/firmware/actions/workflows/on_target.yml/badge.svg?event=schedule)](https://github.com/hello-nrfcloud/firmware/actions/workflows/on_target.yml?query=branch%3Amain+event%3Aschedule)

This project is based on the
[NCS Example Application](https://github.com/nrfconnect/ncs-example-application).

## Getting started

Before getting started, make sure you have a proper nRF Connect SDK development
environment. Follow the official
[Getting started guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/getting_started.html).

### Initialization

The first step is to initialize the workspace folder (`thingy91x-oob`) where the
`firmware` project and all nRF Connect SDK modules will be cloned. Run the
following command:

```shell
# initialize thingy91x-oob workspace
west init -m https://github.com/hello-nrfcloud/firmware --mr main thingy91x-oob

cd thingy91x-oob

# enable Bosch environmental sensor driver
west config manifest.group-filter +bsec

# update nRF Connect SDK modules
west update

# use sysbuild by default
west config build.sysbuild True
```

### Building and running

To build the application, run the following command:

```shell
west build -b thingy91x/nrf9151/ns app
```

When using an external debugger, you can flash using this command:

```shell
west flash --erase
```
