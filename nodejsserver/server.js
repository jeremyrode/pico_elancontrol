"use strict";
const fs = require('fs');
const { SerialPort } = require('serialport');
const { ByteLengthParser } = require('@serialport/parser-byte-length')


// Optional. You will see this name in eg. 'ps' or 'top' command
process.title = 'elan-websocket';
// Port where we'll run the websocket server
const webSocketsServerPort = 1338;
const LOG_FILE = '/home/jprode/ElanControlLog.txt';
const ELAN_POWER = 0;
const ELAN_VOLUP = 4;
const ELAN_VOLDOWN = 36;
// websocket and http servers
const webSocketServer = require('websocket').server;
const http = require('http');
const logfile = fs.createWriteStream(LOG_FILE, {flags:'a'});
// Status Object
let status = {
  volume: [0,0,0,0,0,0],
  mute: [0,0,0,0,0,0],
  input: [0,0,0,0,0,0],
  on: false
};
const slider_commands = [[],[],[],[],[],[]]; //Stores timeout events associated with a slider
let last_update = 0; //Timestamp of last serial update
const old_data = Buffer.alloc(35); //Serial buffer to compair for changes
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
//Wesocket Message
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
      cancelPendingSliders(zone);
      send_zpad_command_serial(zonetoChannel(zone),command); //Calls my C bitbanger
      return;
    }
    if (commnds.length == 3) { //Two colons, it's a slider command
      //console.log('Got slider command: ' + message.utf8Data);
      const zone = parseInt(commnds[1]);
      let desired_vol = parseInt(commnds[2]);
      if (desired_vol < 0 || desired_vol > 48 || zone < 1 || zone > 6) {
        combinedLog('malformatted command: ' + message.utf8Data);
        return;
      }
      cancelPendingSliders(zone); //If we're already sliding, cancel
      if (desired_vol > 33) { //Lets limit someone from flooring the vol via slider
        desired_vol = 33;
      }
      //Set recustion limit to a bit more than we need to handle missing vol codes
      const expected_steps = Math.abs(desired_vol - status.volume[zone-1]) + 6; //Increased for debugging
      if (status.input[zone-1] != 1) { //if not on, turn on, but need delay
        send_zpad_command_serial(zonetoChannel(zone),ELAN_POWER);
        slider_commands[zone-1] = setTimeout(setDesiredVol,250,zone,removeMissingCodes(desired_vol),expected_steps); //delay
      }
      else {
          setDesiredVol(zone,removeMissingCodes(desired_vol),expected_steps);
      }
      return;
    }
    combinedLog('Got short malformatted command from client: ' + message.utf8Data)
  }
  combinedLog('Got non-utf8 command from client');
}
//Recusive funtion to get volume to a desired value based on slider change
function setDesiredVol(zone,desired_vol,num_rec) {
  if (desired_vol == status.volume[zone-1]) { //If we reach target, stop
    return;
  }
  if (num_rec < 0) { //There are missing volume codes that cause inf recursion
    combinedLog('We hit the recursion limit trying to get to ' + desired_vol);
    return;
  }
  //console.log('Zone: ' + zone + ' Desired: ' + desired_vol + ' Current: ' + status.volume[zone-1]);
  if ((desired_vol - status.volume[zone-1]) > 0) {
    send_zpad_command_serial(zonetoChannel(zone),ELAN_VOLUP); //Vol up
  }
  else { //Vol Down
    send_zpad_command_serial(zonetoChannel(zone),ELAN_VOLDOWN); //Vol down
  }
  slider_commands[zone-1] = setTimeout(setDesiredVol,250,zone,desired_vol,num_rec-1); //call ourself in a bit
}
//Cancel any pending changes for a given channel
function cancelPendingSliders(zone) {
  clearTimeout(slider_commands[zone-1]);
  slider_commands[zone-1] = []; //clear the array out
}
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
function onDiffData(sdata) {
  last_update = Date.now(); //We are getting serial data, note the time
  if (status.on == false) { //If we have differing data, we turned on!
    status.on = true; // We're on now!
    onStatusCheck = setInterval(isOn,2000); // Check to see if we're off every 2s
  }
  var new_data = extractData(sdata); // Get the new status
  status = new_data; //Store new status
  updateClients();
}

setInterval(updateClients,10000); // Force an update every 10s, will also help cull stale clients
//Send the new status to the client list when things change
function updateClients() {
  const json = JSON.stringify(status); // encode new status
  for (let i=0; i < clients.length; i++) { //Send the status to clients
    if (clients[i].connected) { //only if client is connected
      clients[i].sendUTF(json);
    }
    else {
      clients.splice(i, 1); // Remove the client if not connected
    }
  }
}
//When the system is off, we get no serial data
function isOn() {
  if (Date.now() - last_update > 2000) { //If no serial data for 2s, we off
    //console.log('System is Off');
    status.volume = [0,0,0,0,0,0];
    status.mute = [0,0,0,0,0,0];
    status.input = [0,0,0,0,0,0];
    status.on = false;
    updateClients();
    clearInterval(onStatusCheck); //No need to check anymore we're off
  }
}
//Monitor the serial port for system status
const port = new SerialPort({ path: '/dev/ttyACM0', baudRate: 921600 });

const parser = port.pipe(new ByteLengthParser({ length: 35 }));
parser.on('data', function(data) {onDiffData(data);});

function send_zpad_command_serial(zone, channel) {
  port.port.write(Buffer.from('S' + zone + channel,'ascii'), function(err) {
    if (err) {
      return console.log("Error on write: ", err.message);
    }
  });
}


//translate from zone to RasberryPi GPIO channel
function zonetoChannel(zone) {
  return zone; //Take this out for now
}

//Remove the annoying volume missing codes from slider targers
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