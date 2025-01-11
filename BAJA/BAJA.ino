#include <SD.h>
#include <EEPROM.h>

const int chipSelect = 10;
const int ledPin = 7; // LED for SD card diagnostics
File myFile;
int runNum = 0;
int Num;
String fileName;
int S1, S2, S3, S4, S5, S6; // Analog input values
int P1, P2, P3, P4, P5, P6; // PWM input pulse widths
String time;

void setup() {
  pinMode(8, INPUT);
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  pinMode(11, INPUT);
  pinMode(12, INPUT);
  pinMode(13, INPUT);
  pinMode(ledPin, OUTPUT);

  Serial.begin(9600);
  while (!Serial); // Wait for Serial Monitor to connect (needed for native USB boards)

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
    myFile.println("Time, S1, S2, S3, S4, S5, S6, P1, P2, P3, P4, P5, P6");
    myFile.close();
  } else {
    Serial.println("Error opening file for writing.");
    digitalWrite(ledPin, HIGH);
    while (1); // Halt further execution
  }

  // Original code:
  // digitalWrite(ledPin, LOW); // Assume SD card initialization failure initially
  // digitalWrite(ledPin, LOW); // Turn off LED, SD initialization successful
  // digitalWrite(ledPin, HIGH); // Turn on LED for error
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

  // Measure pulse widths (PWM signals)
  P1 = pulseIn(8, HIGH, 20000); // Example timeout: 20 ms
  P2 = pulseIn(9, HIGH, 20000);
  P3 = pulseIn(10, HIGH, 20000);
  P4 = pulseIn(11, HIGH, 20000);
  P5 = pulseIn(12, HIGH, 20000);
  P6 = pulseIn(13, HIGH, 20000);

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
      myFile.println();
      myFile.close();
    } else {
      Serial.println("Error writing to file.");
      digitalWrite(ledPin, LOW); // Indicate SD card write failure
    }

    // Original code:
    // digitalWrite(ledPin, HIGH); // Indicate SD card write failure
  }
}

/*
#include <SD.h>
#include <EEPROM.h>
const int chipSelect = 10;
File myFile;
int runNum = 0;
int Num;
String fileName;
int S1,S2,S3,S4,S5,S6,P1,P2,P3,P4,P5,P6;
String time;

void setup() {
  pinMode(8,INPUT);
  pinMode(9,INPUT);
  pinMode(10,INPUT);
  pinMode(11,INPUT);
  pinMode(12,INPUT);
  pinMode(13,INPUT);
  // put your setup code here, to run once:
  Serial.begin(9600);
  // wait for Serial Monitor to connect. Needed for native USB port boards only:
  while (!Serial);

  Serial.print("Initializing SD card...");
  Num = EEPROM.read(0);
  fileName = "log"+String(Num)+".csv";
  Num+=1;
  EEPROM.update(0, Num);

  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("1. is a card inserted?");
    Serial.println("2. is your wiring correct?");
    Serial.println("3. did you change the chipSelect pin to match your shield or module?");
    Serial.println("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!");
    while (true);
  }
  myFile = SD.open(fileName, FILE_WRITE);
  myFile.println("S1,S2,S3,S4,S5,S6,Time");
  myFile.close();
  Serial.println(fileName);
  Serial.println("initialization done.");

  while(true){
    time = String(millis());
    S1=analogRead(A0);
    S2=analogRead(A1);
    S3=analogRead(A2);
    S4=analogRead(A3);
    S5=analogRead(A4);
    S6=analogRead(A5);
    P1=pulseIn(8, HIGH);
    P2=pulseIn(9, HIGH);
    P3=pulseIn(10, HIGH);
    P4=pulseIn(11, HIGH);
    P5=pulseIn(12, HIGH);
    P6=pulseIn(13, HIGH);
    myFile = SD.open(fileName, FILE_WRITE);
      if(myFile){
        myFile.println(String(S1) + "," + String(S2) + "," + String(S3) + "," + String(S4) + "," + String(S5) + "," + String(S6) + "," + String(P1) + "," + String(P2) + "," + String(P3) + "," + String(P4) + "," + String(P5) + "," + String(P6) + "," + time);
        myFile.close();
    }
  }
  }



void loop() {
  // put your main code here, to run repeatedly:





}
*/