#include <SD.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

// SD‐card and LED diagnostics
const int chipSelect = 10;
const int ledPin     = 9;

// File handle and run counter
File    myFile;
int     Num;
String  fileName;

// Analog inputs
int S1, S2, S3, S4, S5, S6;
// “Serial” inputs (we’ll open/close each in readPWM())
float P1, P2, P3, P4, P5;

// SoftwareSerial instances: RX pins 4–8, TX pinned to 3 (unused)
SoftwareSerial pwm1(4, 3);
SoftwareSerial pwm2(5, 3);
SoftwareSerial pwm3(6, 3);
SoftwareSerial pwm4(7, 3);
SoftwareSerial pwm5(8, 3);

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
  while (!Serial) { /* wait for Serial Monitor */ }

  Serial.print("Initializing SD card...");
  digitalWrite(ledPin, LOW);  // off = “not ready yet”

  // Grab run number from EEPROM and build filename
  Num = EEPROM.read(0);
  fileName = "log" + String(Num) + ".csv";
  EEPROM.update(0, Num + 1);

  // Initialize SD
  if (!SD.begin(chipSelect)) {
    Serial.println(" failed!");
    while (1) { digitalWrite(ledPin, LOW); } // halt
  }
  digitalWrite(ledPin, HIGH);
  Serial.println(" done.");

  // Write CSV header
  myFile = SD.open(fileName, FILE_WRITE);
  if (myFile) {
    myFile.println("Time, S1, S2, S3, S4, S5, S6, P1, P2, P3, P4, P5");
    myFile.close();
  } else {
    Serial.println("Error opening file!");
    while (1) { digitalWrite(ledPin, LOW); }
  }
}

void loop() {
  static unsigned long lastWriteTime = 0;
  const unsigned long writeInterval = 20; // ms

  unsigned long t = millis();

  // Read analog sensors
  S1 = analogRead(A0);
  S2 = analogRead(A1);
  S3 = analogRead(A2);
  S4 = analogRead(A3);
  S5 = analogRead(A4);
  S6 = analogRead(A5);

  // Read each “serial” input one at a time
  P1 = readPWM(pwm1);
  P2 = readPWM(pwm2);
  P3 = readPWM(pwm3);
  P4 = readPWM(pwm4);
  P5 = readPWM(pwm5);

  // Throttle SD writes
  if (t - lastWriteTime >= writeInterval) {
    lastWriteTime = t;
    myFile = SD.open(fileName, FILE_WRITE);
    if (myFile) {
      myFile.print(t);   myFile.print(",");
      myFile.print(S1);  myFile.print(",");
      myFile.print(S2);  myFile.print(",");
      myFile.print(S3);  myFile.print(",");
      myFile.print(S4);  myFile.print(",");
      myFile.print(S5);  myFile.print(",");
      myFile.print(S6);  myFile.print(",");
      myFile.print(P1);  myFile.print(",");
      myFile.print(P2);  myFile.print(",");
      myFile.print(P3);  myFile.print(",");
      myFile.print(P4);  myFile.print(",");
      myFile.println(P5);
      myFile.close();
    } else {
      Serial.println("SD write error");
      digitalWrite(ledPin, LOW);
    }
  }
}

// Open a SoftwareSerial port, read any float, then close it immediately
float readPWM(SoftwareSerial& serialPort) {
  serialPort.begin(9600);  // match your transmitter’s baud
  delay(5);                // let it settle

  float val = -1;
  if (serialPort.available()) {
    val = serialPort.parseFloat();
  }

  serialPort.end();
  return val;
}
