.. _hello_nrfcloud_getting-started:

Getting Started
###############

.. contents::
   :local:
   :depth: 2

Before getting started, make sure you have a proper nRF Connect SDK development environment.
Follow the official `Getting started guide`_.

Initialization
**************

Before initializing, start the toolchain environment:

.. code-block::

   nrfutil toolchain-manager launch --

To initialize the workspace folder (:file:`hello-nrfcloud`) where the ``firmware`` project and all nRF Connect SDK modules will be cloned, run the following commands:

.. code-block::

   # Initialize hello-nrfcloud workspace
   west init -m https://github.com/hello-nrfcloud/firmware --mr main hello-nrfcloud

   cd hello-nrfcloud

   # Enable Bosch environmental sensor driver
   west config manifest.group-filter +bsec

   # Update nRF Connect SDK modules
   west update

   # Use sysbuild by default
   west config build.sysbuild True

Building and running
********************

Complete the following steps for building and running:

#. Navigate to the project folder:

   .. code-block::

      cd project

#. To build the application, run the following command:

   .. code-block::

      west build -b thingy91x/nrf9151/ns app

#. When using the serial bootloader, you can update the application using the following command:

   .. code-block::

      west thingy91x-dfu

#. When using an external debugger, you can program using the following command:

   .. code-block::

      west flash --erase

Experimental: You can also use `pyOCD`_ to flash the nRF9151 SiP using the CMSIS-DAP interface provided by the connectivity bridge firmware.

.. important::

   Do not use pyOCD with JLink probes.
   Use `nRF Util`_ or the West Runner instead.

.. note::

   Sometimes, the nRF9151 SiP is detected as protected and is mass-erased automatically.
   In that case, simply program the bootloader as well.

Programming
***********
Complete the following steps for programming:

#. To program the app, run the following command:

   .. code-block::

      pyocd flash build/app/zephyr/zephyr.signed.hex

#. To program the bootloader, run the following command:

   .. code-block::

      pyocd flash nrf91-bl-v2.hex

#. To erase the chip including UICR:

  .. code-block::

     pyocd erase --mass

Experimental: You can update the modem firmware using pyOCD.
To do this, use the included ``nrf91_flasher`` script as follows:

.. code-block::

   python3 scripts/nrf91_flasher.py -m mfw_nrf91x1_2.0.1.zip
