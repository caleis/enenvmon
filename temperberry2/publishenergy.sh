#! /bin/bash
#
# This script reads energy consumption values from Shelly EM and publishes values to MQTT broker
# For publishing the topics to MQTT broker it relies on fifo2mqtt utility
#
# This script is supposed to run from cron in about every 1 minute. 
# (crontab setup e.g is * * * * * to run in every 10 mins.
#
# Version history
# 0.1 initial version - does the basic job
#
# ================================================================================
#
# Version
#
declare SCRIPTVER="v0.1.01"
#
# ================================================================================
#
# User configurable parameters
#
# Getting energy meter data from Shelly EM
# curl -k curl u.v.w.x/emeter/0 2> /dev/null
#
declare READ_METERMAIN_URL="v.w.x.y/emeter/0"
declare READ_METERSOLAR_URL="v.w.x.y/emeter/1"
#
# MQTT publisher FIFO interface
#
declare MQTT_FIFO="/tmp/fifo2mqttpub_interface"
#
# ================================================================================
#
# function errcho - echo to stderr (in verbose mode)
# $1: text to be sent to stderr
#
function verbosemsg ()
{
   if [ $VERBOSE -eq 1 ]
   then
      echo -n -e "$1" 1>&2
   fi
}
#
# function verbose log to stdout (for interactive mode, in in verbose mode)
# $1: text to be sent to stdout
#
function compactmsg ()
{
   if [ $VERBOSE -eq 0 ]
   then
      echo -n -e "$1"
   fi
}
#
# Print usage function, no parameters
#
function usage ()
{
   verbosemsg "Usage: `basename $0` options (-hs)\n"
   verbosemsg "        -h: help (this text)\n"
   verbosemsg "        -v: verbose comments for interactive use [default consise log]\n"
}

# Getting command line parameters
#
NO_ARGS=0
E_OPTERROR=85
VERBOSE=0
#
if [ $# -eq "$NO_ARGS" ] # Script invoked with no command-line args?
then
   verbosemsg "No command line parameters given\n"
fi

while getopts "hv" CMDOPTS
do
   case $CMDOPTS in
      h ) VERBOSE=1
          usage
          exit 0;;
      v ) VERBOSE=1;;
      * ) verbosemsg "Error: invalid command line option"
          compactmsg "E:C\n"
          exit $E_OPTERROR;
   esac
done
shift $(($OPTIND - 1))

# Getting parameters required
#
declare -i RESULT
declare SCRIPTNAME=$(basename "$0")
declare SYSNAME=$(hostname)
declare DATE=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
declare CURRTIME=$(date '+%H%M%S')

# Start the script
#
verbosemsg "==> [$SCRIPTNAME $SCRIPTVER] @$DATE: "
compactmsg "[$SCRIPTVER],"
sleep 2

# Get meter data
#
METERDATA_MAIN=$(curl -k $READ_METERMAIN_URL 2> /dev/null)
METERDATA_SOLAR=$(curl -k $READ_METERSOLAR_URL 2> /dev/null)

# write MQTT data to MQTT_FIFO
#
MQTTFIFO_MAIN="#TOPIC#EnergyMain#PAYLOAD#{\"timestamp\":\"$DATE\",\"meterdata\":$METERDATA_MAIN}#END#"
MQTTFIFO_SOLAR="#TOPIC#EnergySolar#PAYLOAD#{\"timestamp\":\"$DATE\",\"meterdata\":$METERDATA_SOLAR}#END#"

echo $MQTTFIFO_MAIN >/tmp/fifo2mqttpub_interface
echo $MQTTFIFO_SOLAR >/tmp/fifo2mqttpub_interface
verbosemsg "\n  Main Meter data:  $MQTTFIFO_MAIN\n  Solar Meter data: $MQTTFIFO_SOLAR"
compactmsg "$MQTTFIFO_MAIN,$MQTTFIFO_SOLAR"


#Finishing up
#
verbosemsg "\nScript completed\n"
compactmsg "\n"
exit 0

