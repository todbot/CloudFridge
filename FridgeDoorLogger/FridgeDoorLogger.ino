/*
 * Fridge Door Logger with Pachube
 * 
 * Basis of this sketch from "PachubeClientString" example by Tom Igoe
 *
 *
 * Circuit:
 * - Magnetic door switch on pin 7
 * - Ethernet shield attached to pins 10, 11, 12, 13
 * - BlinkM Smart LED on pins A2,A3,A4,A5
 *
 *
 * 2012, Tod E. Kurt, http://todbot.com/blog/
 * 
 */

#include <SPI.h>
#include <Ethernet.h>

#include "./PachubeDetails.h"

const byte doorPin = 7;
// Choose any pins you want.  The pins below let you plug in a BlinkM directly 
const byte sclPin = A5;  // 'c' on BlinkM
const byte sdaPin = A4;  // 'd' on BlinkM
const byte pwrPin = A3;  // '+' on BlinkM
const byte gndPin = A2;  // '-' on BlinkM

#include "./SoftI2CMaster.h"
#include "./BlinkM_funcs_soft.h"

const byte blinkm_addr = 0;

// assign a MAC address for the ethernet controller. fill in your address here:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// initialize the library instance:
EthernetClient client;

unsigned long lastConnectionTime;   // last time connected to server, in millis
boolean lastConnected = false;      // state of connection last time through
unsigned int postingInterval = 15000;  // update period to Pachube.com

unsigned long doorOpenings; // how many times the door has been opened
unsigned long doorLastTime; // the last time the input changed
unsigned long doorUpdateMillis = 100; // time between door observations
int doorOpenCount;  // number of doorUpdateMillis door has been open
int doorOpen;
int doorOpenLast;

//
void setup()
{
  // start serial port:
  Serial.begin(19200);
  Serial.println("FridgeDoorLogger");
  
  pinMode( doorPin, INPUT);
  digitalWrite( doorPin, HIGH); // turn on internal pullup resistor

  BlinkM_begin( sclPin, sdaPin, pwrPin, gndPin );

  delay(1000);   // give the ethernet module time to boot up:

  BlinkM_off(0);
  BlinkM_setFadeSpeed( blinkm_addr, 15 );

  // start the Ethernet connection:
  Serial.print("Getting on the Ethernet...");
  if( Ethernet.begin(mac) == 0 ) {
    Serial.println("Failed to configure Ethernet using DHCP");
    BlinkM_playScript( blinkm_addr, 3, 0,0); // red blink error
    while(1);
  }
  Serial.println("done.");
  BlinkM_playScript( blinkm_addr, 4, 3, 0 ); // quick announce w/ 3 green flash
}


//
void loop()
{
  updateDoorState();

  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  if (!client.connected() && lastConnected) {
    Serial.println("\ndisconnecting.");
    client.stop();
  }

  // if you're not connected, and ten seconds have passed since
  // your last connection, then connect again and send data
  if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
    // convert two data readings to a comma-separated string
    unsigned long doorOpenMillis = doorOpenCount * doorUpdateMillis;
    //String data0String = String( doorOpenMillis );
    //String data1String = String( doorOpenings );
    //dataString += "," + String( doorOpenings );
    char datastr[80];
    sprintf(datastr, "%ld,%ld", doorOpenMillis, doorOpenings);

    sendData( datastr );

    resetDoorState();
  }
  // store the state of the connection for next time through the loop
  lastConnected = client.connected();
}

//
void resetDoorState()
{
  doorOpenings = 0;
  if( !doorOpen ) {
    doorOpenCount = 0;  // reset for next data collection period
  }
  else { 
    doorOpenCount = 1;  // carry over the fact the door is open to next window
  }

}

// check to see if door is open
// log number of milliseconds it's open
// and log the number of times its been opened
void updateDoorState()
{
  if( (millis() - doorLastTime) > doorUpdateMillis ) {
    doorLastTime = millis();
    doorOpen = digitalRead( doorPin );  // read the sensor:

    if( doorOpen == HIGH ) { 
      doorOpenCount++;  
      Serial.println("doorOpen!");
      BlinkM_fadeToRGB( blinkm_addr, 0x00,0x00,0xff );  // blue
      if( doorOpenLast == LOW ) {
        doorOpenings++;
        Serial.print("doorOpenCount=");Serial.print(doorOpenCount,DEC);
        Serial.print(",doorOpenings=");Serial.println(doorOpenings,DEC);
      }
    } else {
      BlinkM_fadeToRGB( blinkm_addr, 0x00,0x00,0x00 );  // off
    }
    doorOpenLast = doorOpen;
  }
}

// this method makes a HTTP connection to the server
// sending data to Pachube
//void sendData(String thisData) 
void sendData(char* dataString )
{
  Serial.print("sendData: "); Serial.print(dataString);
  Serial.print(", strlen:"); Serial.println(strlen(dataString));

  // if there's a successful connection:
  if( client.connect("api.pachube.com", 80) ) {
    Serial.println("connecting...");
    // send the HTTP PUT request. 
    client.print("PUT /api/");
    client.print( pachubeFeedId );
    client.println(".csv HTTP/1.1");
    client.println("Host: api.pachube.com");
    client.print("X-PachubeApiKey: ");
    client.println( pachubeApiKey );
    client.print("Content-Length: ");
    client.println( strlen(dataString), DEC);
    client.println("Content-Type: text/csv");
    client.println("Connection: close");
    client.println();

    // here's the actual content of the PUT request:
    client.println( dataString );

    // note the time that the connection was made:
    lastConnectionTime = millis();
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    client.stop();
    //BlinkM_playScript( blinkm_addr, 3, 0,0); // red blink error
  }
}
