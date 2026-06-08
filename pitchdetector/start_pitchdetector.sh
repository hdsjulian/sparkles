#!/bin/bash
export JACK_NO_START_SERVER=1
cd /home/julian/pitchdetection
git pull
source /home/julian/myenv/bin/activate
python3 /home/julian/pitchdetection/aubioAlgo.py
