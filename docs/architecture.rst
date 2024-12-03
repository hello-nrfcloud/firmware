.. _thingy91_x_helloworld_firmware_architecture:

Firmware Architecture
#####################

.. contents::
   :local:
   :depth: 2

The firmware on the nRF9151 SiP of the Thingy:91 X is factory-programmed.
It showcases the device's functionality if you like to try out the device.
`nRF Cloud CoAP`_ is used as the cloud solution.

Overview
********

The architecture is based on state machines and `ZBUS`_.
The 12 modules communicate through the ZBUS channels instead of using the classic pattern of a :file:`main.c` file to tie the application together.
This enables you to keep the modules relatively small, maintainable and reusable.

The cloud-to-device communication is done with runtime settings.
Device-to-cloud communication uses CBOR encoded-LwM2M objects.
Each module encodes its own data using `zcbor`_.
The format is specified in CDDL files, and encoding functions are automatically generated.
The encoded data is sent to the payload channel, which takes care of forwarding it to the cloud.

Following are the 12 modules that communicate through the ZBUS channels:

Trigger module
   This is the heart of the application, which sends triggers to control other modules.
   The module handles the different states of the application.

Transport module
   This module manages the connection to `nRF Cloud CoAP`_ and notifies about the cloud connection status.
   It also forwards payloads to the cloud.

Network module
   This module wraps the `connection manager`_ subsystem and notifies about network events.

FOTA module
  This module handles FOTA using the nRF Cloud CoAP and notifies about the FOTA status.

App module
  This module manages runtime settings, wraps the `Date-Time`_ library, and notifies when time is available.

Memfault module
  This module uploads runtime stats and coredumps to `Memfault`_ for easier debugging.

Battery module
  This module wraps the `Fuel Gauge`_ subsystem and publishes battery status as payload.

Button module
  This module notifies other modules about button presses and publishes button messages as payload.

Environment module
  This module wraps the `BME68X IAQ driver`_ driver and publishes air quality, temperature, and humidity data as payload.

LED module
  This module monitors various events and displays sophisticated LED patterns.

Location module
  This module wraps the `Location`_ library, gathers and sends location data.

  .. note::
     The data is sent directly using the library rather than being repackaged in the module.

Shell module
  This module tests related shell functions and turns off UART for power saving when leaving the polling mode.

Accessing of modules with ZBUS channels
***************************************

The following table showcases the access of modules with different ZBUS channels:

+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| CHANNEL      | Battery | Environment | Location | Network | Trigger | App | Button | FOTA | Led | Memfault | Shell | Transport |
+==============+=========+=============+==========+=========+=========+=====+========+======+=====+==========+=======+===========+
| CLOUD        |         |             | R        |         | R       | R   |        | R    |     | R        |       | W         |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| CONFIG       |         |             | R        |         | R       | W   |        |      |     |          |       |           |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| LOCATION     |         |             | W        |         | R       |     |        |      | R   |          |       |           |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| BUTTON       |         |             |          |         | R       |     | W      |      |     |          | W     |           |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| FOTA         |         |             |          |         | R       |     |        | W    |     |          |       |           |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| TRIGGER      | R       | R           | R        | R       | W       |     |        |      |     |          |       |           |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| TRIGGER_MODE |         |             |          |         | W       |     |        |      | R   |          |       |           |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| NETWORK      | R       |             | R        | W       |         |     |        |      | R   |          |       | R         |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| PAYLOAD      | W       | W           |          | W       |         |     | W      |      |     |          |       | R         |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+
| TIME         | R       | R           |          | R       |         | W   |        |      | R   |          |       |           |
+--------------+---------+-------------+----------+---------+---------+-----+--------+------+-----+----------+-------+-----------+

.. note::
   The ERROR channel and channels only used internally in modules are omitted.
