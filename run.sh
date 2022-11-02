#!/bin/bash
sleep 5
cd  /home/jetson/ProgriX/ds-labelme/
./ds_v1.out -c platform/config/deepstream_app_config.txt 1> log.txt 2> error.txt