##########################################################################################
# Copyright (c) 2026 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import json
import os
import urllib.request


def github_request(url: str) -> dict:
    """
    Send a GET request to the GitHub REST API and return the decoded JSON body.

    Uses GITHUB_TOKEN from the environment for authentication when it is set.

    :param url: Full GitHub API URL
    :return: Decoded JSON response
    :raises urllib.error.URLError: On network or HTTP errors
    """
    headers = {"Accept": "application/vnd.github+json"}
    token = os.getenv("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def latest_release(repo: str) -> dict:
    """
    Get the latest published release of a GitHub repository.

    :param repo: Repository as "owner/name"
    :return: Release object as returned by the GitHub API
    :raises urllib.error.URLError: On network or HTTP errors
    """
    return github_request(f"https://api.github.com/repos/{repo}/releases/latest")


def download_asset(url: str, destination: str) -> None:
    """
    Download a release asset to a local file.

    Uses GITHUB_TOKEN from the environment for authentication when it is set.

    :param url: Asset download URL (browser_download_url)
    :param destination: Local file path to write to
    :raises urllib.error.URLError: On network or HTTP errors
    :raises OSError: If the destination file can't be written
    """
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
