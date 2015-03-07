
#include <SPI.h>
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <SD.h>
#include <avr/sleep.h>

// Ladyada's logger modified by Bill Greiman to use the SdFat library
//
// This code shows how to listen to the GPS module in an interrupt
// which allows the program to have more 'freedom' - just parse
// when a new NMEA sentence is available! Then access data when
// desired.
//
// Tested and works great with the Adafruit Ultimate GPS Shield
// using MTK33x9 chipset
//    ------> http://www.adafruit.com/products/
// Pick one up today at the Adafruit electronics shop 
// and help support open source hardware & software! -ada

SoftwareSerial mySerial(8, 7);
Adafruit_GPS GPS(&mySerial);

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO  true
/* set to true to only log to SD when GPS has a fix, for debugging, keep it false */
#define LOG_FIXONLY false  

// Set the pins used
#define chipSelect 10
#define ledPin 13

File logfile;

// read a Hex value and return the decimal equivalent
uint8_t parseHex(char c) {
  if (c < '0')
    return 0;
  if (c <= '9')
    return c - '0';
  if (c < 'A')
    return 0;
  if (c <= 'F')
    return (c - 'A')+10;
}

// blink out an error code
void error(uint8_t errno) {
/*
  if (SD.errorCode()) {
    putstring("SD error: ");
    Serial.print(card.errorCode(), HEX);
    Serial.print(',');
    Serial.println(card.errorData(), HEX);
  }
  */
  while(1) {
    uint8_t i;
    for (i=0; i<errno; i++) {
      digitalWrite(ledPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
      delay(100);
    }
    for (i=errno; i<10; i++) {
      delay(200);
    }
  }
}

void setup() {
  // for Leonardos, if you want to debug SD issues, uncomment this line
  // to see serial output
  //while (!Serial);
  
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  Serial.begin(115200);
  Serial.println("\r\nUltimate GPSlogger Shield");
  pinMode(ledPin, OUTPUT);

  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect, 11, 12, 13)) {
  //if (!SD.begin(chipSelect)) {      // if you're using an UNO, you can use this line instead
    Serial.println("Card init. failed!");
    error(2);
  }
  char filename[15];
  strcpy(filename, "GPSLOG00.TXT");
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = '0' + i/10;
    filename[7] = '0' + i%10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
    }
  }

  logfile = SD.open(filename, FILE_WRITE);
  if( ! logfile ) {
    Serial.print("Couldnt create "); Serial.println(filename);
    error(3);
  }
  Serial.print("Writing to "); Serial.println(filename);
  
  // connect to the GPS at the desired rate
  GPS.begin(9600);

  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For logging data, we don't suggest using anything but either RMC only or RMC+GGA
  // to keep the log files at a reasonable size
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 or 5 Hz update rate

  // Turn off updates on antenna status, if the firmware permits it
  GPS.sendCommand(PGCMD_NOANTENNA);
  
  Serial.println("Ready!");
}

void loop() {
  char c = GPS.read();
  if (GPSECHO)
     if (c)   Serial.print(c);

  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
    
    // Sentence parsed! 
    //Serial.println(F("OK"));
    if (LOG_FIXONLY && !GPS.fix) {
        Serial.print(F("No Fix"));
        return;
    }
    
    // Write google/human friendly gps lat/long to logfile
    Serial.println(F("WRITING DATA TO SDRAM:"));
   
    Serial.print("\nTime: ");
    Serial.print(GPS.hour, DEC); Serial.print(':');
    Serial.print(GPS.minute, DEC); Serial.print(':');
    Serial.print(GPS.seconds, DEC); Serial.print('.');
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC); Serial.print('/');
    Serial.print(GPS.month, DEC); Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print(F(", "));
    Serial.print((int)GPS.fix);
    Serial.print(F(", "));
    if ((GPS.lat == 'N') ) {
      Serial.print("+") + Serial.print(decimalDegrees(GPS.latitude), 4);
      }
    if ((GPS.lat == 'S') ) {
      Serial.print("-") + Serial.print(decimalDegrees(GPS.latitude), 4);
      }
    Serial.print(F(", "));
    
    Serial.print(GPS.lat);
    
    Serial.print(F(", "));
    
    if ((GPS.lon == 'E') ) {
      Serial.print("+") + Serial.print(decimalDegrees(GPS.longitude), 4);
      }
    if ((GPS.lon == 'W') ) {
      Serial.print("-") + Serial.print(decimalDegrees(GPS.longitude), 4);
      }
    Serial.print(F(", "));
    
    Serial.print(GPS.lon);
    
    Serial.print(F(", "));
    
    Serial.print(GPS.altitude);
    
    Serial.print(F(", "));
    
    Serial.print(GPS.satellites);
    
    Serial.print(F(", "));
    
    Serial.println(GPS.HDOP);
    
    char *stringptr = GPS.lastNMEA();
    uint8_t stringsize = strlen(stringptr);
    
    logfile.print(GPS.day, DEC); logfile.print('/');
    logfile.print(GPS.month, DEC); logfile.print("/20");
    logfile.print(GPS.year, DEC);
    logfile.print(F(";"));
    logfile.print(GPS.hour, DEC); logfile.print(':');
    logfile.print(GPS.minute, DEC); logfile.print(':');
    logfile.print(GPS.seconds, DEC); 
    logfile.print(F(";"));
    
    if ((GPS.lat == 'N') ) {
      logfile.print("+") + logfile.print(decimalDegrees(GPS.latitude), 4);
      }
    if ((GPS.lat == 'S') ) {
      logfile.print("-") + logfile.print(decimalDegrees(GPS.latitude), 4);
      }
    logfile.print(F(";"));
    
    logfile.print(GPS.lat);
    
    logfile.print(F(";"));
    
    if ((GPS.lon == 'E') ) {
      logfile.print("+") + logfile.print(decimalDegrees(GPS.longitude), 4);
      }
    if ((GPS.lon == 'W') ) {
      logfile.print("-") + logfile.print(decimalDegrees(GPS.longitude), 4);
      }
    logfile.print(F(";"));
    
    logfile.print(GPS.lon);
    
    logfile.print(F(";"));
   
    logfile.print(GPS.altitude);
    
    logfile.print(F(";"));
    
    logfile.print(GPS.satellites);
    
    logfile.print(F(";"));
    
    logfile.println(GPS.HDOP);
    
    if (strstr(stringptr, "RMC"))   logfile.flush();
    Serial.println();
   
    delay(10000); 
  }
}


// convert NMEA coordinate to decimal degrees
float decimalDegrees(float nmeaCoord) {
  uint16_t wholeDegrees = 0.01*nmeaCoord;
  return wholeDegrees + (nmeaCoord - 100.0*wholeDegrees)/60.0;
}
