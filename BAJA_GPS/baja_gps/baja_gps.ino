#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

SoftwareSerial GPSData(4,6);
SoftwareSerial GPSOUT(-1, 9);

String pps = "";

TinyGPSPlus gps;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  GPSData.begin(9600);
  GPSOUT.begin(9600);

}

void loop() {
double lat = random(36000, 36999)/1000.000 ;
double lon = random(-79000, -78000)/1000.000 ;
double head = random(0, 3590)/10.0 ;
int speed = random(0,30);
int alt = random(0,100);
  // put your main code here, to run repeatedly:

  pps = String(lat, 3) + "," + String(lon, 3) + "," +String(head, 1) + "," +String(speed) + "," +String(alt);
  //pps = "HI";
  GPSOUT.println(pps);

  delay(1000);
}
