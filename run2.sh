#!/bin/bash

cd  /home/jetson/Mikhail/DataReceiver/
export PYTHONPATH="$PYTHONPATH:/home/jetson/.local/lib/python3.6/site-packages"
python3 InferResultsServer.py > /dev/null 2> /home/jetson/ProgriX/ds-labelme/server.txt