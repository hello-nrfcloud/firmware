.. _thingy91_x_helloworld_firmware:

Thingy:91 X: Hello nRF Cloud firmware
#####################################

The Hello nRF Cloud firmware is the official, factory-programmed firmware for the nRF9151 SiP on the Thingy:91 X device.

The application search for and connects to an LTE network and sends network and sensor data to nRF Cloud, using CoAP over UDP as transport protocol.
Sampled data that is sent from the application, can be viewed in the `hello.nrfcloud.com`_ web interface.

Some of the key features of the firmware include:

* LTE network connection management
* Secure and efficient communication with nRF Cloud using CoAP with DTLS connection ID over UDP
* Sensor data collection and transmission
* Firmware-Over-the-Air (FOTA) capability for application and modem updates
* Modem tracing capability
* LED indication for visualizing the operating state of the application

The project repository is based on the `NCS Example Application`_ and customized to the needs of the application.
Read more about the various aspects of the firmware in the following sections.

.. toctree::
   :maxdepth: 2
   :caption: Subpages:

   about
   gsg_guide
   architecture
   troubleshooting
   known_issues
   release_notes
