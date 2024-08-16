# OOB on target test

## Run test locally

### Setup docker
```shell
docker pull ghcr.io/hello-nrfcloud/firmware:v2.0.0-preview31
cd <path_to_oob_dir>
cp <path_to_hex>/merged.hex thingy91x-oob/tests/on_target/artifacts/
docker run --rm -it \
  --privileged \
  -v /dev:/dev:rw \
  -v /run/udev:/run/udev \
  -v <abspath_to_oob_dir>:/work/thingy91x-oob \
  ghcr.io/hello-nrfcloud/firmware:v2.0.0-preview31 \
  /bin/bash
```

### Verify nrfutil/jlink works
```shell
nrfutil -V
nrfutil device list
```

### Run test
```shell
cd thingy91x-oob/tests/on_target
pip install -r requirements.txt --break-system-packages
export SEGGER=<your_segger>
pytest -s -v -m dut1 tests --firmware-hex artifacts/merged.hex
```

## Test docker image version control

JLINK_VERSION=V794e
