#!/usr/bin/env python3
import time
import serial

ser = serial.Serial(port='/dev/ttyACM0')
print(ser.name)

while True:
    print("Got: ", ser.read().hex() )
