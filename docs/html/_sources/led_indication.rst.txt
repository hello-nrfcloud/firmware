.. _thingy91_x_helloworld_firmware_about:

LED indication
##############

.. contents::
   :local:
   :depth: 2

The firmware supports multiple LED patterns to visualize the operating state of the application.
The following table describes the supported LED states:

+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| LED effect     | Color      | Meaning                                      | Duration (seconds)                                  |
+================+============+==============================================+=====================================================+
| Blinking       | Yellow     | Device is reconnecting to the LTE network    | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Blinking       | Green      | Searching for location                       | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Blinking slow  | Blue       | Device is actively polling cloud             | 10 minutes after last config update or button press |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Blinking       | Purple     | Device is downloading FOTA image             | Until the download has completed or failed          |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Solid          | Configured | Device has received an LED configuration     | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Blinking rapid | Red        | Fatal error, the device will reboot          | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
| Blinking slow  | Red        | Irrecoverable fatal error                    | NA                                                  |
+----------------+------------+----------------------------------------------+-----------------------------------------------------+
