# SyncSep

SyncSep.c
Joshua Reich / josh@i2pi.com / @i2pi
July 2020.

This program is designed to run on the low-cost 8-pin, 8-bit, 8 MHz,
PIC12HV752 MCU. 

It takes in the Luma/Sync (green-cable) signal from a 1080p component video
signal, and outputs:

  - H Sync      -- HIGH for each incoming h-sync pulse 
  - V Sync      -- HIGH for the first v-sync pulse
  - Line Gate   -- HIGH for the duration of the visible analog video content

This is similar to the function of an LM1881, but works for 1080p component
video, instead of NTSC/PAL composite video. 

Pinout:

                    Vdd  1.    8  GND
       RA5 / V Sync OUT  2     7  COG1OUT1 / Line Gate Out
     C1IN1- / Signal IN  3     6  NC
                     NC  4     5  C1OUT / H Sync Out
