// WhiteBox Labs -- Tentacle Shield -- Send sensor readings to the cloud: initialstate.com
// https://www.whiteboxes.ch/tentacle
//
//
// This sample code was written on an Arduino YUN, and depends on it's Bridge library to
// communicate wirelessly. For Arduino Mega, Uno etc, see the respective examples.
//
// This example shows how to take readings from the sensors in an asynchronous way, completely
// without using any delays. This allows to do other things while waiting for the sensor data.
// The sensor data is uploaded to InitialState cloud service. You will need an account at initialstate.com
//
// This example sketch includes code from https://github.com/InitialState/arduino_streamers
// (check the above link to see how to make the networking code compatible with other ethernet/wifi shields)
//
// USAGE:
//---------------------------------------------------------------------------------------------
// - Set all your EZO circuits to I2C before using this sketch.
//    - You can use the "tentacle-steup.ino" sketch to do so)
//    - Make sure each circuit has a unique I2C ID set
// - Change the variables NUM_CIRCUITS, channel_ids and channel_names to reflect your setup
// - Configure the InitialState settings: accessKey, bucketKey, bucketName
// - To talk to the Yun console, select your Yun's name and IP address in the Arduino IDE Port menu.
//    - The Yun will only show up in the Ports menu, if your computer is on the same Network as the Yun.
//    - If your Yun does not appear in the Ports menu, make sure the Yun is connected to your network
//      (see the basic Yun tutorials on the Arduino website to learn how to setup your Yun)
//
//---------------------------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------------------------------

#include <Wire.h>                              // enable I2C.
#include <Console.h>                           // Yun Console
#include <Process.h>                           // Yun Process, to run processes on the Yun's linux 
#include <SoftwareSerial.h>      // Include the software serial library     


SoftwareSerial sSerial(0, 1);  // RX, TX  - Name the software serial library sSerial (this cannot be omitted)
//assigned to pins 0 and 1 for maximum compatibility

int s0 = 7;                         // Arduino pin 7 to control pin S0
int s1 = 6;                         // Arduino pin 6 to control pin S1
int enable_1 = 5;                   // Arduino pin 5 to control pin E on board 1
int enable_2 = 4;                   // Arduino pin 4 to control pin E on board 2

#define NUM_CIRCUITS 3                         // <-- CHANGE THIS  set how many I2C circuits are attached to the Tentacle

const unsigned int send_readings_every = 2000; // set at what intervals the readings are sent to the computer (NOTE: this is not the frequency of taking the readings!)
unsigned long next_serial_time;

char sensordata[30];                          // A 30 byte character array to hold incoming data from the sensors
byte computer_bytes_received = 0;             // We need to know how many characters bytes have been received
byte sensor_bytes_received = 0;               // We need to know how many characters bytes have been received
byte code = 0;                                // used to hold the I2C response code.
byte in_char = 0;                             // used as a 1 byte buffer to store in bound bytes from the I2C Circuit.
int time;                                     // used to change the dynamic polling delay needed for I2C read operations.

int channel_ids[] = {100, 101, 102};        // <-- CHANGE THIS. A list of I2C ids that you set your circuits to.
String channel_names[] = {"PH", "DO", "TEMP"};   // <-- CHANGE THIS. A list of channel names (must be the same order as in channel_ids[]) - only used to give a name to the "Signals" sent to initialstate.com
String sensor_data[NUM_CIRCUITS];             // an array of strings to hold the readings of each channel
int channel = 0;                              // INT pointer to hold the current position in the channel_ids/channel_names array
char *cmd;                                    //Char pointer used in string parsing

char computerdata[48];                        // we make a 20 byte character array to hold incoming data from a pc/mac/other.
int computer_in_byte;                         // a variable to read incoming console data into
boolean computer_msg_complete = false;

const unsigned int reading_delay = 1000;      // time to wait for the circuit to process a read command. datasheets say 1 second.
unsigned long next_reading_time;              // holds the time when the next reading should be ready from the circuit
boolean request_pending = false;              // wether or not we're waiting for a reading

const unsigned int cloud_update_interval = 10000; // time to wait for the circuit to process a read command. datasheets say 1 second.
unsigned long next_cloud_update;              // holds the time when the next reading should be ready from the circuit

// INITIAL STATE CONFIGURATION
#define ISBucketURL "https://groker.initialstate.com/api/buckets" // bucket creation url. 
#define ISEventURL  "https://groker.initialstate.com/api/events"  // data destination. Thanks to Yun's linux, we can use SSL, yeah :)       
String bucketKey = "arduino12345";                                 // unique identifier for your bucket (unique in the context of your access key)
String bucketName = "Water Quality Monitor Yun";                  // Bucket name. Will be used to label your Bucket in the initialstate website.
String accessKey = "your-access-key-from-initialstate.com";       // <-- CHANGE THIS Access key (copy/paste from your initialstate account settings page)
// NOTE: when changing bucketName, make sure you change bucketKey, too. Bucket key can be anything - it's used to differentiate buckets, in case you have multiple.

boolean I2C_mode = false;                     // bool switch for serial/I2C

void setup() {

  pinMode(s1, OUTPUT);              // Set the digital pin as output.
  pinMode(s0, OUTPUT);              // Set the digital pin as output.
  pinMode(enable_1, OUTPUT);      // Set the digital pin as output.
  pinMode(enable_2, OUTPUT);      // Set the digital pin as output.
  
  pinMode(13, OUTPUT);                                 // set the led output pin
  Bridge.begin();
  Console.begin();                                     // initialize serial communication over network:
  while (!Console) ;                                   // wait for Console port to connect.
  Console.flush();
  Console.println(" ");
  Console.println("READY_");
  sSerial.begin(38400);              // Set the soft serial port to rate of default channel (0).
  Wire.begin();			                       // enable I2C port.

  delay(1000);                                         // Time for the ethernet shield to boot

  createBucket();                                      // create a bucket, if it doesn't exist yet
  next_serial_time = millis() + send_readings_every*3; // wait a little longer before sending serial data the first time
  next_cloud_update = millis() + cloud_update_interval;
}



void loop() {
  updateAtlas();
  updateSensors();                // read / write to the sensors. returns fast, does not wait for the data to arrive
  updateSerial();                 // write sensor data to the serial port
  updateCloud();                  // send the sensor data to the cloud. returns fast, except when a cloud update is due.
  // do your other arduino stuff here
}

void updateAtlas() {

  while (Console.available() > 0) {             // On Yun, there's no serialEvent(), so we read all data from the console here
    computer_in_byte = Console.read();          // read a byte

    if (computer_in_byte == '\n' || computer_in_byte == '\r') {      // if a newline character arrives, we assume a complete command has been received
      computerdata[computer_bytes_received] = 0;
      computer_msg_complete = true;
      computer_bytes_received = 0;
    } else {                                     // or just ad the byte to 
      computerdata[computer_bytes_received] = computer_in_byte;
      computer_bytes_received++;
    }
  }
  
  if (computer_msg_complete) {                  // If we received a command from the computer
    channel = atoi(strtok(computerdata, ":"));  // Let's parse the string at each colon
    cmd = strtok(NULL, ":");                    // Let's parse the string at each colon
    open_channel();                             // Call the function "open_channel" to open the correct data path

    if (I2C_mode == false) {    // if serial channel selected
      sSerial.print(cmd);         // Send the command from the computer to the Atlas Scientific device using the softserial port
      sSerial.print("\r");        // After we send the command we send a carriage return <CR>
    } else {        // if I2C address selected
      I2C_call();     // send to I2C
    }

    computer_msg_complete = false;              //Reset the var computer_msg_complete to be ready for the next command
  }

  if (sSerial.available() > 0) {                          // If data has been transmitted from an Atlas Scientific device
    sensor_bytes_received = sSerial.readBytesUntil(13, sensordata, 30);   // we read the data sent from the Atlas Scientific device until we see a <CR>. We also count how many character have been received
    sensordata[sensor_bytes_received] = 0;                  // we add a 0 to the spot in the array just after the last character we received. This will stop us from transmitting incorrect data that may have been left in the buffer
    Console.println(sensordata);                            // let’s transmit the data received from the Atlas Scientific device to the serial monitor
  }
  
}


// send the data to the cloud - but only when it's time to do so
void updateCloud() {
  
  if (millis() >= next_cloud_update) {                // is it time for the next serial communication?

    sendData();

    next_cloud_update = millis() + cloud_update_interval;
  }
}



// do serial communication in a "asynchronous" way
void updateSerial() {
  if (millis() >= next_serial_time) {              // is it time for the next serial communication?
    Console.println("---------------");
    for (int i = 0; i < NUM_CIRCUITS; i++) {       // loop through all the sensors
      Console.print(channel_names[i]);             // print channel name
      Console.print(":\t");
      Console.println(sensor_data[i]);             // print the actual reading
    }
    Console.print("FM");
    Console.print(":\t");
    Console.println(String(sensordata));
    Console.println("---------------");
    next_serial_time = millis() + send_readings_every;
  }
}



// take sensor readings in a "asynchronous" way
void updateSensors() {
  if (request_pending) {                          // is a request pending?
    if (millis() >= next_reading_time) {          // is it time for the reading to be taken?
      receiveReading();                           // do the actual I2C communication
    }
  } else {                                        // no request is pending,
    channel = (channel + 1) % NUM_CIRCUITS;       // switch to the next channel (increase current channel by 1, and roll over if we're at the last channel using the % modulo operator)
    requestReading();                             // do the actual I2C communication
  }
}



// Request a reading from the current channel
void requestReading() {
  request_pending = true;
  Wire.beginTransmission(channel_ids[channel]); // call the circuit by its ID number.
  Wire.write('r');        		        // request a reading by sending 'r'
  Wire.endTransmission();          	        // end the I2C data transmission.
  next_reading_time = millis() + reading_delay; // calculate the next time to request a reading
}



// Receive data from the I2C bus
void receiveReading() {
  sensor_bytes_received = 0;                        // reset data counter
  memset(sensordata, 0, sizeof(sensordata));        // clear sensordata array;

  Wire.requestFrom(channel_ids[channel], 48, 1);    // call the circuit and request 48 bytes (this is more then we need).
  code = Wire.read();
  
  while (Wire.available()) {          // are there bytes to receive?
    in_char = Wire.read();            // receive a byte.

    if (in_char == 0) {               // if we see that we have been sent a null command.
      Wire.endTransmission();         // end the I2C data transmission.
      break;                          // exit the while loop, we're done here
    }
    else {
      sensordata[sensor_bytes_received] = in_char;  // load this byte into our array.
      sensor_bytes_received++;
    }
  }
  
  char *filtered_sensordata;                     // pointer to hold a modified version of the data
  filtered_sensordata = strtok (sensordata,","); // we split at the first comma - needed for the ec stamp only

  switch (code) {                  	    // switch case based on what the response code is.
    case 1:                       	    // decimal 1  means the command was successful.
      sensor_data[channel] = filtered_sensordata;
      break;                        	    // exits the switch case.

    case 2:                        	    // decimal 2 means the command has failed.
      Console.print("channel \"");
      Console.print( channel_names[channel] );
      Console.println ("\": command failed");
      break;                         	    // exits the switch case.

    case 254:                      	    // decimal 254  means the command has not yet been finished calculating.
      Console.print("channel \"");
      Console.print( channel_names[channel] );
      Console.println ("\": reading not ready");
      break;                         	    // exits the switch case.

    case 255:                      	    // decimal 255 means there is no further data to send.
      Console.print("channel \"");
      Console.print( channel_names[channel] );
      Console.println ("\": no answer");
      break;                         	    // exits the switch case.
  }
  request_pending = false;                  // set pending to false, so we can continue to the next sensor
}



// make a HTTP connection to the server and send create the InitialState Bucket (if it doesn't exist yet)
void createBucket()
{
  // Initialize the process
  Process isbucket;

  isbucket.begin("curl");
  isbucket.addParameter("-k");    // we use SSL, but we bypass certificate verification! 
  isbucket.addParameter("-v");
  isbucket.addParameter("-X");
  isbucket.addParameter("POST");
  isbucket.addParameter("-H");
  isbucket.addParameter("Content-Type:application/json");
  isbucket.addParameter("-H");
  isbucket.addParameter("Accept-Version:0.0.1");

  // IS Access Key Header
  isbucket.addParameter("-H");
  isbucket.addParameter("X-IS-AccessKey:" + accessKey);

  // IS Bucket Key Header
  isbucket.addParameter("-d");
  isbucket.addParameter("{\"bucketKey\": \"" + bucketKey + "\", \"bucketName\": \"" + bucketName + "\"}");
  
  isbucket.addParameter(ISBucketURL);
  
  // Run the process
  isbucket.run();
}



// make a HTTP connection to the server and send the sensor readings
void sendData()
{
  // Initialize the process
  Process isstreamer;

  isstreamer.begin("curl");
  isstreamer.addParameter("-k");  // we use SSL, but we bypass certificate verification! 
  isstreamer.addParameter("-v");
  isstreamer.addParameter("-X");
  isstreamer.addParameter("POST");
  isstreamer.addParameter("-H");
  isstreamer.addParameter("Content-Type:application/json");
  isstreamer.addParameter("-H");
  isstreamer.addParameter("Accept-Version:0.0.1");

  // IS Access Key Header
  isstreamer.addParameter("-H");
  isstreamer.addParameter("X-IS-AccessKey:" + accessKey);

  // IS Bucket Key Header
  // Note that bucketName is not needed here
  isstreamer.addParameter("-H");
  isstreamer.addParameter("X-IS-BucketKey:" + bucketKey);

  isstreamer.addParameter("-d");

  // Initialize a string to hold our signal data
  String jsonData;

  jsonData = "[";

  for (int i=0; i<NUM_CIRCUITS; i++)
  {
    jsonData += "{\"key\": \"" + channel_names[i] + "\", \"value\": \"" + sensor_data[i] + "\"}";

    if (i != NUM_CIRCUITS - 1)
    {
      jsonData += ",";
    }
  }

  jsonData += ",{\"key\": \"FM\", \"value\": \"" + String(sensordata) + "\"}";

  jsonData += "]";

  isstreamer.addParameter(jsonData);

  isstreamer.addParameter(ISEventURL);

  // Print posted data for debug
  Console.print("Sending data: ");
  Console.println(jsonData);

  // Run the process
  isstreamer.run();
}

void intro() {                                // print intro
  Console.flush();
  Console.println(" ");
  Console.println("READY_");
}

void open_channel() {                         // function controls which UART/I2C port is opened.
  I2C_mode = false;            // false for serial, true for I2C
  switch (channel) {                          // Looking to see what channel to open

    case 0:                                   // If channel==0 then we open channel 0
      digitalWrite(enable_1, LOW);            // Setting enable_1 to low activates primary channels: 0,1,2,3
      digitalWrite(enable_2, HIGH);           // Setting enable_2 to high deactivates secondary channels: 4,5,6,7
      digitalWrite(s0, LOW);                  // S0 and S1 control what channel opens
      digitalWrite(s1, LOW);                  // S0 and S1 control what channel opens
      break;                                  // Exit switch case

    case 1:
      digitalWrite(enable_1, LOW);
      digitalWrite(enable_2, HIGH);
      digitalWrite(s0, HIGH);
      digitalWrite(s1, LOW);
      break;

    case 2:
      digitalWrite(enable_1, LOW);
      digitalWrite(enable_2, HIGH);
      digitalWrite(s0, LOW);
      digitalWrite(s1, HIGH);
      break;

    case 3:
      digitalWrite(enable_1, LOW);
      digitalWrite(enable_2, HIGH);
      digitalWrite(s0, HIGH);
      digitalWrite(s1, HIGH);
      break;

    case 4:
      digitalWrite(enable_1, HIGH);
      digitalWrite(enable_2, LOW);
      digitalWrite(s0, LOW);
      digitalWrite(s1, LOW);
      break;

    case 5:
      digitalWrite(enable_1, HIGH);
      digitalWrite(enable_2, LOW);
      digitalWrite(s0, HIGH);
      digitalWrite(s1, LOW);
      break;

    case 6:
      digitalWrite(enable_1, HIGH);
      digitalWrite(enable_2, LOW);
      digitalWrite(s0, LOW);
      digitalWrite(s1, HIGH);
      break;

    case 7:
      digitalWrite(enable_1, HIGH);
      digitalWrite(enable_2, LOW);
      digitalWrite(s0, HIGH);
      digitalWrite(s1, HIGH);
      break;

    default:          // I2C mode
      digitalWrite(enable_1, HIGH);   // disable soft serial
      digitalWrite(enable_2, HIGH);   // disable soft serial

      if (channel <= 127) {
        I2C_mode = true;      // 0 for serial, 1 for I2C
        return;
      }

  }
}

void I2C_call() {                // function to parse and call I2C commands
  sensor_bytes_received = 0;                    // reset data counter
  memset(sensordata, 0, sizeof(sensordata));    // clear sensordata array;

  if (cmd[0] == 'c' || cmd[0] == 'r')time = 1400;
  else time = 300;                              //if a command has been sent to calibrate or take a reading we
  //wait 1400ms so that the circuit has time to take the reading.
  //if any other command has been sent we wait only 300ms.
  
  Wire.beginTransmission(channel);  // call the circuit by its ID number.
  Wire.write(cmd);            // transmit the command that was sent through the serial port.
  Wire.endTransmission();           // end the I2C data transmission.

  delay(time);

  code = 254;       // init code value

  while (code == 254) {                 // in case the cammand takes longer to process, we keep looping here until we get a success or an error

    Wire.requestFrom(channel, 48, 1);   // call the circuit and request 48 bytes (this is more then we need).
    code = Wire.read();

    while (Wire.available()) {          // are there bytes to receive.
      in_char = Wire.read();            // receive a byte.

      if (in_char == 0) {               // if we see that we have been sent a null command.
        Wire.endTransmission();         // end the I2C data transmission.
        break;                          // exit the while loop.
      }
      else {
        sensordata[sensor_bytes_received] = in_char;  // load this byte into our array.
        sensor_bytes_received++;
      }
    }


    switch (code) {                   // switch case based on what the response code is.
      case 1:                         // decimal 1.
        Console.println("Success");   // means the command was successful.
        break;                          // exits the switch case.

      case 2:                         // decimal 2.
        Console.println("< command failed");      // means the command has failed.
        break;                          // exits the switch case.

      case 254:                       // decimal 254.
        Console.println("< command pending");     // means the command has not yet been finished calculating.
        delay(200);                     // we wait for 200ms and give the circuit some time to complete the command
        break;                          // exits the switch case.

      case 255:                       // decimal 255.
        Console.println("No Data");     // means there is no further data to send.
        break;                          // exits the switch case.
    }

  }

  Console.println(sensordata);  // print the data.
}
