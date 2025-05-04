#include <SoftwareSerial.h>

// RPM values and timing  
float        rpm1 = 0, rpm2 = 0;
unsigned long lastTime1 = 0, lastTime2 = 0;

// Two‑zero detection state  
int   zeros1 = 0, zeros2 = 0;  
bool  armed1 = true, armed2 = true;

// “TX unused” SoftwareSerial instances  
SoftwareSerial P1(-1, 3); // send rpm1 on pin 3  

String data;

void setup() {
  Serial.begin(9600);
  P1.begin(9600);
  
  pinMode(13, OUTPUT);
}

void loop() {
  // 1) read raw analog from your AIM sensors
  int reading1 = analogRead(A3);  
  int reading2 = analogRead(A5);

  // 2) SENSOR1: count zeros in a row, re‑arm when non‑zero
  if (reading1 == 0) {
    zeros1++;
  } else {
    zeros1 = 0;
    armed1 = true;
  }
  // trigger on two consecutive zeros
  if (zeros1 >= 2 && armed1) {
    unsigned long now = micros();
    if (lastTime1 > 0) {
      rpm1 = 60000000.0 / (now - lastTime1);
      rpm1 = min(rpm1, 3800.0);
    }
    lastTime1 = now;
    digitalWrite(13, HIGH);
    armed1 = false;
  }

  // 3) SENSOR2: same two‑zero logic
  if (reading2 == 0) {
    zeros2++;
  } else {
    zeros2 = 0;
    armed2 = true;
  }
  if (zeros2 >= 2 && armed2) {
    unsigned long now = micros();
    if (lastTime2 > 0) {
      rpm2 = 60000000.0 / (now - lastTime2);
      rpm2 = min(rpm2, 3800.0);
    }
    lastTime2 = now;
    armed2 = false;
  }

  // 4) send RPMs over SoftwareSerial
  data = String(rpm1, 1);
  data += ",";
  data += String(rpm2, 1);
  P1.println(data);
  

  // 5) turn off LED after blink
  digitalWrite(13, LOW);
}
