#!/bin/bash
export JACK_NO_START_SERVER=1
source /home/julian/myenv/bin/activate
exec python3 /home/julian/sparkles/pitchdetector/aubioAlgo.py
