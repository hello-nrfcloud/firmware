# Thingy:91 Out-of-Box Experience Application

![Build and Release](https://github.com/hello-nrfcloud/firmware/workflows/Build%2C%20Test%2C%20Release/badge.svg)
[![@commitlint/config-conventional](https://img.shields.io/badge/%40commitlint-config--conventional-brightgreen
)](https://github.com/conventional-changelog/commitlint/tree/master/@commitlint/config-conventional)

This project is based on the
[NCS Example Application](https://github.com/nrfconnect/ncs-example-application).

## Getting started

Before getting started, make sure you have a proper nRF Connect SDK development
environment. Follow the official
[Getting started guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/getting_started.html).

### Initialization

The first step is to initialize the workspace folder (`thingy-oob`) where the
`firmware` project and all nRF Connect SDK modules will be cloned. Run the
following command:

```shell
# initialize thingy-oob workspace
west init -m https://github.com/hello-nrfcloud/firmware --mr main thingy-oob
# update nRF Connect SDK modules
cd thingy-oob
west update
```

### Building and running

To build the application, run the following command:

```shell
west build -b thingy91_nrf9160_ns asset_tracker_v2
```

If you are using the Powerfoyle solar shield, you can enable power monitoring
capabilities by using this build command:

```shell
west build -b thingy91_nrf9160_ns asset_tracker_v2 -- -DSHIELD=powerfoyle
```

When using an external debugger, you can flash using this command:

```shell
west flash
```

Please refer to the official
[Asset Tracker V2 documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/2.4.2/nrf/applications/asset_tracker_v2/doc/asset_tracker_v2_description.html)
for details.
