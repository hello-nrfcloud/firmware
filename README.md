# Thingy:91 X Out-of-Box Experience Application

#### Oncommit:
[![Build](https://github.com/hello-nrfcloud/firmware/actions/workflows/build.yml/badge.svg)](https://github.com/hello-nrfcloud/firmware/actions/workflows/build.yml)
[![Target tests](https://github.com/hello-nrfcloud/firmware/actions/workflows/test.yml/badge.svg)](https://github.com/hello-nrfcloud/firmware/actions/workflows/test.yml)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=hello-nrfcloud_firmware&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=hello-nrfcloud_firmware)

#### Nightly:
[![Target_tests](https://github.com/hello-nrfcloud/firmware/actions/workflows/test.yml/badge.svg?event=schedule)](https://github.com/hello-nrfcloud/firmware/actions/workflows/test.yml?query=branch%3Amain+event%3Aschedule)

This project is based on the
[NCS Example Application](https://github.com/nrfconnect/ncs-example-application).

## Getting started

Before getting started, make sure you have a proper nRF Connect SDK development
environment. Follow the official
[Getting started guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/getting_started.html).

### Initialization

Before initializing start the toolchain environment:

```shell
nrfutil toolchain-manager launch --shell
```

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

First change folder:

```shell
cd project
```

To build the application, run the following command:

```shell
west build -b thingy91x/nrf9151/ns app
```

When using the serial bootloader, you can update using this command:

```
west thingy91x-dfu
```

When using an external debugger, you can flash using this command:

```shell
west flash --erase
```

### LED pattern

| LED effect     | Color      | Meaning                                      | Duration (seconds)                                  |
|----------------|------------|----------------------------------------------|-----------------------------------------------------|
| Blinking       | Yellow     | Device is (re-)connecting to the LTE network | NA                                                  |
| Blinking       | Green      | Location searching                           | NA                                                  |
| Blinking slow  | Blue       | Device is actively polling cloud             | 10 minutes after last config update or button press |
| Solid          | Configured | Device has received a LED configuration      | NA                                                  |
| Blinking rapid | Red        | Fatal error, the device will reboot          | NA                                                  |
| Blinking slow  | Red        | Irrecoverable Fatal error                    | NA                                                  |

### Modem Traces

Modem traces are enabled by default on the Thingy:91 device. These traces can be output to UART for analysis using the **nRF Connect for Desktop Cellular Monitor** application.

#### Steps to Capture and Dump Modem Traces:

1. **Connect to a Serial Terminal**
   - Connect your Thingy:91 device to a serial terminal on **UART 0**. This will allow you to interact with the device's shell commands. You might need to push **Button 1** to wake the UART up.

2. **Set Up Cellular Monitor Application**
   - Open the [Cellular Monitor Application](https://docs.nordicsemi.com/bundle/nrf-connect-cellularmonitor/page/index.html).
   - Connect the Thingy:91 to the application, select **UART 1** as the trace output, and click **Start Traces** to begin capturing modem activity.

3. **Dump Traces via UART**
   - Use the following shell commands in the connected serial terminal to manage and dump the modem traces on **UART 1**:

   ```shell
   modem_trace stop         # Stop modem tracing if running
   modem_trace size         # Check the size of stored traces
   modem_trace dump_uart    # Dump traces to UART 1 for analysis
