# wvc700r3-live-better-mppt

This repo is based on the great work from "mace" on Photovoltaic Forum:
https://www.photovoltaikforum.com/thread/209046-wvc700r3-mppt-regler-tauschen/

The MPPT controller of the WVC inverters works rather moderately to poorly. Most of the time it gets stuck somewhere and the modules run in voltage ranges where you know that something can't fit. 

This project fixes these problems with the inverter "WVC-700R3-Live".

The idea was to separate the existing processor from the switching converter. The necessary signals such as current, voltage, ZCD on the board to pick up and an own MPPT controller there to install. 

A small AVR-MC which only does the MPP tracking. 

The original main processor still does the monitoring and management.
