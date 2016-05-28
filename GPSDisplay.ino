// Test code for Adafruit GPS modules using MTK3329/MTK3339 driver
//
// This code shows how to listen to the GPS module in an interrupt
// which allows the program to have more 'freedom' - just parse
// when a new NMEA sentence is available! Then access data when
// desired.
//
// Tested and works great with the Adafruit Ultimate GPS module
// using MTK33x9 chipset
//    ------> http://www.adafruit.com/products/746
// Pick one up today at the Adafruit electronics shop 
// and help support open source hardware & software! -ada

#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <Wire.h>
// If you're using a GPS module:
// Connect the GPS Power pin to 5V
// Connect the GPS Ground pin to ground
// If using software serial (sketch example default):
//   Connect the GPS TX (transmit) pin to Digital 3
//   Connect the GPS RX (receive) pin to Digital 2
// If using hardware serial (e.g. Arduino Mega):
//   Connect the GPS TX (transmit) pin to Arduino RX1, RX2 or RX3
//   Connect the GPS RX (receive) pin to matching TX1, TX2 or TX3

// If you're using the Adafruit GPS shield, change 
// SoftwareSerial mySerial(3, 2); -> SoftwareSerial mySerial(8, 7);
// and make sure the switch is set to SoftSerial

// If using software serial, keep this line enabled
// (you can change the pin numbers to match your wiring):
SoftwareSerial mySerial(3, 2);

// If using hardware serial (e.g. Arduino Mega), comment out the
// above SoftwareSerial line, and enable this line instead
// (you can change the Serial number to match your wiring):

//HardwareSerial mySerial = Serial1;


Adafruit_GPS GPS(&mySerial);


// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences. 
#define GPSECHO  false

// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;
void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy
const byte s7sAddress = 0x71;
char tempString[10];

int state = 0;
int lastState = 0;
int mode = 0;

void setup()  
{
  pinMode(6,OUTPUT);
  pinMode(5,INPUT);

  Wire.begin();
  clearDisplayI2C();

  setBrightnessI2C(255);
  delay(1000);
  clearDisplayI2C();
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  Serial.begin(115200);
  Serial.println("Adafruit GPS library basic test!");

  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);

  
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ);   // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz

  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  // the nice thing about this code is you can have a timer0 interrupt go off
  // every 1 millisecond, and read data from the GPS for you. that makes the
  // loop code a heck of a lot easier!
  useInterrupt(true);

  delay(1000);
  // Ask for firmware version
  mySerial.println(PMTK_Q_RELEASE);
}


// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
#ifdef UDR0
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
#endif
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

uint32_t timer = millis();
void loop()                     // run over and over again
{
  digitalWrite(6, HIGH);
  // in case you are not using the interrupt above, you'll
  // need to 'hand query' the GPS, not suggested :(
  if (! usingInterrupt) {
    // read data from the GPS in the 'main loop'
    char c = GPS.read();
    // if you want to debug, this is a good time to do it!
    if (GPSECHO)
      if (c) Serial.print(c);
  }
  
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
  
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }

  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  timer = millis();

  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 200) { 
    timer = millis(); // reset the timer
    
    
    if (GPS.fix)
    {
      determineState();
    }

    else
    {
      setDecimalsI2C(0b000000);
      s7sSendStringI2C("5hiT");
    }
  }
}


void determineState()
{
  state = digitalRead(5);
  if (state != lastState && state == LOW)
  {
    mode ++;
    if (mode > 3)
    {
      mode = 0;
    }
  }
  delay(50);
  lastState = state;
  generateDisplay();
}

void generateDisplay()
{
  switch (mode)
  {
    case 0: 
        displaySpeed();
        break;
    case 1:
        displayAngle();
        break;
    case 2: 
        displayTime();
        break;
    case 3:
        displaySats();
        break;
  }
}

void displaySpeed()
{
  setDecimalsI2C(0b000000);
  int mphNum = int(GPS.speed*1.15078);
  String mph = String(mphNum);
  String output = "";
  if (mphNum >= 0 && mphNum < 10)
  {
      output = "  " + mph +" ";
  }
  else
  {
      output = " " + mph + " ";
  }
  s7sSendStringI2C(output);
}

void displayAngle()
{
  int angleNum = int(GPS.angle);
  String angle = String(angleNum);
  String output = "";
  if (angleNum >0 && angleNum < 10)
  {
    output = "H  " + angle;
  }

  else if (angleNum >= 10 && angleNum < 100)
  {
    output = "H " + angle;
  }

  else if (angleNum >= 100) 
  {
    output = "H" + angle;
  }
  s7sSendStringI2C(output);
}

void displayTime()
{
  int hourNum = GPS.hour;
  int minNum = GPS.minute;
  String hour; 
  String minutes; 

  if (hourNum < 10)
  {
    hour = "0" + String(hourNum);
  }

  else
  {
    hour = String(hourNum);
  }

  if (minNum < 10)
  {
    minutes = "0" + String(minNum);
  }

  else
  {
    minutes = String(minNum);
  }
 
  
  s7sSendStringI2C(hour + minutes);
  
  setDecimalsI2C(0b00010000);

}

void displaySats()
{
  String sats;
  if (GPS.satellites < 10)
  {
    sats = "0" + String(GPS.satellites);
  }

  else
  {
    sats = String(GPS.satellites);
  }
  s7sSendStringI2C("5t" + sats);
}
void s7sSendStringI2C(String toSend)
{
  Wire.beginTransmission(s7sAddress);
  for (int i = 0; i < 4; i ++)
  {
    Wire.write(toSend[i]);
  }
  Wire.endTransmission();
}

void clearDisplayI2C()
{
  Wire.beginTransmission(s7sAddress);
  Wire.write(0x76);
  Wire.endTransmission();
}

void setBrightnessI2C(byte value)
{
  Wire.beginTransmission(s7sAddress);
  Wire.write(0x7A);
  Wire.write(value);
  Wire.endTransmission();
}

void setDecimalsI2C(byte decimals)
{
  Wire.beginTransmission(s7sAddress);
  Wire.write(0x77);
  Wire.write(decimals);
  Wire.endTransmission();
}
