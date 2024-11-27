.. _thingy91_x_helloworld_firmware_about:

About Thingy:91 X: Hello nRF Cloud firmware
###########################################

.. contents::
   :local:
   :depth: 1

This project is based on the `NCS Example Application`_.

.. to do intro

LED indication
**************

The firmware supports multiple LED patterns to visualize the operating state of the application.
The following table describes the supported LED states:

+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| LED effect     | Color      | Meaning                                      | Duration (seconds)                                  |
+================+============+==============================================+=====================================================+
| Blinking       | Yellow     | Device is (re-)connecting to the LTE network | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Blinking       | Green      | Location searching                           | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Blinking slow  | Blue       | Device is actively polling cloud             | 10 minutes after last config update or button press |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Solid          | Configured | Device has received a LED configuration      | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Blinking rapid | Red        | Fatal error, the device will reboot          | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Blinking slow  | Red        | Irrecoverable Fatal error                    | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+

Interactive bill of materials (BOM)
***********************************

You can explore the interactive BOM for the Thingy:91 X board online on `Interactive BOM PCA20065 1.0.0.`_
