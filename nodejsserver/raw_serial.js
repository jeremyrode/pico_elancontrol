"use strict";
const { SerialPort } = require('serialport');
const { ByteLengthParser } = require('@serialport/parser-byte-length')


function onData(data) {
  console.log("Got: " + data.toString('hex'));
}

//Monitor the serial port for system status
const port = new SerialPort({ path: '/dev/ttyACM0', baudRate: 14400});

const parser = port.pipe(new ByteLengthParser({ length: 1 }))
parser.on('data', function(data) {onData(data);}) // will have 8 bytes per data event

console.log("Running...")
