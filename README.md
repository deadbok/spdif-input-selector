SPDIF input selector.
=====================

Sources for the firmware used in my 
[TDA1545 NONOS DAC](http://groenholdt.net/cat_projects-TDA1545-DAC_index.html).

This firmware activates a relay in the input selector, it is basically a 
glorified rotary selector.

The button has two modes:
- Short press, advances to the next input.
- Long press, scans all inputs starting from the next input, until an active 
  input is found.

Hardware.
=========

The PIC is connected to the relays through port A. The button is connected on
port RB0. There is an additional relay to shut off data to the DAC while
switching input, connected on port RB3.
The auto search mode receives a lock signal from the SPDIF converter, then
procedes to check for actual data on the input. To do this, the LOCK signal is
connected to port RB5 and the I2S signal on RB4.
