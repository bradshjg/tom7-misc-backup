#!/bin/bash

cd /home/pi/tom7misc/tempo && make tempo.exe
sudo killall tempo.exe
cd /home/pi/tom7misc/tempo && sudo nohup ./tempo.exe > /dev/null 2>&1 &
disown
