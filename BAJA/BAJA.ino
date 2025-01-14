#include <SD.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

const int chipSelect = 10;
const int ledPin = 2; // LED for SD card diagnostics
File myFile;
int runNum = 0;
int Num;
String fileName;
int S1, S2, S3, S4, S5, S6; // Analog input values
float P1, P2, P3, P4, P5, P6; // PWM input pulse widths
String GPS;
String time;

// Define SoftwareSerial instances for PWM inputs
SoftwareSerial pwm1(4, -1); // RX on pin 8, TX not used
SoftwareSerial pwm2(5, -1); // RX on pin 9, TX not used
SoftwareSerial pwm3(6, -1); // RX on pin 10, TX not used
SoftwareSerial pwm4(7, -1); // RX on pin 11, TX not used
SoftwareSerial pwm5(8, -1); // RX on pin 12, TX not used
SoftwareSerial pwm6(9, -1); // RX on pin 13, TX not used
SoftwareSerial GPSInfo(3, -1);


void setup() {
  pinMode(ledPin, OUTPUT);

  Serial.begin(9600);
  while (!Serial); // Wait for Serial Monitor to connect (needed for native USB boards)

  // Start SoftwareSerial instances
  pwm1.begin(9600);
  pwm2.begin(9600);
  pwm3.begin(9600);
  pwm4.begin(9600);
  pwm5.begin(9600);
  pwm6.begin(9600);
  GPSInfo.begin(9600);

  Serial.print("Initializing SD card...");
  digitalWrite(ledPin, HIGH); // Assume SD card initialization failure initially

  Num = EEPROM.read(0);
  fileName = "log" + String(Num) + ".csv";
  Num += 1;
  EEPROM.update(0, Num);

  if (!SD.begin(chipSelect)) {
    Serial.println("Initialization failed.");
    digitalWrite(ledPin, LOW); // Turn off LED for error
    while (1); // Halt further execution
  }
  digitalWrite(ledPin, HIGH); // Turn on LED, SD initialization successful
  Serial.println("Initialization done.");

  myFile = SD.open(fileName, FILE_WRITE);
  if (myFile) {
    myFile.println("Time, S1, S2, S3, S4, S5, S6, P1, P2, P3, P4, P5, P6, lat, lng, head, speed");
    myFile.close();
  } else {
    Serial.println("Error opening file for writing.");
    digitalWrite(ledPin, HIGH);
    while (1); // Halt further execution
  }
}

void loop() {
  static unsigned long lastWriteTime = 0;
  const unsigned long writeInterval = 100; // Write to SD every 100 ms

  // Read analog inputs
  S1 = analogRead(A0);
  S2 = analogRead(A1);
  S3 = analogRead(A2);
  S4 = analogRead(A3);
  S5 = analogRead(A4);
  S6 = analogRead(A5);

  // Read PWM signals using SoftwareSerial
  P1 = readPWM(pwm1);
  P2 = readPWM(pwm2);
  P3 = readPWM(pwm3);
  P4 = readPWM(pwm4);
  P5 = readPWM(pwm5);
  P6 = readPWM(pwm6);

  // Read GPS data
  if(GPSInfo.available()){
    GPS = GPSInfo.readString();
    GPS.trim();
  } else {
    GPS = "-1";
  }
  unsigned long currentTime = millis();

  // Buffer data to minimize SD card writes
  if (currentTime - lastWriteTime >= writeInterval) {
    lastWriteTime = currentTime;
    myFile = SD.open(fileName, FILE_WRITE);
    if (myFile) {
      myFile.print(currentTime);
      myFile.print(",");
      myFile.print(S1);
      myFile.print(",");
      myFile.print(S2);
      myFile.print(",");
      myFile.print(S3);
      myFile.print(",");
      myFile.print(S4);
      myFile.print(",");
      myFile.print(S5);
      myFile.print(",");
      myFile.print(S6);
      myFile.print(",");
      myFile.print(P1);
      myFile.print(",");
      myFile.print(P2);
      myFile.print(",");
      myFile.print(P3);
      myFile.print(",");
      myFile.print(P4);
      myFile.print(",");
      myFile.print(P5);
      myFile.print(",");
      myFile.print(P6);
      myFile.print(",");
      myFile.print(GPS);
      myFile.println();
      myFile.close();
    } else {
      Serial.println("Error writing to file.");
      digitalWrite(ledPin, LOW); // Indicate SD card write failure
    }
  }
}

float readPWM(SoftwareSerial& serialPort) {
  if (serialPort.available()) {
    return serialPort.parseFloat(); // Parse integer from incoming data
  } else {
    return -1; // Return -1 if no data is available
  }
}


