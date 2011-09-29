#!/bin/sh

./explodomatica demo0.wav


./explodomatica --nlayers 10 --speedfactor 2 --duration 4.5 --preexplosions 0 demo1.wav  

./explodomatica --nlayers 10 --speedfactor 3 --duration 2.5 --preexplosions 0 demo2.wav  

./explodomatica --nlayers 1 --speedfactor 0.3 --duration 5 --preexplosions 2 --pre-delay 0.4 demo3.wav  

