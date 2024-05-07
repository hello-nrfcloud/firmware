# Thingy:91 Out-of-Box Experience Application

![Build and Release](https://github.com/hello-nrfcloud/firmware/workflows/Build%2C%20Test%2C%20Release/badge.svg)

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
# update nRF Connect SDK modules
cd thingy91x-oob
west update
```

### Building and running

To build the application, run the following command:

```shell
west build -b thingy91x/nrf9151/ns app
```

When using an external debugger, you can flash using this command:

```shell
west flash
```
