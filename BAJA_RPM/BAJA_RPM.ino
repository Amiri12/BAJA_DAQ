#include <SoftwareSerial.h>

float rpm1, rpm2;
unsigned long timeold1, timeold2;
int out = 0;

SoftwareSerial P1(-1, 3);
SoftwareSerial P2(-1, 5);
SoftwareSerial P3(-1, 6);
SoftwareSerial P4(-1, 7);
SoftwareSerial P5(-1, 8);


void setup() {
  Serial.begin(9600);
  P1.begin(9600);
  P2.begin(9600);
  P3.begin(9600);
  P4.begin(9600);
  P5.begin(9600);
  pinMode(13, OUTPUT);
}

void loop() {
  static bool lastState1 = false;
  static bool lastState2 = false;
  bool currentState1 = analogRead(A3) == 0;
  bool currentState2 = analogRead(A5) < 400;

  int X_Accel = map(analogRead(A2), 438, 295, 1000, -1000);
  int Y_Accel = map(analogRead(A1), 438, 295, 1000, -1000);
  int Z_Accel = map(analogRead(A0), 438, 295, 1000, -1000);

  if (currentState1 && !lastState1) {
    unsigned long currentTime = micros();
    if (currentTime > timeold1) {
      digitalWrite(13, HIGH);
      rpm1 = min(3800, (60000000.0 / (currentTime - timeold1)));
      timeold1 = currentTime;
    }
  }
  
  if (currentState2 && !lastState2) {
    unsigned long currentTime = micros();
    if (currentTime > timeold2) {
      rpm2 = min(3800, (60000000.0 / (currentTime - timeold2)));
      timeold2 = currentTime;
    }
  }


  P1.println(rpm1);
  P2.println(rpm2);
  P3.println(X_Accel);
  P4.println(Y_Accel);
  P5.println(Z_Accel);
  digitalWrite(13, LOW);

  lastState1 = currentState1;
  lastState2 = currentState2;
}
