##########################################################################################
# Copyright (c) 2026 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import io
import json
import os
import re
import time
import zipfile
from typing import Iterator, Optional
from urllib.parse import quote

import requests

NRF_CLOUD_API_URL = "https://api.nrfcloud.com/v1"
FW_VERSION_PATTERN = re.compile(r"^[a-zA-Z0-9._-]{1,60}$")
APP_BUNDLE_ID_PATTERN = re.compile(r"^APP\*[0-9a-zA-Z]{8}\*.+$")
BUNDLE_LOOKUP_TIMEOUT_S = 300
BUNDLE_LOOKUP_INTERVAL_S = 5


def _raise_for_status(response: requests.Response) -> None:
    if not response.ok:
        raise requests.HTTPError(
            f"{response.status_code} {response.reason} for {response.request.method} "
            f"{response.url}: {response.text}",
            response=response,
        )


class NrfCloudFirmwareBundles():
    """
    Uploads and deletes firmware bundles in an nRF Cloud account.

    Uses the same bundle format as .github/workflows/register-firmware-bundles.mjs.
    """

    def __init__(self, api_key: str, timeout: int = 60) -> None:
        """
        :param api_key: nRF Cloud API key of the account the FOTA test device belongs to
        :param timeout: Timeout in seconds for each request
        """
        self.api_key = api_key
        self.timeout = timeout

    def upload_app_bundle(self, update_bin_path: str, fw_version: str) -> str:
        """
        Zip an application update image with a manifest and upload it as a firmware bundle.

        :param update_bin_path: Path to the signed application update image (*-update-signed.bin)
        :param fw_version: Firmware version to give the bundle, must be unique in the account and
                           match FW_VERSION_PATTERN (1-60 characters of a-z, A-Z, 0-9, '.', '_', '-')
        nRF Cloud processes uploads asynchronously and doesn't return the bundle ID, so this
        waits until a bundle with fw_version and an ID matching APP_BUNDLE_ID_PATTERN appears in
        the account's firmware list. While processing, the list can hold an entry for fw_version
        whose ID is a bare UUID, which hello.nrfcloud.com rejects in FOTA jobs.

        :return: Bundle ID of the uploaded bundle, e.g. "APP*1a2b3c4d*<fw_version>"
        :raises ValueError: If fw_version doesn't match FW_VERSION_PATTERN
        :raises requests.HTTPError: If nRF Cloud rejects the upload or the firmware list request,
                                    with the response body in the message
        :raises TimeoutError: If the bundle doesn't show up in the firmware list within
                              BUNDLE_LOOKUP_TIMEOUT_S. The bundle may still exist and must then be
                              deleted by version.
        :raises OSError: If the update image can't be read
        """
        if not FW_VERSION_PATTERN.fullmatch(fw_version):
            raise ValueError(f"fw_version '{fw_version}' doesn't match {FW_VERSION_PATTERN.pattern}")

        file_name = os.path.basename(update_bin_path)
        manifest = {
            "name": f"hello.nrfcloud.com {fw_version}",
            "description": "On-target test build of the Thingy:91 X firmware",
            "fwversion": fw_version,
            "format-version": 1,
            "files": [
                {
                    "file": file_name,
                    "type": "application",
                    "size": os.path.getsize(update_bin_path),
                }
            ],
        }

        bundle_zip = io.BytesIO()
        with zipfile.ZipFile(bundle_zip, "w", zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("manifest.json", json.dumps(manifest, indent=2))
            archive.write(update_bin_path, file_name)

        response = requests.post(
            f"{NRF_CLOUD_API_URL}/firmwares",
            headers={
                "Authorization": f"Bearer {self.api_key}",
                "Content-Type": "application/zip",
            },
            data=bundle_zip.getvalue(),
            timeout=self.timeout,
        )
        _raise_for_status(response)

        deadline = time.monotonic() + BUNDLE_LOOKUP_TIMEOUT_S
        while True:
            bundle_id = self.find_bundle_id(fw_version)
            if bundle_id:
                return bundle_id
            if time.monotonic() > deadline:
                pending_ids = [item.get("bundleId") for item in self.list_bundles()
                               if item.get("version") == fw_version]
                raise TimeoutError(
                    f"No bundle with version {fw_version} and an APP bundle ID in the nRF Cloud "
                    f"firmware list {BUNDLE_LOOKUP_TIMEOUT_S} s after upload, "
                    f"IDs listed for this version: {pending_ids}"
                )
            time.sleep(BUNDLE_LOOKUP_INTERVAL_S)

    def find_bundle_id(self, fw_version: str) -> Optional[str]:
        """
        Look up the bundle ID of an application firmware bundle by its version.

        :param fw_version: Firmware version the bundle was uploaded with
        :return: Bundle ID matching APP_BUNDLE_ID_PATTERN, or None if the account has no such
                 bundle with this version yet
        :raises requests.HTTPError: If the firmware list request fails, with the response body in the message
        """
        for item in self.list_bundles():
            bundle_id = item.get("bundleId") or ""
            if item.get("version") == fw_version and APP_BUNDLE_ID_PATTERN.fullmatch(bundle_id):
                return bundle_id
        return None

    def list_bundles(self) -> Iterator[dict]:
        """
        List all firmware bundles in the account, following pagination.

        :return: Iterator over the ListFirmware items (dicts with "bundleId", "version", "type", ...)
        :raises requests.HTTPError: If a firmware list request fails, with the response body in the message
        """
        params = {"pageLimit": 100}
        while True:
            response = requests.get(
                f"{NRF_CLOUD_API_URL}/firmwares",
                headers={"Authorization": f"Bearer {self.api_key}"},
                params=params,
                timeout=self.timeout,
            )
            _raise_for_status(response)
            page = response.json()

            yield from page.get("items", [])

            next_token = page.get("pageNextToken")
            if not next_token:
                return
            params["pageNextToken"] = next_token

    def delete_bundle(self, bundle_id: str) -> None:
        """
        Delete a firmware bundle.

        :param bundle_id: Bundle ID returned by upload_app_bundle()
        :raises requests.HTTPError: If nRF Cloud rejects the deletion, with the response body in the message
        """
        response = requests.delete(
            f"{NRF_CLOUD_API_URL}/firmwares/{quote(bundle_id, safe='')}",
            headers={"Authorization": f"Bearer {self.api_key}"},
            timeout=self.timeout,
        )
        _raise_for_status(response)
