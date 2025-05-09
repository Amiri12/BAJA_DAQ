#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>

// — GPS & RPM on SoftwareSerial —
TinyGPSPlus gps;
SoftwareSerial gpsSerial(4, 5);  // GPS TX → pin 4 (RX)
SoftwareSerial rpmSerial(2, -1);  // RPM TX → pin 3 (RX)

// — SD card & file naming via EEPROM —
const int  chipSelect  = 10;
char       fileName[13];
int        fileIndex;

// — Timing —
unsigned long startTime;
unsigned long lastLog    = 0;
const unsigned long logInterval = 1000; // ms between logs

// — Built-in LED pin —
const int ledPin = 13;

// Helper: read one “rpm1,rpm2” line or return "NA,NA" on timeout
String readRPMPair(unsigned long timeout = 100) {
  unsigned long t0 = millis();
  while (rpmSerial.available()) rpmSerial.read();
  while (!rpmSerial.available() && millis() - t0 < timeout) {}
  if (!rpmSerial.available()) return "NA,NA";
  String s = rpmSerial.readStringUntil('\n');
  s.trim();
  return s;
}

void setup() {

  // LED setup
  pinMode(ledPin, OUTPUT);

  // Serials
  Serial.begin(115200);
  gpsSerial.begin(9600);
  rpmSerial.begin(9600);

  // SD & filename
  pinMode(chipSelect, OUTPUT);
  fileIndex = EEPROM.read(0) % 100;
  snprintf(fileName, sizeof(fileName), "log%02d.csv", fileIndex);
  EEPROM.update(0, fileIndex + 1);

  if (!SD.begin(chipSelect)) {
    // SD init failed → LED stays off
    Serial.println("SD init failed!");
    while (1) digitalWrite(ledPin, LOW);
  }

  // Write header if new
  if (!SD.exists(fileName)) {
    File hdr = SD.open(fileName, FILE_WRITE);
    hdr.println("Time_ms,Latitude,Longitude,Speed_kmh,Heading_deg,RPM1,RPM2");
    hdr.close();
  }

  // Indicate SD & header OK
  digitalWrite(ledPin, HIGH);
  startTime = millis();
}

void loop() {
  // 1) Feed GPS into parser
  gpsSerial.listen();    // <— make GPS port active
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  // 2) Throttle to logInterval
  unsigned long now = millis();
  if (now - lastLog < logInterval) return;
  lastLog = now;

  // 3) Gather data
  unsigned long elapsed = now - startTime;

String lat;
if (gps.location.isValid()) {
  lat = String(gps.location.lat(), 6);
} else {
  lat = "NA";
}

String lon;
if (gps.location.isValid()) {
  lon = String(gps.location.lng(), 6);
} else {
  lon = "NA";
}

String spd;
if (gps.speed.isValid()) {
  spd = String(gps.speed.kmph(), 2);
} else {
  spd = "NA";
}

String hdg;
if (gps.course.isValid()) {
  hdg = String(gps.course.deg(), 1);
} else {
  hdg = "NA";
}
  rpmSerial.listen();
  String rpmP = readRPMPair();

  // 4) Blink LED and write CSV
  digitalWrite(ledPin, HIGH);
  if(gps.location.isValid()){
    //Serial.println("LOCK");
  }    // start blink
  File f = SD.open(fileName, FILE_WRITE);
  if (f) {
    f.print(elapsed); f.print(',');
    f.print(lat);     f.print(',');
    f.print(lon);     f.print(',');
    f.print(spd);     f.print(',');
    f.print(hdg);     f.print(',');
    f.println(rpmP);
    f.close();
  } else {
    Serial.println("Error opening log file");
  }
  delay(10);                     // let the blink be visible
  digitalWrite(ledPin, LOW);     // end blink
}
