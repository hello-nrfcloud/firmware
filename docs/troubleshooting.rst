.. _thingy91_x_helloworld_firmware_troubleshooting:

Troubleshooting
###############

.. contents::
   :local:
   :depth: 2

The following section contains guidelines on how to troubleshoot issues with the device.

Modem traces
************

Modem traces are enabled by default on the Thingy:91 X and continuously stored to external flash.
You can output these traces to UART for analysis using the `Cellular Monitor app`_.

Complete the following steps to capture and dump modem traces:

#. Connect to a serial terminal.

   a. Connect your Thingy:91 X to a serial terminal on UART 0.
      This allows interaction with the device’s shell commands.
      You might need to push **Button 1** to wake the UART.

#. Set up modem tracing.

   a. Open the `Cellular Monitor app`_.
   #. Connect the Thingy:91 X to the application, select UART 1 as the trace output, and click :guilabel:`Start Traces` to begin capturing modem activity.

#. Dump traces through UART.

   a. Ensure that the error scenario has been captured.
      The older traces will be overwritten by new ones when the flash buffer is full.
      So, ensure that the issue you want to troubleshoot has occurred relatively close to the dumping of the traces over UART.
   #. Use the following shell commands in the connected serial terminal to manage and dump the modem traces on UART 1:

      .. code-block:: none

          modem_trace stop         # Stop modem tracing if running
          modem_trace size         # Check the size of stored traces
          modem_trace dump_uart    # Dump traces to UART 1 for analysis

Factory reset (Bootloader)
**************************

If you have not used a debug probe to program your Thingy:91 X, and it became unresponsive, it is possible to recover the device without additional tools.

See `Installing nRF Util`_ for instructions for installing nRF Util. Install the nRF Util device command as well.
Download the following files from the hello-nrfcloud/firmware release:

* `nRF5340 DFU`_
* `nRF9151 DFU`_

If the device shows up on USB, the nRF5340 is correctly running the Connectivity bridge.
If not, you can try to put the nRF5340 into serial recovery mode.
This is done by turning off the device using the power switch, and pressing and holding **Button 2** while turning it back on.
An MCUboot device shows up.

.. code-block:: none

   nrfutil device program --firmware connectivity-bridge-v2.0.1-thingy91x-nrf53-dfu.zip --traits mcuboot

Then, the Connectivity bridge triggers serial recovery mode for the nRF9151 automatically.

Alternatively, you can force the nRF9151 serial recovery mode the same way with the nRF5340, but using **Button 1** instead.

.. code-block:: none

   nrfutil device program --firmware hello.nrfcloud.com-v2.0.1-thingy91x-nrf91-dfu.zip --traits mcuboot

Factory reset (J-Link)
**********************

If your Thingy:91 X is powered on and connected using a USB cable, it shows up as a USB device.
Else, something is preventing the Connectivity bridge application on the nRF5340 from running.
For example, this could be an application running on the nRF9151 that is driving the wrong pins.
Another typical case is that the nRF5340 has been flashed by accident.

You need to use an external J-Link to follow these steps.
A Nordic development kit with a debug-out port can be used as well.

See `Installing nRF Util`_ for instructions for installing nRF Util. Install the nRF Util device command as well.
Download the following files from the hello-nrfcloud/firmware release:

* `nRF5340 application firmware`_
* `nRF5340 network firmware`_
* `nRF9151 firmware`_

Set the SWD switch to **nRF91** and run the ``nrfutil device recover`` command to recover the nRF9151.
Repeat the same for the nRF5340. You can use the ``nrfutil device device-info`` command to check that you are connected to the correct chip.
When using a DK, there is an ambiguity between targeting the on-board chip and using debug-out.
Running the ``nrfutil device device-info`` command is recommended to make sure you are not programming the on-board target accidentally.

If nRF5340 recovery fails, an nRF9151 firmware might have been flashed previously.
Even though the device shows up as unrecoverable, the soft-lock can be removed with these commands.
You might have to repeat them for a successful result.
Do not use them on an nRF91 Series device.

.. code-block:: none

   nrfutil device recover --x-family nrf53 --core Network --traits jlink
   nrfutil device recover --x-family nrf53 --core Application --traits jlink

Now that both chips are recovered, set the SWD switch to **nRF91**.
To check you are connected to the correct chip, run the ``nrfutil device device-info`` command.

.. code-block:: none

   nrfutil device program --firmware hello.nrfcloud.com-v2.0.1+debug-thingy91x-nrf91.hex

Set the SWD switch to **nRF53**.
To check you are connected to the correct chip, run the ``nrfutil device device-info`` command.

.. code-block:: none

   nrfutil device program --core Network --options reset=RESET_NONE,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE --firmware connectivity-bridge-v2.0.1-thingy91x-nrf53-net.hex
   nrfutil device program --options reset=RESET_SYSTEM,chip_erase_mode=ERASE_ALL,verify=VERIFY_NONE --firmware connectivity-bridge-v2.0.1-thingy91x-nrf53-app.hex

The device shows up as a USB device again.
Press **Button 2** to reset the nRF9151 DK.
If you see that **LED 1** lights up, the recovery was successful.
If you do not have a usable device after this procedure, contact Nordic Semiconductor's technical support on `DevZone`_.
