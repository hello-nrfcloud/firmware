##########################################################################################
# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
##########################################################################################

import os
from typing import Union
import requests
from requests.exceptions import HTTPError
import sys
sys.path.append(os.getcwd())
from utils.logger import get_logger

logger = get_logger()

class HelloNrfCloudFOTA():
    def __init__(self, url: str="https://api.hello.nordicsemi.cloud/2024-04-17", timeout: int=10) -> None:
        """Initializes the class.

        :param url: Default API URL
        :param timeout: Timeout for the requests
        """
        self.url = url
        self.timeout = timeout

    def post_fota_job(self, device_id: str, type: str, fingerprint: str, bundle_id: str) -> Union[dict, None]:
        """
        Posts a new FOTA job to the specified device.

        :param device_id: The ID of the device to send the FOTA update to
        :param type: The type of FOTA update to be sent
        :param fingerprint: The fingerprint to be used in the request
        :param bundle_id: The bundle ID to be sent in the payload
        :return: The JSON response from the API or None in case of error
        """
        if type in ["delta", "full"]:
            type = "modem"
        url = f"{self.url}/device/{device_id}/fota/{type}?fingerprint={fingerprint}"

        payload = {
            "upgradePath": {
                ">=0.0.0": bundle_id
            }
        }

        try:
            response = requests.post(url, json=payload, timeout=self.timeout)

            response.raise_for_status()

            return response.json()

        except HTTPError as http_err:
            logger.error(f"HTTP error occurred: {http_err}")
        except Exception as err:
            logger.error(f"An error occurred: {err}")

        return None

    def get_fota_bundles(self, device_id: str, fingerprint: str) -> Union[dict, None]:
        """
        Retrieves the list of FOTA bundles available for the specified device.

        :param device_id: The ID of the device
        :param fingerprint: The fingerprint to be used in the request
        :return: The JSON response from the API or None in case of error
        """
        url = f"{self.url}/device/{device_id}/fota/bundles?fingerprint={fingerprint}"

        try:
            response = requests.get(url, timeout=self.timeout)
            response.raise_for_status()
            return response.json()

        except HTTPError as http_err:
            print(f"HTTP error occurred: {http_err}")
        except Exception as err:
            print(f"An error occurred: {err}")

        return None
