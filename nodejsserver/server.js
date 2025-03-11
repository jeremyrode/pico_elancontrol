"use strict";
const fs = require('fs');
const { SerialPort } = require('serialport');
const { DelimiterParser } = require('@serialport/parser-delimiter');

// Optional. You will see this name in eg. 'ps' or 'top' command
process.title = 'elan-websocket-pico';
// Port where we'll run the websocket server
const webSocketsServerPort = 1338;
const LOG_FILE = '/home/jprode/ElanControlLog.txt';
const STATUS_LOG = '/home/jprode/ElanStatusLog.bin';
const ELAN_POWER = 0;
const ELAN_VOLUP = 4;
const ELAN_VOLDOWN = 36;
// websocket and http servers
const webSocketServer = require('websocket').server;
const http = require('http');
const logfile = fs.createWriteStream(LOG_FILE, {flags:'a'});
const commandlogfile = fs.createWriteStream(STATUS_LOG);
// Status Object
let status = {
  volume: [0,0,0,0,0,0],
  mute: [0,0,0,0,0,0],
  input: [0,0,0,0,0,0],
  on: false
};
let last_update = 0; //Timestamp of last serial update
const clients = []; //Who's connected
let onStatusCheck; //Interval that checks if system is still on

//  HTTP server
const server = http.createServer(function(request, response) {
    combinedLog('Received request for ' + request.url);
    response.writeHead(404);
    response.end();
});
server.listen(webSocketsServerPort, function() {
	combinedLog("Server is listening on port " + webSocketsServerPort);
});
// WebSocket server
const wsServer = new webSocketServer({
	httpServer: server
});
// Websocket request
wsServer.on('request', function(request) {
	// combinedLog('Connection from origin ' + request.remoteAddress);
	const connection = request.accept(null, request.origin);
	// we need to know client index to remove them on 'close' event
	let index = clients.push(connection) - 1;
	connection.sendUTF(JSON.stringify(status)); //Send the current status to new clients
	connection.on('message', function(message) {onClientMessage(message);});
	// user disconnected (this doesn't get all disconnects)
	connection.on('close', function(reasonCode, description) {
		// remove user from the list of connected clients
		clients.splice(index, 1);
	});
});
// Wesocket Message
function onClientMessage(message) {
  if (message.type === 'utf8') {
    const commnds = message.utf8Data.split(":"); //Yes, I should have used JSON
    if (commnds.length == 2) { // one colon, it's a straight command
      const command = parseInt(commnds[0]);
      const zone = parseInt(commnds[1]);
      if (command < 0 || command > 63 || zone < 1 || zone > 6) {
        combinedLog('malformatted command: ' + message.utf8Data);
        return;
      }
      send_zpad_command_serial(zone, command); // Send command
    } else if (commnds.length == 3) { //Two colons, it's a slider command
      //console.log('Got slider command: ' + message.utf8Data);
      const zone = parseInt(commnds[1]);
      let desired_vol = parseInt(commnds[2]);
      if (desired_vol < 0 || desired_vol > 48 || zone < 1 || zone > 6) {
        combinedLog('malformatted slider command: ' + message.utf8Data);
        return;
      }
      if (desired_vol > 33) { //Lets limit someone from flooring the vol via slider
        desired_vol = 33;
      }
      send_slider_command_serial(zone,removeMissingCodes(desired_vol));
    } else {
      combinedLog('malformatted # of colons: ' + message.utf8Data);
    }
  } else {
    combinedLog('Got non-utf8 command from client: ' + message);
  }
}

//Translate binary status to our status variables
function extractData(sdata) {
  const cstatus = status;
  for (let i = 0; i < 6; i++) {
    cstatus.volume[i] = sdata[3*i]; //Volume is sent as a 6-bit attenuation value in LSBs
    cstatus.mute[i] = sdata[3*i+1]; // Mute is just a bit
    cstatus.input[i] = sdata[3*i+2]; // Status is 3 LSBs
  }
  return cstatus;
}
//The serial data is broadcast regardless of status change, see if data is different
function onData(sdata) {
  switch (sdata[sdata.length-1]) { //Footer tells us what kind of message
    case 0xEA: //STATUS Message
      last_update = Date.now(); //We are getting data, note the time
      if (status.on == false) { //If we have differing data, we turned on!
        status.on = true; // We're on now!
        onStatusCheck = setInterval(isOn,10000); // Check to see if we're off
      }
      status = extractData(sdata); // Get the new status
      updateClients();
      break;
    case 0xEF: //Error Log
      combinedLog("Pico: " + sdata.toString('utf8',0,sdata.length-2));
      break;
    default:
      combinedLog("Invalid Footer from Pico");
  }
}
//Send updates to connected clients, remove if not connected (Andriod)
function updateClients() {
  const json_status = JSON.stringify(status); // encode new status
  for (let i=0; i < clients.length; i++) { //for all connected clients
    if (clients[i].connected) { //only if client is connected
      clients[i].sendUTF(json_status); //Send the status to clients
    }
    else {
      clients.splice(i, 1); // Remove the client if not connected
    }
  }
}
//When the system is off, we get no keepalives from the pico
function isOn() {
  if (Date.now() - last_update > 10000) { //If no serial data for 10s, we off
    console.log('System is Off');
    status.volume = [0,0,0,0,0,0];
    status.mute = [0,0,0,0,0,0];
    status.input = [0,0,0,0,0,0];
    status.on = false;
    updateClients();
    clearInterval(onStatusCheck); //No need to check anymore we're off
  }
}
//Monitor the serial port for system status
const port = new SerialPort({ path: '/dev/ttyACM0', baudRate: 14400 });
//Parse with the header (same as the Elan, why not)
const parser = port.pipe(new DelimiterParser({ delimiter: Buffer.from('E0C00081','hex') }));
//Call onData when we get a header
parser.on('data', function(data) {onData(data);});
//Request the pico to send a zPad command
function send_zpad_command_serial(zone, command) {
  combinedLog('Sending Zpad zone: ' + zone + ' and command: ' + command);
  port.write([67, zone, command], function(err) {
    if (err) {
      combinedLog("Error on serial write: " + err.message);
      return;
    }
  });
}
//Request the pico to handle a slider
function send_slider_command_serial(zone, vol) {
  port.write([68, zone, vol], function(err) {
    if (err) {
      combinedLog("Error on serial write: " + err.message);
      return;
    }
  });
}
//Remove the annoying volume missing codes from slider targets
function removeMissingCodes(vol) {
  switch (vol) {
    case 4:
      return 3;
    break;
    case 7:
      return 6;
    break;
    case 10:
      return 9;
    break;
    case 13:
      return 12;
    break;
    case 16:
      return 15;
    break;
    case 16:
      return 15;
    break;
    case 19:
      return 18;
    break;
    case 22:
      return 21;
    break;
    case 25:
      return 24;
    break;
    case 28:
      return 27;
    break;
    case 31:
      return 30;
    break;
    case 34:
      return 33;
    break;
    case 38:
      return 37;
    break;
    case 41:
      return 40;
    break;
    case 44:
      return 43;
    break;
    case 47:
      return 46;
    break;
		default:
		  return vol;
	}
}
// logging function
function combinedLog(message) {
  let curDate = new Date();
  let dateStr = curDate.toString();
  message = dateStr.slice(0,dateStr.length-33) + ' ' + message; //Prepend Time to message
  console.log(message);
  logfile.write(message + '\n')
}
