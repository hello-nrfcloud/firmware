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
