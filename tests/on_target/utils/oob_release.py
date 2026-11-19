##########################################################################################
# Copyright (c) 2026 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import os
import types

from utils.github_release import download_asset, latest_release
from utils.logger import get_logger

logger = get_logger()

OOB_REPO = "hello-nrfcloud/firmware"


def download_latest_release_hex(artifacts_dir: str) -> types.SimpleNamespace:
    """
    Download the nRF91 application hex of the latest hello.nrfcloud.com firmware release.

    The file is reused if it already exists in artifacts_dir.
    Keep artifacts_dir separate from the CI build artifacts, since the release hex
    has the same naming scheme as the firmware under test.

    :param artifacts_dir: Directory to store the hex file in
    :return: Namespace with `path` (local hex file) and `tag` (release tag, e.g. "v3.0.1")
    :raises urllib.error.URLError: If the release can't be looked up or downloaded
    :raises RuntimeError: If the latest release has no nRF91 application hex
    """
    release = latest_release(OOB_REPO)
    tag = release["tag_name"]
    hex_name = f"hello.nrfcloud.com-{tag}-thingy91x-nrf91.hex"

    asset = next((a for a in release.get("assets", []) if a["name"] == hex_name), None)
    if asset is None:
        raise RuntimeError(f"Release {tag} of {OOB_REPO} has no asset named {hex_name}")

    os.makedirs(artifacts_dir, exist_ok=True)
    destination = os.path.join(artifacts_dir, hex_name)

    if os.path.isfile(destination):
        logger.info(f"Using cached release hex {destination}")
    else:
        logger.info(f"Downloading {hex_name} from release {tag}")
        download_asset(asset["browser_download_url"], destination)

    return types.SimpleNamespace(path=destination, tag=tag)
