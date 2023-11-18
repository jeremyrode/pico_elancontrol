"use strict";
const { SerialPort } = require('serialport');
const { DelimiterParser } = require('@serialport/parser-delimiter');

const ELAN_POWER = 0;
const ELAN_VOLUP = 4;
const ELAN_VOLDOWN = 36;

let onStatusCheck; //Interval that checks if system is still on

// Status Object
let status = {
  volume: [0,0,0,0,0,0],
  mute: [0,0,0,0,0,0],
  input: [0,0,0,0,0,0],
  on: false
};

//Translate binary status to our status variables
function extractData(sdata) {
  const cstatus = {
    volume: [0,0,0,0,0,0],
    mute: [0,0,0,0,0,0],
    input: [0,0,0,0,0,0],
    on: true
  };
  for (let i = 0; i < 6; i++) {
    cstatus.volume[i] = 48 - sdata[i*6+2] & 0b00111111; //Volume is sent as a 6-bit attenuation value in LSBs
    cstatus.mute[i] = (sdata[i*6] & 0b00010000) >>> 4; // Mute is just a bit
    cstatus.input[i] = sdata[i*6] & 0b00000111; // Status is 3 LSBs
  }
  return cstatus;
}

//The serial data is broadcast regardless of status change, see if data is different
function onData(sdata) {
  switch (sdata[sdata.length-1]) {
    case 0xEA:
      var new_data = extractData(sdata); // Get the new status
      status = new_data; //Store new status
      console.log("Got Status: " + sdata.toString('hex'));
      console.log(status);
      break;
    case 0xEF:
      console.log("Error From Pico: " + sdata.toString('utf8',0,sdata.length-2));
      break;
    default:
      console.log("Invalid Footer from Pico");
  }

}


function send_zpad_command_serial(zone, channel) {
  port.port.write(Buffer.from('S' + zone + channel,'ascii'), function(err) {
    if (err) {
      return console.log("Error on write: ", err.message);
    }
  });
}


//Monitor the serial port for system status
const port = new SerialPort({ path: '/dev/ttyACM0', baudRate: 14400});

const parser = port.pipe(new DelimiterParser({ delimiter: Buffer.from('E0C00081','hex') }));

parser.on('data', function(data) {onData(data);});



//When the system is off, we get no serial data
function isOn() {
  if (Date.now() - last_update > 2000) { //If no serial data for 2s, we off
    //console.log('System is Off');
    status.volume = [0,0,0,0,0,0];
    status.mute = [0,0,0,0,0,0];
    status.input = [0,0,0,0,0,0];
    status.on = false;
    clearInterval(onStatusCheck); //No need to check anymore we're off
  }
}
