#include <EEPROM.h>
#include <TinyGPSPlus.h>

#include <Usb.h>
#include <usbhub.h>
#include <masstorage.h>
#include <Storage.h>
#include <PCPartition/PCPartition.h>
#include <FAT/FAT.h>

/*
  Mega + USB Host + generic_storage → DAQ with RPM + GPS + “heartbeat” LED

  • Mounts the first FAT16/FAT32 partition on a USB stick
  • In setup(): creates (or overwrites) “0:/DATA.CSV” and writes a header:
      TIME, SUS1, SUS2, SUS3, SUS4,  ACCL1, ACCL2, ACCL3,  MPH, LAT, LON, RPM1, RPM2
  • In loop(): every 1000 ms:
       – sample SUS1–SUS4 (A0–A3), ACCL1–ACCL3 (A4–A6)
       – read any GPS bytes, update latitude/longitude/speed
       – latch and reset the interrupt-driven RPM counters for two channels
       – append one CSV line containing all of the above
       – flash pin 13 HIGH for 100 ms as a “heartbeat”
*/

// -------------- RTClib stubs so FAT.cpp does not look for an RTC chip  --------------
//    (We never use real timestamps in the CSV, we log “millis()/1000” ourselves.)
#include <RTClib.h>
DateTime::DateTime(int32_t t)  { /* no-op */ }
DateTime::DateTime(time_t   t)  { /* no-op */ }
time_t DateTime::FatPacked()  { return (time_t)0; }
DateTime RTCnow()  { return DateTime((int32_t)0); }

// -------------- TinyGPS++ for parsing GPS NMEA from Serial1  --------------
TinyGPSPlus gps;

// -------------- SPI / FatFs objects  --------------
static USB      Usb;
USBHub         Hub1(&Usb);
static PFAT   *Fats[_VOLUMES];
static part_t  parts[_VOLUMES];
static storage_t sto[_VOLUMES];
static PCPartition *PT;
static bool    partsready = false;
static bool    fatready  = false;
static int     cpart     = 0;

// -------------- RPM (pulse) inputs  --------------
//   We assume two sensors wired to digital pins 2 and 3 (INT0, INT1) with pulldown.
//   Each pulse increments a counter; we debounce edges < 1000 µs apart.
volatile unsigned int pulse1Count = 0;
volatile unsigned int pulse2Count = 0;
volatile unsigned long last1Micros = 0;
volatile unsigned long last2Micros = 0;
static const unsigned long DEBOUNCE_US = 1000UL;  // ignore pulses <1 ms apart

// -------------- “Heartbeat” LED  --------------
static const int HEARTBEAT_LED = 13;

// -------------- Helper: detect if a partition type is FAT16/32  --------------
bool isfat(uint8_t t) {
  return (t == 0x01 || t == 0x04 || t == 0x06 ||
          t == 0x0B || t == 0x0C || t == 0x0E ||
          t == 0x0F || t == 0x1);
}

// -------------- Interrupt service routines for RPM  --------------
void pulse1Detected() {
  unsigned long now = micros();
  if (now - last1Micros < DEBOUNCE_US) return;
  last1Micros = now;
  pulse1Count++;
}

void pulse2Detected() {
  unsigned long now = micros();
  if (now - last2Micros < DEBOUNCE_US) return;
  last2Micros = now;
  pulse2Count++;
}

void setup() {
  // 1) Start USB/Serial Monitor on pins 0/1 for debugging
  Serial.begin(115200);
  while (!Serial) { }

  // 2) Configure the “heartbeat” LED
  pinMode(HEARTBEAT_LED, OUTPUT);
  digitalWrite(HEARTBEAT_LED, LOW);

  // 3) Configure RPM interrupt‐inputs on pins 2 & 3
  pinMode(2, INPUT_PULLUP);  // Sensor 1 pulse (INT0)
  pinMode(3, INPUT_PULLUP);  // Sensor 2 pulse (INT1)
  attachInterrupt(digitalPinToInterrupt(2), pulse1Detected, RISING);
  attachInterrupt(digitalPinToInterrupt(3), pulse2Detected, RISING);

  // 4) Start GPS listener on Serial1 (pins 19=RX1, 18=TX1); GPS TX should be wired to RX1
  Serial1.begin(9600);

  // 5) Initialize USB Host + generic_storage (FatFs) to mount a USB stick
  for (int i = 0; i < _VOLUMES; i++) {
    Fats[i] = nullptr;
    sto[i].private_data = new pvt_t;
    ((pvt_t *)sto[i].private_data)->B = 255;
  }
  Init_Generic_Storage();

  // 6) Try to init the Host Shield; keep retrying until it succeeds
  while (Usb.Init(200) == -1) {
    Serial.println(F("No USB Host Shield detected—retrying…"));
    delay(500);
  }
  Serial.println(F("USB Host Shield initialized."));

  // 7) Scan for the first FAT16/32 partition, mount it as volume 0
  while (!fatready) {
    Usb.Task();  // drive the USB state machine

    for (int B = 0; B < MAX_USB_MS_DRIVERS; B++) {
      if (UHS_USB_BulkOnly[B]->GetAddress() == 0) continue;
      int maxLUN = UHS_USB_BulkOnly[B]->GetbMaxLUN();
      for (int lun = 0; lun <= maxLUN; lun++) {
        if (!UHS_USB_BulkOnly[B]->LUNIsGood(lun)) continue;

        partsready = true;
        ((pvt_t *)(sto[lun].private_data))->lun = lun;
        ((pvt_t *)(sto[lun].private_data))->B   = B;
        sto[lun].Reads      = *UHS_USB_BulkOnly_Read;
        sto[lun].Writes     = *UHS_USB_BulkOnly_Write;
        sto[lun].Status     = *UHS_USB_BulkOnly_Status;
        sto[lun].Initialize = *UHS_USB_BulkOnly_Initialize;
        sto[lun].Commit     = *UHS_USB_BulkOnly_Commit;
        sto[lun].TotalSectors = UHS_USB_BulkOnly[B]->GetCapacity(lun);
        sto[lun].SectorSize   = UHS_USB_BulkOnly[B]->GetSectorSize(lun);

        PT = new PCPartition;
        if (PT->Init(&sto[lun]) == 0) {
          // If there’s an MBR, scan up to 4 entries for FAT
          for (int pi = 0; pi < 4; pi++) {
            part_t *entry = PT->GetPart(pi);
            if (entry && entry->type != 0x00 && isfat(entry->type)) {
              memcpy(&parts[cpart], entry, sizeof(part_t));
              Fats[cpart] = new PFAT(&sto[lun], cpart, parts[cpart].firstSector);
              if (Fats[cpart]->MountStatus()) {
                delete Fats[cpart];
                Fats[cpart] = nullptr;
              } else {
                cpart++;
              }
            }
          }
        } else {
          // No MBR → superfloppy mode: treat sector 0 as FAT
          Fats[cpart] = new PFAT(&sto[lun], cpart, 0);
          if (Fats[cpart]->MountStatus()) {
            delete Fats[cpart];
            Fats[cpart] = nullptr;
          } else {
            cpart++;
          }
        }
        delete PT;
      }
    }

    if (partsready && cpart > 0) {
      fatready = true;
      Serial.println(F("→ FAT partition successfully mounted."));
      break;
    }
  }

  // 8) At this point, volume “0:” is ready. Create (or overwrite) “0:/DATA.CSV”
  //    and write a single header line.
  {
    // Find the FATFS* for volume 0
    FATFS *fs = nullptr;
    for (int i = 0; i < cpart; i++) {
      if (Fats[i]->volmap == 0) {
        fs = Fats[i]->ffs;
        break;
      }
    }
    if (!fs) {
      Serial.println(F("Error: cannot find FATFS for volume 0."));
      while (1);
    }

    FIL    csvFile;
    FRESULT rc;
    UINT   bw;

    rc = f_open(&csvFile, "0:/DATA.CSV", FA_CREATE_ALWAYS | FA_WRITE);
    if (rc) {
      Serial.print(F("f_open(header) failed, rc="));
      Serial.println((uint32_t)rc);
      while (1);
    }
    // Write a header with all the columns we’ll log:
    const char *header = 
      "TIME, SUS1, SUS2, SUS3, SUS4,  ACCL1, ACCL2, ACCL3,  MPH, LAT, LON, RPM1, RPM2\r\n";

    rc = f_write(&csvFile, header, strlen(header), &bw);
    f_close(&csvFile);
    if (rc || bw < strlen(header)) {
      Serial.print(F("Header write failed, rc="));
      Serial.println((uint32_t)rc);
      while (1);
    }
    Serial.println(F("→ Header written to DATA.CSV"));
  }
} // end setup()

void loop() {
  static unsigned long lastMillis = 0;
  unsigned long now = millis();

  // -------------- 1) Poll GPS serial and feed bytes to TinyGPS++
  while (Serial1.available()) {
    char c = Serial1.read();
    gps.encode(c);
  }

  // -------------- 2) Once per second, build + append one CSV line
  if (now - lastMillis >= 250) {
    lastMillis = now;

    // 2a) Sample the four suspension sensors (A0–A3)
    int sus1 = analogRead(A0);
    int sus2 = analogRead(A1);
    int sus3 = analogRead(A2);
    int sus4 = analogRead(A3);

    // 2b) Sample the three accelerometers (A4–A6)
    int accl1 = analogRead(A4);
    int accl2 = analogRead(A5);
    int accl3 = analogRead(A6);

    // 2c) Extract GPS‐derived speed, latitude, longitude if a new fix is available
    double lat = 0.0, lon = 0.0, mph = 0.0;
    if (gps.location.isUpdated()) {
      lat = gps.location.lat();
      lon = gps.location.lng();
      mph = gps.speed.mph();
    }

    // 2d) Calculate RPMs from the interrupt counters.
    //     Each “pulseCount” has been counting edges since the last loop.
    //     We do “/2” if your sensor produces two edges per revolution;
    //     multiply by 60 to get rev/min, then by 1 (since 1 s elapsed).
    unsigned int count1 = pulse1Count;
    unsigned int count2 = pulse2Count;
    pulse1Count = 0;
    pulse2Count = 0;

    // Example: if your pickup gives one rising edge per half‐revolution,
    // then (count1/2)*60 = RPM. Adjust “/2” if your sensor is different.
    double rpm1 = (count1 / 2.0) * 60.0;
    double rpm2 = (count2 / 2.0) * 60.0;

    // 2e) Open “0:/DATA.CSV”, seek to end, and append one line
    {
      FIL    csvFile;
      FRESULT rc;
      UINT   bw;

      rc = f_open(&csvFile, "0:/DATA.CSV", FA_OPEN_EXISTING | FA_WRITE);
      if (rc == FR_NO_FILE) {
        // (If somehow the file vanished, you could recreate header here.
        // But for now just bail.)
        Serial.println(F("ERROR: DATA.CSV missing!"));
        return;
      }
      if (rc) {
        Serial.print(F("ERROR: f_open(append) failed, rc="));
        Serial.println((uint32_t)rc);
        return;
      }

      // Seek to EOF
      DWORD fileSize = f_size(&csvFile);
      rc = f_lseek(&csvFile, fileSize);
      if (rc) {
        Serial.print(F("ERROR: f_lseek to EOF failed, rc="));
        Serial.println((uint32_t)rc);
        f_close(&csvFile);
        return;
      }

      // Build a single CSV‐formatted line with all 13 fields:
      //   TIME, SUS1, SUS2, SUS3, SUS4,  ACCL1, ACCL2, ACCL3,  MPH, LAT, LON, RPM1, RPM2
      char lineBuf[128];
      int len = snprintf(lineBuf, sizeof(lineBuf),
        "%8lu, %4d, %4d, %4d, %4d,   %4d, %4d, %4d,   %5.1f,  %9.6f, %9.6f,  %.1f,  %.1f\r\n",
        now / 1000.0,  // TIME in seconds
        sus1, sus2, sus3, sus4,
        accl1, accl2, accl3,
        mph,
        lat,
        lon,
        rpm1,
        rpm2
      );

      // Write the full line
      rc = f_write(&csvFile, lineBuf, len, &bw);
      Serial.print(F("→ "));
      Serial.print(lineBuf);  // also echo to USB Serial Monitor
      if (rc || bw < (UINT)len) {
        Serial.print(F("ERROR: f_write failed, rc="));
        Serial.println((uint32_t)rc);
        f_close(&csvFile);
        return;
      }

      // Close (flush to disk)
      rc = f_close(&csvFile);
      if (rc) {
        Serial.print(F("ERROR: f_close failed, rc="));
        Serial.println((uint32_t)rc);
        return;
      }

      // 2f) Blink the LED as a “heartbeat” indicator (turn on 100 ms)
      digitalWrite(HEARTBEAT_LED, HIGH);
      delay(20);
      digitalWrite(HEARTBEAT_LED, LOW);
    }
  } // end “if (1 s elapsed)”
} // end loop()
