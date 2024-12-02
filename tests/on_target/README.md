# OOB on target test

## Run test locally

NOTE: The tests have been tested on Ubuntu 22.04. For details on how to install Docker please refer to the Docker documentation https://docs.docker.com/engine/install/ubuntu/

### Setup docker
```shell
docker pull ghcr.io/hello-nrfcloud/firmware:docker-v1.0.3
cd <path_to_oob_dir>
docker run --rm -it \
  --privileged \
  -v /dev:/dev:rw \
  -v /run/udev:/run/udev \
  -v .:/work/thingy91x-oob \
  ghcr.io/hello-nrfcloud/firmware:docker-v1.0.3 \
  /bin/bash
cd thingy91x-oob/tests/on_target
```

### Verify nrfutil/jlink works
```shell
JLinkExe -V
nrfutil -V
```

### NRF91 tests
Precondition: thingy91x with segger fw on 53

Get device id
```shell
nrfutil device list
```

Set env
```shell
export SEGGER=<your_segger>
```

Additional fota and memfault envs
```shell
export IMEI=<your_imei>
export FINGERPRINT=<your_fingerprint>
```

Run desired tests, example commands
```shell
pytest -s -v -m "not slow" tests
pytest -s -v -m "not slow" tests/test_functional/test_uart_output.py
pytest -s -v -m "not slow" tests/test_functional/test_location.py::test_wifi_location
pytest -s -v -m "slow" tests/test_functional/test_fota.py::test_full_mfw_fota
```

### NRF53 tests

Precondition: thingy91x with external debugger attached

IMPORTANT: switch must be on nrf53.

Set env
```shell
export SEGGER_NRF53=<your_segger_ext_deb>
export SEGGER_NRF91=<your_segger_onboard_deb>
export UART_ID_DUT_2=<your_dut_uart_id>
```

```
pytest -s -v -m "slow" tests/test_bridge/test_serial_dfu.py::test_dfu
pytest -s -v -m "slow" tests/test_bridge/test_conn_bridge.py::test_conn_bridge
```

### PPK test

Precondition: thingy91x with segger fw on 53 and PPK setup

Set env
```shell
export SEGGER_PPK=<your_segger>
pytest -s -v -m "slow" tests/test_ppk/test_power.py::test_power
```

## Test docker image version control

JLINK_VERSION=V794i
GO_VERSION=1.20.5
