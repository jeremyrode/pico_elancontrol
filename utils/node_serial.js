

const { SerialPort } = require('serialport');

const port = new SerialPort({ path: '/dev/ttyACM0', baudRate: 921600 });


port.write('0', function(err) {
  if (err) {
    return console.log("Error on write: ", err.message);
  }
  console.log("Message sent successfully");
});
