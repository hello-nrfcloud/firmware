.. _hello_nrfcloud_antennatuning:

Antenna Tuning
##############

The Thingy:91 X Hello nRF Cloud firmware uses the `Modem Antenna`_ library to configure the nRF9151’s MIPI RF Front-End Control Interface (RFFE) and the COEX pins.
The Modem Antenna library uses the `%XMIPIRFFEDEV`_, `%XMIPIRFFECTRL`_, and `%COEXCONFIG`_ AT commands to send the configuration to the nRF9151 modem.
The Thingy:91 X board files define the default AT commands that are sent to the modem using the following Kconfig options:

* ``CONFIG_MODEM_ANTENNA_AT_MIPIRFFEDEV``
* ``CONFIG_MODEM_ANTENNA_AT_MIPIRFFECTRL_INIT``
* ``CONFIG_MODEM_ANTENNA_AT_MIPIRFFECTRL_ON``
* ``CONFIG_MODEM_ANTENNA_AT_MIPIRFFECTRL_OFF``
* ``CONFIG_MODEM_ANTENNA_AT_MIPIRFFECTRL_PWROFF``
* ``CONFIG_MODEM_ANTENNA_AT_COEX0``

The default values for the Kconfig options used when building firmware for the Thingy:91 X target can be found in the `Thingy:91 X nRF9151 board configuration`_ file.

You can modify the default values to optimize the antenna tuning of the Thingy:91 X for specific usage scenarios.
Ignion, the manufacturer of the antenna used on the Thingy:91 X, explains in the `Ignion Antenna Technology Powering The-Nordic-Thingy 91 X`_ application note how the antenna tuning can be optimized for specific usage scenarios by adjusting the switching states of the antenna.

The antenna switch states can be set in the :file:`prj.conf` file by changing the configuration of the ``%XMIPIRFFECTRL`` AT command or during runtime by issuing the ``AT%XMIPIRFFECTRL=<new_antenna_switch_states>`` AT command.

.. important::
   Modification of the default configuration for the RF Front-End of the Thingy:91 X may result in radiated emissions outside regulatory limits.
   If you have a Thingy:91 X with a modified RF Front-End configuration, you must ensure that the operation of the modified Thingy:91 X complies with applicable regulatory requirements.
