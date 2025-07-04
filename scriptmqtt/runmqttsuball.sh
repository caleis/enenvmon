#! /bin/bash
#
# This script runs mosquitto_sub to subscribe to all MQTT submissions (all topics)
#
# Version history
# 1.0 first version
#
#
# ================================================================================
# User configurable parameters
#
declare LOGFILE="/home/pi/logs-fifo2mqttpub/mqttsuball.log"
#
# ================================================================================
#
if [ -e $LOGFILE ]
then
   mv $LOGFILE $LOGFILE.$(date +%Y%m%d%H%M)
fi
#
sleep 60
/usr/bin/mosquitto_sub -h localhost -t "#" -v >> $LOGFILE &

