float rpm;
unsigned long timeold;
int out = 0;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(3, OUTPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
  
  static bool lastState = false;
  bool currentState = analogRead(A6) < 790;
  int X_Accel = map(anologRead(A2), 0, 1024, 0, 255);
  int Y_Accel = map(analogRead(A3), 0, 1024, 0, 255);
  int Z_Accel = map(anaolgRead(A4), 0, 1024, 0, 255);
  if (currentState && !lastState) {
    if (analogRead(A6)<790){
      Serial.println(analogRead(A6));
      unsigned long currentTime = micros();
      if (currentTime > timeold) { // Ensure time difference is valid
        rpm = min(3800,(60000000.0 / (currentTime - timeold)));
        Serial.print(rpm);
        Serial.print("\t");
        Serial.println(out);
         // Calculate RPM
        timeold = currentTime; // Update timeold for the next calculation
      }

    }
  }
  
  out = map(rpm, 0, 3800, 0, 255);
  analogWrite(3, out);
  analogWrite(5, );
  
  lastState = currentState;
}
