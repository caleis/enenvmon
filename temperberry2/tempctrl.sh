#! /bin/bash
#
# This script logs internal and external climatic conditions
# For external data it used OpenWeatherMap API, so no outdoor sensors handled
# (It went from actual boiler control to just logging)
# When setting up running frequency take into account, that OpenWeatherMap API allows 100 requests per day
# So set it to max every 15 minutes from v.0.7
#
# Version history
# 0.1 initial version - very basic functionality
# 0.2 some error checking on the relay operation
# 0.3 minor bugs - in progress
# 0.4 datacenter cabinet probe added
# 0.5 lounge probe added + modified for 'transparent' shelly setup (thermostat to input, boiler to output)
# 0.6 shelly 2.5 replaced with 2x shelly 1, thermostat is not controlling inputs (so inputs not used in script)
# 0.7 controller function is now optional, external weather info from OpenWeatherMap added
# 0.8 publish temperature data to MQTT browser as well (using fifo2mqtt utility)
# 0.9 unused control functionality removed
#
# ================================================================================
# User configurable parameters
#
# Script version
declare SCRIPTVER="v0.9.00"
#
# Physical devices
#
declare SENSDEV_HALL="28.022981070000"
declare SENSDEV_LOUNGE="28.91B507D6013C"
declare SENSDEV_BEDROOM="28.A32F81070000"
declare SENSDEV_DATACTR="28.576C07D6013C"
#
# Openweathermap.org parameters
#
# Requesting current weather data for current geographic location
#
declare OWMURL="https://api.openweathermap.org/data/2.5/weather?lat=51.442724&lon=-2.613337&units=metric&appid="
declare OWMAPIKEY="<apikey>"
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
#
# Print usage function, no parameters
#
function usage ()
{
   verbosemsg "Usage: `basename $0` options (-hv)\n"
   verbosemsg "        -h: help (this text)\n"
   verbosemsg "        -v: verbose comments for interactive use [default consise log]\n"
}
#
# Getting command line parameters
#
NO_ARGS=0
E_OPTERROR=85
VERBOSE=0
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
declare CURRTIME=$(date '+%H%M')
#
# Start the script
verbosemsg "==> [$SCRIPTNAME $SCRIPTVER] @$DATE: "
compactmsg "$DATE,$SCRIPTVER,"
#
# Getting temperatures
# Lounge
declare TEMP_LOUNGE
TEMP_LOUNGE=$(cat /mnt/1wire/$SENSDEV_LOUNGE/temperature 2>/dev/null)
RESULT=$?
if [ $RESULT -ne 0 ]
then
   TEMP_LOUNGE="NA"
fi
TEMP_LOUNGE=$(echo $TEMP_LOUNGE | awk '{printf "%.1f", $1}')
#
# Hall downstairs
declare TEMP_HALL
TEMP_HALL=$(cat /mnt/1wire/$SENSDEV_HALL/temperature 2>/dev/null)
RESULT=$?
if [ $RESULT -ne 0 ]
then
   TEMP_HALL="NA"
fi
TEMP_HALL=$(echo $TEMP_HALL | awk '{printf "%.1f", $1}')
#
# Bedroom upstairs
declare TEMP_BEDROOM
TEMP_BEDROOM=$(cat /mnt/1wire/$SENSDEV_BEDROOM/temperature 2>/dev/null)
RESULT=$?
if [ $RESULT -ne 0 ]
then
   TEMP_BEDROOM="NA"
fi
TEMP_BEDROOM=$(echo $TEMP_BEDROOM | awk '{printf "%.1f", $1}')
#
# Datacenter cabinet
declare TEMP_DATACTR
TEMP_DATACTR=$(cat /mnt/1wire/$SENSDEV_DATACTR/temperature 2>/dev/null)
RESULT=$?
if [ $RESULT -ne 0 ]
then
   TEMP_DATACTR="NA"
fi
TEMP_DATACTR=$(echo $TEMP_DATACTR | awk '{printf "%.1f", $1}')
#
# Print them for debug
verbosemsg "Lounge: $TEMP_LOUNGE, Hallway: $TEMP_HALL, Bedroom: $TEMP_BEDROOM, Datacenter: $TEMP_DATACTR degrees"
compactmsg "$TEMP_LOUNGE,$TEMP_HALL,$TEMP_BEDROOM,$TEMP_DATACTR,"
#
# Getting external weather data and append it to log
declare OWMRESULT=$(/usr/bin/curl -s $OWMURL$OWMAPIKEY 2> /dev/null)
declare OWMMAIN=$(echo "$OWMRESULT" | jq '.weather[0].main' | tr -d '"')
declare OWMTEMP=$(echo "$OWMRESULT" | jq '.main.temp')
declare -i OWMCLOUDPERC=$(echo "$OWMRESULT" | jq '.clouds.all')
# rain section may not exist if no rain
declare OWMRAIN=$(echo "$OWMRESULT" | jq '.rain."1h"')
if [ $OWMRAIN = 'null' ]
then
   OWMRAIN=0
fi
declare OWMWINDSPEED=$(echo "$OWMRESULT" | jq '.wind.speed')
declare -i OWMWINDDEG=$(echo "$OWMRESULT" | jq '.wind.deg')
declare -i OWMPRESSURE=$(echo "$OWMRESULT" | jq '.main.pressure')
declare -i OWMHUMIDITY=$(echo "$OWMRESULT" | jq '.main.humidity')
declare -i OWMVISIBILITY=$(echo "$OWMRESULT" | jq '.visibility')
verbosemsg ", Weather summary: $OWMMAIN, Temp: $OWMTEMP degrees, Cloud: $OWMCLOUDPERC%, Rain: $OWMRAIN mm, Wind speed: $OWMWINDSPEED m/s, Wind dir: $OWMWINDDEG degrees, Pressure: $OWMPRESSURE mbar, Humidity: $OWMHUMIDITY%, Visibility: $OWMVISIBILITY m"
compactmsg "$OWMMAIN,$OWMTEMP,$OWMCLOUDPERC,$OWMRAIN,$OWMWINDSPEED,$OWMWINDDEG,$OWMPRESSURE,$OWMHUMIDITY,$OWMVISIBILITY"
#
# write MQTT data to MQTT_FIFO
echo "#TOPIC#EnviroCond#PAYLOAD#{\"timestamp\":\"$DATE\",\"int\":{\"lounge\":$TEMP_LOUNGE,\"hall\":$TEMP_HALL,\"bedroom\":$TEMP_BEDROOM,\"datactr\":$TEMP_DATACTR},\
\"ext\":{\"summary\":\"$OWMMAIN\",\"temp\":$OWMTEMP,\"cloud\":$OWMCLOUDPERC,\"rain\":$OWMRAIN,\"windspeed\":$OWMWINDSPEED,\"winddeg\":$OWMWINDDEG,\
\"pressure\":$OWMPRESSURE,\"humidity\":$OWMHUMIDITY,\"visibility\":$OWMVISIBILITY}}#END#" >/tmp/fifo2mqttpub_interface
#
#Finishing up
verbosemsg "\nScript completed\n"
compactmsg "\n"
exit 0

