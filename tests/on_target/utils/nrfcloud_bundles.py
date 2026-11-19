##########################################################################################
# Copyright (c) 2026 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import io
import json
import os
import re
import zipfile
from urllib.parse import quote

import requests

NRF_CLOUD_API_URL = "https://api.nrfcloud.com/v1"
FW_VERSION_PATTERN = re.compile(r"^[a-zA-Z0-9._-]{1,60}$")


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
        :return: Bundle ID of the uploaded bundle, e.g. "APP*1a2b3c4d*<fw_version>"
        :raises ValueError: If fw_version doesn't match FW_VERSION_PATTERN
        :raises requests.HTTPError: If nRF Cloud rejects the upload, with the response body in the message
        :raises RuntimeError: If the response has no bundle ID
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

        bundle_id = response.json().get("bundleId")
        if not bundle_id:
            raise RuntimeError(f"nRF Cloud upload response has no bundleId: {response.text}")

        return bundle_id

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
