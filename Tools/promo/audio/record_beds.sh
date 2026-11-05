#!/bin/sh
# Every game-audio bed the mix asks for, in place, in a sound instance: python Tools/promo/capture.py boot --fps 0 --res 1920x1080 --sound, then this. About 10 minutes. Positions are seed 777's.
C="python Tools/promo/capture.py"
$C bed shore_dawn --seconds 30 --at -300 -700 760 --clock 6 30 --settle 15
$C bed meadow_dawn --seconds 30 --at 200 900 760 --clock 6 45 --settle 10
$C bed sea --seconds 30 --at -5000 -5000 800 --clock 7 15 --settle 18
$C bed forest_day --seconds 30 --at 20000 140000 900 --clock 15 30 --settle 25
$C bed wind_high --seconds 30 --at -1279880 1040000 44000 --clock 16 30 --settle 30
$C bed rainforest --seconds 30 --at 195000 -5000 900 --clock 12 0 --settle 25
$C bed lake --seconds 30 --at 448288 545741 9300 --clock 10 30 --settle 25
$C bed cave --seconds 30 --at -203506 425574 1620 --clock 13 0 --settle 25
$C run "MO.Player.Teleport -200 1000 760"
sleep 15
$C run "MO.Fire.SpawnLit"
$C bed fire --seconds 30 --clock 18 40 --settle 3
$C bed fire_night --seconds 30 --clock 22 0 --settle 3
