# wvc700r3-live-better-mppt

This repo is based on the great work from "mace" on Photovoltaic Forum:

https://www.photovoltaikforum.com/thread/209046-wvc700r3-mppt-regler-tauschen/

ATTENTION:

The instructions here in this repo are for educational purposes only. Very high voltages occur in the units. There is danger to life and body. The modifications may only be carried out by trained personnel. The modifications will invalidate all warranty claims and approvals such as certificates. We are not responsible for any damage. You are responsible for everything yourself.

INTRODUCTION:

The MPPT controller of the WVC inverters works rather moderately to poorly. Most of the time it gets stuck somewhere and the modules run in voltage ranges where you know that something can't fit. 

This project fixes these problems with the inverter "WVC-700R3-Live".

The idea was to separate the existing processor from the switching converter. The necessary signals such as current, voltage, ZCD on the board to pick up and an own MPPT controller there to install. 

A small AVR-MC which only does the MPP tracking. 

The original main processor still does the monitoring and management.

OPTIONAL:

If you have trouble with wrong voltages and/or currents in your wvc-600 / wvc-700 inverter you can try the built-in, non documented calbrating function:

https://github.com/clatoverada/wvc700r3-live-better-mppt/wiki/Calibrate-Voltages-in-the-mobile-app

BILL OF MATERIALS:

1x ATTINY85-20PU (PDIP-8)

1x Programmer for ATTINY85 Chips

1x EPCOS B72220P3271K101
