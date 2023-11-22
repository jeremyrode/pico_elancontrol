#!/usr/bin/env python3
ser = open('/home/jprode/ElanStatusLog.bin','rb')

x=ser.read(36)
bin_string = bin(int(x.hex(),16));
bin_string = bin_string[3:] #Cut off 0B
hex_string = x.hex()
old_bin_string = bin_string
old_hex_string = hex_string
notDone = len(x)
while notDone:
	bin_string = bin(int(x.hex(),16));
	bin_string = bin_string[3:] #Cut off 0B
	hex_string = x.hex()
	print("-----Delta Pattern-----")
	for y in range(0,len(bin_string),8):
		print("{:02d}".format(round(y/8)) + ': ',end='')
		print(hex_string[round(y/8)+1:round(y/8)+2],end=' ')
		print(bin_string[y:y+7],end=' ')
		for i in range(y,y+7):
			if len(bin_string) < i+1:
				print('X',end='')
				continue
			if bin_string[i] == old_bin_string[i]:
				print(' ',end='')
			else:
				print('-',end='')
		print(' ' + old_bin_string[y:y+7],end=' ')
		print(hex_string[round(y/8)+1:round(y/8)+2])
	old_bin_string = bin_string
	old_hex_string = hex_string
	x = ser.read(36)
	notDone = len(x)
