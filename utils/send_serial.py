#!/usr/bin/env python3
import time
import serial

ser = serial.Serial(port='/dev/ttyACM0')
print(ser.name)

ser.write(('S').encode("ascii"))
ser.write(bytes([int(6)]))
ser.write(bytes([int(4)]))


# ser.write(('S').encode("ascii"))
# ser.write(bytes([int(5)]))
# ser.write(bytes([int(0)]))
# ser.write(('S').encode("ascii"))
# ser.write(bytes([int(4)]))
# ser.write(bytes([int(0)]))
# ser.write(('S').encode("ascii"))
# ser.write(bytes([int(3)]))
# ser.write(bytes([int(0)]))
# ser.write(('S').encode("ascii"))
# ser.write(bytes([int(2)]))
# ser.write(bytes([int(0)]))
# ser.write(('S').encode("ascii"))
# ser.write(bytes([int(1)]))
# ser.write(bytes([int(0)]))
