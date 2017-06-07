#!/bin/bash

# Remove other processes using the resource
#service camerapp stop
#killall -9 ct_demo

# Plugin binary
#export GST_PLUGIN_PATH=${PWD}/plugin/
export GST_PLUGIN_PATH=/home/ct1/peter-gst-ct1/plugin/


# Roseek binaries
#export LD_LIBRARY_PATH=${PWD}/ct/bin/
export LD_LIBRARY_PATH=/usr/ct_system/ctdemo_linux/bin/

# Start with debug
GST_DEBUG=*:2,GCF_APP_*:4,GCF_PLUGIN_*:4 ./bin/gst-rtsp-app

