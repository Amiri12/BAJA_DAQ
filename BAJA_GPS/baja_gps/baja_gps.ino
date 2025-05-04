#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

SoftwareSerial GPSData(4, 6);    // GPS module → RX pin 4
SoftwareSerial GPSOUT(-1, 9);    // our “virtual TX” out on pin 9

TinyGPSPlus gps;
float  lat, lon;
double speedMph;

void setup() {
  Serial.begin(9600);            // for debug
  GPSData.begin(9600);           // listen to NMEA from GPS
  GPSOUT.begin(9600);            // send processed string out
}

void loop() {
  // 1) Feed incoming NMEA bytes into TinyGPS++
  while (GPSData.available()) {
    gps.encode(GPSData.read());
  }

  // 2) Extract valid location & speed
  if (gps.location.isValid()) {
    lat = gps.location.lat();
    lon = gps.location.lng();
  } else {
    lat = lon = -1;
  }

  if (gps.speed.isValid()) {
    speedMph = gps.speed.mph();
  } else {
    speedMph = -1;
  }

  // 3) Build a comma-separated string *without* a trailing comma
  //    (makes it easier on the master)
  String pps = String(lat, 6) + "," 
             + String(lon, 6) + "," 
             + String(speedMph, 1);

  // 4) Send it out
  GPSOUT.println(pps);

  // (Optionally delay here to match your master’s read rate)
  //delay(200);
}
