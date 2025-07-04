#! /bin/bash
#
# This script runs fifo2mqttpub
#
# Version history
# 1.0 first version
# 1.1 delay added to start - wait for broker up, delete fifo if exist
#
#
# ================================================================================
# User configurable parameters
#
declare LOGFILE="/home/pi/logs-fifo2mqttpub/fifo2mqttpub.log"
declare EXECFILE="/home/pi/fifo2mqttpub"
declare FIFOFILE=/tmp/fifo2mqttpub_interface
declare BROKER="u.v.w.x:1883"
declare MQTT_CLIENTID="Boilerbrain_Fifo2MQTT"
#
# ================================================================================
#
if [ -e $LOGFILE ]
then
   mv $LOGFILE $LOGFILE.$(date +%Y%m%d%H%M)
fi
#
# Wait until Mosquitto broker is up
sleep 30
#
if [ -e $FIFOFILE ]
then
	rm $FIFOFILE
fi
#
$EXECFILE -d 1 -b -m $BROKER -c $MQTT_CLIENTID 2>$LOGFILE

