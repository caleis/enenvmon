#!/bin/bash
  
# turn on bash's job control
set -m
  
# Start the dbinjector process 
python3 /home/ssluser/mqtt2db.py >>/home/ssluser/mqtt2db.log

