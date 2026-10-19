##########################################################################################
# Copyright (c) 2026 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import json
import os
import re
import urllib.error
import urllib.request
from typing import Tuple

from utils.logger import get_logger

logger = get_logger()

ATT_REPO = "nrfconnect/Asset-Tracker-Template"
STANDARD_ATT_DFU_PATTERN = re.compile(
    r"^asset-tracker-template-(v[\d.]+)-thingy91x-nrf91-dfu\.zip$"
)


def _github_request(url: str) -> dict:
    headers = {"Accept": "application/vnd.github+json"}
    token = os.getenv("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def _latest_standard_att_dfu_asset() -> Tuple[str, str, str]:
    release = _github_request(f"https://api.github.com/repos/{ATT_REPO}/releases/latest")
    tag = release.get("tag_name", "")
    for asset in release.get("assets", []):
        name = asset["name"]
        if STANDARD_ATT_DFU_PATTERN.match(name):
            return name, tag, asset["browser_download_url"]
    raise RuntimeError(
        "No standard Thingy:91 X ATT application DFU bundle in the latest release"
    )


def _download_asset(url: str, destination: str) -> None:
    headers = {"Accept": "application/octet-stream"}
    token = os.getenv("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=300) as response, open(destination, "wb") as out:
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            out.write(chunk)


def _newest_cached_att_dfu_zip(artifacts_dir: str):
    if not os.path.isdir(artifacts_dir):
        return None
    versions = []
    for name in os.listdir(artifacts_dir):
        match = STANDARD_ATT_DFU_PATTERN.match(name)
        if match:
            version = tuple(int(part) for part in match.group(1)[1:].split(".") if part)
            versions.append((version, name))
    if not versions:
        return None
    return os.path.join(artifacts_dir, max(versions)[1])


def download_latest_standard_att_dfu_zip(artifacts_dir: str = "artifacts") -> str:
    """
    Fetch the latest standard Thingy:91 X ATT application DFU zip from GitHub releases.

    Always checks the latest release tag; downloads when the matching file is not
    already present under artifacts_dir.
    """
    try:
        name, tag, url = _latest_standard_att_dfu_asset()
    except urllib.error.URLError as err:
        cached = _newest_cached_att_dfu_zip(artifacts_dir)
        if cached:
            logger.warning("ATT release lookup failed (%s), using cached %s", err, cached)
            return cached
        raise RuntimeError(
            f"ATT release lookup failed: {err}. "
            "Set GITHUB_TOKEN if GitHub API rate limits apply."
        ) from err
    os.makedirs(artifacts_dir, exist_ok=True)
    destination = os.path.join(artifacts_dir, name)

    if os.path.isfile(destination):
        logger.info("Using cached Asset Tracker Template DFU %s (%s)", name, tag)
        return destination

    logger.info("Downloading Asset Tracker Template %s (%s)", name, tag)
    try:
        _download_asset(url, destination)
    except urllib.error.HTTPError as err:
        raise RuntimeError(
            f"Failed to download {name}: HTTP {err.code}. "
            "Set GITHUB_TOKEN if GitHub API rate limits apply."
        ) from err

    return destination
