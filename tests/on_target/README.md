# OOB on target test

## Run test locally

### Setup docker
```shell
docker pull ghcr.io/hello-nrfcloud/firmware:v2.0.0-preview55
cd <path_to_oob_dir>
west build -p -b thingy91x/nrf9151/ns app
cp build/merged.hex tests/on_target/artifacts/hello.nrfcloud.com-aaa000-thingy91x-nrf91.hex
docker run --rm -it \
  --privileged \
  -v /dev:/dev:rw \
  -v /run/udev:/run/udev \
  -v .:/work/thingy91x-oob \
  ghcr.io/hello-nrfcloud/firmware:v2.0.0-preview55 \
  /bin/bash
```

### Verify nrfutil/jlink works
```shell
JLinkExe -V
nrfutil -V
```

### Install requirements
```shell
cd thingy91x-oob/tests/on_target
pip install -r requirements.txt --break-system-packages
```

### Run UART tests

Precondition: thingy91x with segger fw on 53

```shell
export SEGGER=<your_segger>
pytest -s -v -m "dut1 and uart" tests
```

### Run FOTA tests

Precondition: thingy91x with segger fw on 53

```shell
export SEGGER=<your_segger>
export IMEI=<your_imei>
export FINGERPRINT=<your_fingerprint>
pytest -s -v -m "dut1 and fota" tests
```

### Run DFU tests

Precondition: thingy91x with external debugger attached

IMPORTANT: switch must be on nrf53.

Set all this bunch of env variables appropriately (see worflow for details):
SEGGER_NRF53, SEGGER_NRF91, UART_ID, NRF53_HEX_FILE, NRF53_APP_UPDATE_ZIP,
NRF53_BL_UPDATE_ZIP, NRF91_HEX_FILE, NRF91_APP_UPDATE_ZIP, NRF91_BL_UPDATE_ZIP

```
pytest -s -v -m dut2 tests
```

## Test docker image version control

JLINK_VERSION=V794i
GO_VERSION=1.20.5
