.. _thingy91_x_helloworld_firmware_known_issues:

Known issues
############

.. contents::
   :local:
   :depth: 3

The following section contains Known issues related to the Hello nRF Cloud firmware.

v2-0-1 v2-0-0

Serial DFU stops working after using GNU screen
   Serial DFU for nRF91 stops working after using GNU screen.
   This will be fixed with the next nRF Connect SDK release (`sdk-nrf PR #19024`_).

  **Workaround:** Use a different serial terminal application, for example, `Serial Terminal app`_, ``tio`` or, power cycle device, if needed.

  **Affected platforms:** Thingy:91 X

v2-0-0 v2-0-1

FOTA image downloads may fail to resume correctly after a network disconnection
   FOTA image downloads may fail to resume correctly after a network disconnection.
   The downloader may use the wrong byte offset when requesting file fragments, which results in a corrupted image being downloaded.
   The image will be rejected by the bootloader, and will not harm the device.
   After a failed download, the device will attempt to download the image again.

  **Workaround:** Disable the ``CONFIG_NRF_CLOUD_COAP_DOWNLOADS`` Kconfig option in the application configuration.

  **Affected platforms:** Thingy:91 X
