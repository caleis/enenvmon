#!/usr/bin/python3 
#
# *********************************************************************
#
#        Blackberry Data Warehouse MQTT to SQL Database Injector Script
#
# *********************************************************************
#
#        Program name:     mqtt2db.py
#        Module name:      mqtt2db.py
#
#
#        @author           Zoltan Fekete
#        Date              19/04/2025
#
# *********************************************************************
#
#        Module description:
#        Subscribes to various topics on MQTT broker located on temperberry2 machine
#        Receives messages and insert them into various tables on BBDWH MySQL DB
#
#
#        @version          0.1.00
#        0.1 - initial version, subscribes only to EnergyMain topic
#        0.2 - subscribes also to EnergySolar and EnviroCond topics, still basic
#        0.3 - subscribes also to BoilerStats topics, setup still basic,
#              parameters hardcoded 
#
# *********************************************************************
#        Copyright (C) Zoltan Fekete 2025
#        This program is free software: you can redistribute it and/or modify
#        under the terms of the GNU General Public License version 3
#        as published by the Free Software Foundation.
#
#        This program is distributed in the hope that it will be useful,
#        but WITHOUT ANY WARRANTY; without even the implied warranty of
#        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#        GNU General Public License for more details.
#
#        You should have received a copy of the GNU General Public License
#        along with this program.  If not, see <http://www.gnu.org/licenses/>. 
# *********************************************************************
#
pgm_ver = "0.3.00"

import mysql.connector
from mysql.connector.constants import ClientFlag
import paho.mqtt.client as mqtt
import json
import datetime

#SQL CONNECTOR 
mydb = mysql.connector.connect(
  host="v.w.x.y",
  user="ssl_user",
  passwd="xxxx",
  database="bbdwh-sql",
  client_flags=[ClientFlag.SSL],
  ssl_ca="/home/ssluser/newcerts/ca.pem",
  ssl_cert="/home/ssluser/newcerts/client-cert.pem",
  ssl_key="/home/ssluser/newcerts/client-key.pem",
)
mycursor = mydb.cursor()

#ON_CONNECT 
def on_connect(mqttc, obj, flags, rc, prop):
    print("Connected  - Client: " + str(mqttc) + " Object: " + str(obj) + " Flags: " + str(flags) + " Rc: " + str(rc) + " Prop: " + str(prop))

# ON_MESSAGE 
# Example: 
#def on_message(mqttc, obj, msg):
#    print(msg.topic + " " + str(msg.qos) + " " + str(msg.payload))

def on_message(mqttc, obj, msg):
	print("On_message Topic: " + str(msg.topic) + " - Client: " + str(mqttc) + " Object: " + str(obj) + " Message: " + str(msg))
	print("           " + msg.payload.decode("utf-8"))
#TO INSERT FOR A PARTICULAR TOPIC BASED ON DB 
	if msg.topic =='EnergyMain':
		#print(msg.payload.decode("utf-8"))
		d=json.loads(msg.payload.decode("utf-8"))
		#ts = datetime.datetime.now()
		timestamp = d.get("timestamp")
		md = d.get("meterdata")
		#print("           md is ", md)
		power = md.get("power")
		reactive = md.get("reactive")
		pf = md.get("pf")
		voltage = md.get("voltage")
		is_valid = md.get("is_valid")
		total = md.get("total")
		total_returned = md.get("total_returned")
		#ts = datetime.datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%S%z")
		ts = timestamp[0:10] + ' ' + timestamp[-9:-1]
		sql = "INSERT INTO EnergyMain(TimeStamp, Power, Reactivepower, Powerfactor, voltage, Total, Totalreturned, Isvalid) VALUES (%s, %s, %s, %s, %s, %s, %s, %s)"
		val = (ts, power, reactive, pf, voltage, total, total_returned, is_valid)
		print("           sql is ",sql)
		print("           val is ",val)
		mycursor.execute(sql, val)
		mydb.commit()
		print(mycursor.rowcount, "record inserted.")
	if msg.topic =='EnergySolar':
		#print(msg.payload.decode("utf-8"))
		d=json.loads(msg.payload.decode("utf-8"))
		#ts = datetime.datetime.now()
		timestamp = d.get("timestamp")
		md = d.get("meterdata")
		#print("           md is ", md)
		power = md.get("power")
		reactive = md.get("reactive")
		pf = md.get("pf")
		voltage = md.get("voltage")
		is_valid = md.get("is_valid")
		total = md.get("total")
		total_returned = md.get("total_returned")
		#ts = datetime.datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%S%z")
		ts = timestamp[0:10] + ' ' + timestamp[-9:-1]
		sql = "INSERT INTO EnergySolar(TimeStamp, Power, Reactivepower, Powerfactor, voltage, Total, Totalreturned, Isvalid) VALUES (%s, %s, %s, %s, %s, %s, %s, %s)"
		val = (ts, power, reactive, pf, voltage, total, total_returned, is_valid)
		print("           sql is ",sql)
		print("           val is ",val)
		mycursor.execute(sql, val)
		mydb.commit()
		print(mycursor.rowcount, "record inserted.")
	if msg.topic =='EnviroCond':
		#print(msg.payload.decode("utf-8"))
		d=json.loads(msg.payload.decode("utf-8"))
		#ts = datetime.datetime.now()
		timestamp = d.get("timestamp")
		id = d.get("int")
		ed = d.get("ext")
		#print("           id is ", id)
		#print("           ed is ", ed)
		templounge = id.get("lounge")
		temphall = id.get("hall")
		tempbedroom = id.get("bedroom")
		tempdatactr = id.get("datactr")
		summary = ed.get("summary")
		tempext = ed.get("temp")
		cloud = ed.get("cloud")
		rain = ed.get("rain")
		windspeed = ed.get("windspeed")
		winddeg = ed.get("winddeg")
		pressure = ed.get("pressure")
		humidity = ed.get("humidity")
		visibility = ed.get("visibility")
		#ts = datetime.datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%S%z")
		ts = timestamp[0:10] + ' ' + timestamp[-9:-1]
		sql = "INSERT INTO EnviroCond(TimeStamp, TempLounge, TempHall, TempBedroom, TempDatactr, TempExt, Cloud, Rain, Windspeed, Winddeg, Pressure, Humidity, Visibility, Summary) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"
		val = (ts, templounge, temphall, tempbedroom, tempdatactr, tempext, cloud, rain, windspeed, winddeg, pressure, humidity, visibility, summary)
		print("           sql is ",sql)
		print("           val is ",val)
		mycursor.execute(sql, val)
		mydb.commit()
		print(mycursor.rowcount, "record inserted.")
	if msg.topic =='BoilerStats':
		#print(msg.payload.decode("utf-8"))
		d=json.loads(msg.payload.decode("utf-8"))
		#ts = datetime.datetime.now()
		timestamp = d.get("timestamp")
		periodctr = d.get("periodctr")
		ds = d.get("downstairs")
		us = d.get("upstairs")
		print("           ds is ", ds)
		print("           us is ", us)
		dsonperiod = ds.get("onperiod")
		dsoncounter = ds.get("oncounter")
		dsonpercentage = ds.get("onpercentage")
		usonperiod = us.get("onperiod")
		usoncounter = us.get("oncounter")
		usonpercentage = us.get("onpercentage")
		#ts = datetime.datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%S%z")
		ts = timestamp[0:10] + ' ' + timestamp[-9:-1]
		sql = "INSERT INTO BoilerStats(TimeStamp, PeriodCtr, DownOnperiod, DownOnctr, DownOnpercentage, UpOnperiod, UpOnctr, UpOnpercentage) VALUES (%s, %s, %s, %s, %s, %s, %s, %s)"
		val = (ts, periodctr, dsonperiod, dsoncounter, dsonpercentage, usonperiod, usoncounter, usonpercentage)
		print("           sql is ",sql)
		print("           val is ",val)
		mycursor.execute(sql, val)
		mydb.commit()
		print(mycursor.rowcount, "record inserted.")

# ON_PUBLISH 
def on_publish(mqttc, obj, mid):
    print("mid: " + str(mid))

#ON_ SUBSCRIBE
def on_subscribe(mqttc, obj, mid, rc, prop):
    print("Subscribed - Client: " + str(mqttc) + " Object: " + str(obj) + " Mid: " + str(mid) + " Rc: " + str(rc) + " Prop: " + str(prop))
#ON_LOG
def on_log(mqttc, obj, level, string):
    print(string)

# Start of program
print("MQTT2DB database data injector, version ", pgm_ver, " (C) Z.Fekete (2025)")

# If you want to use a specific client id, use
# mqttc = mqtt.Client("client-id")
# but note that the client id must be unique on the broker. Leaving the client
# id parameter empty will generate a random id for you.
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_publish = on_publish
mqttc.on_subscribe = on_subscribe
# Uncomment to enable debug messages
# mqttc.on_log = on_log
mqttc.connect("u.v.w.x", 1883, 60)
mqttc.subscribe("EnergyMain", 0)
mqttc.subscribe("EnergySolar", 0)
mqttc.subscribe("EnviroCond", 0)
mqttc.subscribe("BoilerStats", 0)

mqttc.loop_forever()


