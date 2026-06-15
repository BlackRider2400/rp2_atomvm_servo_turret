#!/bin/bash
set -e

mix compile

packbeam create turret.avm _build/dev/lib/turret/ebin/*.beam

uf2tool create -o turret.uf2 -f rp2040 -s 0x10180000 turret.avm

picotool reboot -f -u
sleep 3
picotool load AtomVM.uf2
picotool load atomvmlib-rp2-pico.uf2
picotool load turret.uf2
picotool reboot

sleep 2
screen /dev/ttyACM0 115200
