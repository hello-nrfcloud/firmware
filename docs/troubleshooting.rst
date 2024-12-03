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
