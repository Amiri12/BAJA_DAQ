#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <SD.h>
#include <EEPROM.h>

// SD‐card and LED diagnostics
const int chipSelect = 10;
const int ledPin     = 9;

// File handle and run counter
File    myFile;
int     Num;
String  fileName;

// “Analog” inputs S1–S4 on A0–A3, S7/S8 on A6–A7
int   S1, S2, S3, S4;
int   S7, S8;

// MPU data
float AccX, AccY, AccZ;
float RotX, RotY, RotZ;

// MPU instance
Adafruit_MPU6050 mpu;

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
  while (!Serial);

  // — MPU6050 init on hardware I²C (A4=SDA, A5=SCL) —
  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1) { digitalWrite(ledPin, LOW); delay(100); }
  }

  // — SD setup —
  Serial.print("Initializing SD card...");
  digitalWrite(ledPin, LOW);
  Num = EEPROM.read(0);
  fileName = "log" + String(Num) + ".csv";
  EEPROM.update(0, Num + 1);
  if (!SD.begin(chipSelect)) {
    Serial.println(" failed!");
    while (1) { digitalWrite(ledPin, LOW); }
  }
  digitalWrite(ledPin, HIGH);
  Serial.println(" done.");
  myFile = SD.open(fileName, FILE_WRITE);
  if (myFile) {
    myFile.println(
      "Time,"
      "S1,S2,S3,S4,"
      "AccelX,AccelY,AccelZ,"
      "GyroX,GyroY,GyroZ,"
      "S7,S8"
    );
    myFile.close();
  } else {
    Serial.println("Error opening file!");
    while (1) { digitalWrite(ledPin, LOW); }
  }
}

void loop() {
  static unsigned long lastWriteTime = 0;
  const unsigned long writeInterval = 40; // ms
  unsigned long t = millis();

  // 1) Read analog inputs A0–A3
  S1 = analogRead(A0);
  S2 = analogRead(A1);
  S3 = analogRead(A2);
  S4 = analogRead(A3);

  // 2) Read MPU data
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  AccX = a.acceleration.x;
  AccY = a.acceleration.y;
  AccZ = a.acceleration.z;
  RotX  = g.gyro.x;
  RotY  = g.gyro.y;
  RotZ  = g.gyro.z;

  // 3) Read analog‐only pins A6/A7
  S7 = analogRead(A6);
  S8 = analogRead(A7);

  // 4) Throttle and write to SD
  if (t - lastWriteTime >= writeInterval) {
    lastWriteTime = t;
    myFile = SD.open(fileName, FILE_WRITE);
    if (myFile) {
      myFile.print(t);      myFile.print(',');
      myFile.print(S1);     myFile.print(',');
      myFile.print(S2);     myFile.print(',');
      myFile.print(S3);     myFile.print(',');
      myFile.print(S4);     myFile.print(',');
      myFile.print(AccX, 3); myFile.print(',');
      myFile.print(AccY, 3); myFile.print(',');
      myFile.print(AccZ, 3); myFile.print(',');
      myFile.print(RotX, 3); myFile.print(',');
      myFile.print(RotY, 3); myFile.print(',');
      myFile.print(RotZ, 3); myFile.print(',');
      myFile.print(S7);     myFile.print(',');
      myFile.println(S8);
      myFile.close();
    } else {
      Serial.println("SD write error");
      digitalWrite(ledPin, LOW);
    }
  }
}
