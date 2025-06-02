#include <EEPROM.h>

/*
  Mega + USB Host + generic_storage → Log A0/A1 to CSV every 1 s
  ----------------------------------------------------------------
  1) Mounts the first FAT16/FAT32 partition on a USB stick
  2) In setup(): creates (or overwrites) “0:/DATA.CSV” and writes a header:
       Time_s,Analog0,Analog1
  3) In loop(): every 1000 ms, appends a new line:
       <seconds>,<analogRead(A0)>,<analogRead(A1)>
  4) Closes the file each time to ensure data is flushed

  Required libraries (installed via Library Manager or cloned into Documents/Arduino/libraries/):
    • USB_Host_Shield_2.0
    • xmem2                    (for external RAM; generic_storage needs it)
    • generic_storage (by xxxajk) → Storage.h, PCPartition, FAT…
    • RTClib                  (only so FAT.cpp’s timestamp calls are satisfied)

  Since we don’t actually care about real dates, we stub out the RTClib calls:
    – DateTime::DateTime(int32_t)
    – DateTime::DateTime(time_t)
    – time_t   DateTime::FatPacked()
    – DateTime RTCnow()

  That way, FAT.cpp never fails on missing RTC symbols.
*/


#include <RTClib.h>  // pulls in DateTime

// -------------------------------
// STUB #1: DateTime::DateTime(int32_t)
// -------------------------------
DateTime::DateTime(int32_t t) {
  // no-op
}

// -------------------------------
// STUB #2: DateTime::DateTime(time_t)
// -------------------------------
DateTime::DateTime(time_t t) {
  // no-op
}

// -------------------------------
// STUB #3: DateTime::FatPacked()
// -------------------------------
time_t DateTime::FatPacked() {
  return (time_t)0;
}

// -------------------------------
// STUB #4: RTCnow()
// -------------------------------
DateTime RTCnow() {
  // Force the 32-bit‐int overload; FAT.cpp only needs *something* of type DateTime
  return DateTime((int32_t)0);
}

// Now pull in all of the USB/Storage/FAT headers. Our four stubs above satisfy FAT.cpp’s
// calls to RTCnow(), FatPacked(), and the two DateTime constructors.

#include <Usb.h>
#include <usbhub.h>
#include <masstorage.h>               // Mass-Storage (MSC)
#include <Storage.h>                  // generic_storage core
#include <PCPartition/PCPartition.h>  // partition scanning
#include <FAT/FAT.h>                  // FatFS API (f_open, f_write, f_close, etc.)

// -----------------------------------------------------------------------------------
// generic_storage / FAT support variables
// -----------------------------------------------------------------------------------
static USB      Usb;                  // USB Host core
USBHub         Hub1(&Usb);             // only if your shield has a built-in hub
static PFAT   *Fats[_VOLUMES];         // one PFAT* per logical FAT volume
static part_t  parts[_VOLUMES];        // partition table entries
static storage_t sto[_VOLUMES];        // storage callbacks + capacity, etc.

static PCPartition *PT;                // for partition enumeration
static bool    partsready = false;     // true once we detect ≥1 valid LUN
static bool    fatready  = false;     // true once we successfully mount a PFAT
static int     cpart     = 0;          // how many partitions we’ve mounted

// -----------------------------------------------------------------------------------
// Helper: check if a partition type byte corresponds to FAT16/FAT32
// -----------------------------------------------------------------------------------
bool isfat(uint8_t t) {
  return (t == 0x01 || t == 0x04 || t == 0x06 ||
          t == 0x0B || t == 0x0C || t == 0x0E ||
          t == 0x0F || t == 0x1);
}


char filename[32];

void setup() {
    
    int someNumber = EEPROM.read(0);
    EEPROM.write(0, someNumber++);

  // 1) Create a char buffer large enough for the path + name + null terminator
  //    e.g. "0:/LOG_1234.CSV" is 15 chars including null, so pick 32 to be safe.
  

  // 2) Populate it. snprintf returns the number of bytes written (minus null).
  //    Format: "0:/PREFIX_%d.CSV"
  snprintf(filename, sizeof(filename), "0:/LOG_%d.CSV", someNumber);
  Serial.begin(115200);
  Serial.print(filename);
  while (!Serial) { }  // wait for Serial to initialize

  // ------------------------------------------------------------------------
  // 1) Prepare arrays so we can later mount partitions
  // ------------------------------------------------------------------------
  for (int i = 0; i < _VOLUMES; i++) {
    Fats[i] = nullptr;
    // Allocate private_data for each storage_t (generic_storage expects a pvt_t*)
    sto[i].private_data = new pvt_t;
    ((pvt_t *)sto[i].private_data)->B = 255;  // “invalid” until we fill it in
  }

  // ------------------------------------------------------------------------
  // 2) Initialize generic_storage BEFORE starting USB
  // ------------------------------------------------------------------------
  Init_Generic_Storage();

  // ------------------------------------------------------------------------
  // 3) Start USB Host Shield. Retry until it appears.
  // ------------------------------------------------------------------------
  while (Usb.Init(200) == -1) {
    Serial.println(F("No USB Host Shield detected—retrying..."));
    delay(500);
  }
  Serial.println(F("USB Host Shield initialized."));

  // ------------------------------------------------------------------------
  // 4) Poll & mount the first FAT16/32 partition on any plugged-in MSC device
  // ------------------------------------------------------------------------
  while (!fatready) {
    Usb.Task();  // service USB state machine

    // Cycle through all possible MSC slots
    for (int B = 0; B < MAX_USB_MS_DRIVERS; B++) {
      if (UHS_USB_BulkOnly[B]->GetAddress() == 0) {
        // no device at this index
        continue;
      }

      // Found an MSC device at index B. Query its LUNs (logical unit numbers)
      int maxLUN = UHS_USB_BulkOnly[B]->GetbMaxLUN();
      for (int lun = 0; lun <= maxLUN; lun++) {
        if (!UHS_USB_BulkOnly[B]->LUNIsGood(lun)) {
          continue;
        }

        partsready = true;  // we saw at least one valid LUN

        // Fill in the storage_t callbacks/info for this LUN
        ((pvt_t *)(sto[lun].private_data))->lun = lun;
        ((pvt_t *)(sto[lun].private_data))->B   = B;
        sto[lun].Reads      = *UHS_USB_BulkOnly_Read;
        sto[lun].Writes     = *UHS_USB_BulkOnly_Write;
        sto[lun].Status     = *UHS_USB_BulkOnly_Status;
        sto[lun].Initialize = *UHS_USB_BulkOnly_Initialize;
        sto[lun].Commit     = *UHS_USB_BulkOnly_Commit;
        sto[lun].TotalSectors = UHS_USB_BulkOnly[B]->GetCapacity(lun);
        sto[lun].SectorSize   = UHS_USB_BulkOnly[B]->GetSectorSize(lun);

        // Attempt to read a partition table on this LUN:
        PT = new PCPartition;
        if (PT->Init(&sto[lun]) == 0) {
          // Valid MBR—scan up to 4 partitions
          for (int pi = 0; pi < 4; pi++) {
            part_t *entry = PT->GetPart(pi);
            if (entry && entry->type != 0x00 && isfat(entry->type)) {
              // Copy the partition descriptor and attempt to mount via PFAT
              memcpy(&parts[cpart], entry, sizeof(part_t));
              Fats[cpart] = new PFAT(&sto[lun], cpart, parts[cpart].firstSector);
              if (Fats[cpart]->MountStatus()) {
                // MountStatus()!=0 means “failed to mount”: delete & skip
                delete Fats[cpart];
                Fats[cpart] = nullptr;
              } else {
                cpart++;
              }
            }
          }
        } else {
          // No partition table—try “superfloppy” mode (sector 0 = FAT)
          Fats[cpart] = new PFAT(&sto[lun], cpart, 0);
          if (Fats[cpart]->MountStatus()) {
            delete Fats[cpart];
            Fats[cpart] = nullptr;
          } else {
            cpart++;
          }
        }
        delete PT;
      } // for each LUN
    }   // for each MSC driver index

    if (partsready && cpart > 0) {
      fatready = true;
      Serial.println(F("→ FAT partition mounted successfully."));
      break;
    }
  } // while(!fatready)

  // ------------------------------------------------------------------------
  // 5) At this point, Fats[0] is the first mounted FAT volume (drive “0:”).
  //    Create “0:/DATA.CSV” (overwrite), and write a single header row:
  //       Time_s,Analog0,Analog1
  // ------------------------------------------------------------------------
  {
    // Find the FATFS pointer for volume 0
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

    // Open or create “0:/DATA.CSV”—use FA_CREATE_ALWAYS to overwrite
    rc = f_open(&csvFile, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (rc) {
      Serial.print(F("f_open failed (header), rc="));
      Serial.println((uint32_t)rc);
      while (1);
    }

    // Write the header row
    // “Time_s” = seconds since Arduino started. “Analog0” = A0, “Analog1” = A1.
    const char *header = "Time_s,Analog0,Analog1\r\n";
    rc = f_write(&csvFile, header, strlen(header), &bw);
    if (rc || bw < strlen(header)) {
      Serial.print(F("f_write(header) failed, rc="));
      Serial.println((uint32_t)rc);
      f_close(&csvFile);
      while (1);
    }

    // Close the file so header is committed
    rc = f_close(&csvFile);
    if (rc) {
      Serial.print(F("f_close(header) failed, rc="));
      Serial.println((uint32_t)rc);
      while (1);
    }

    Serial.println(F("Header written to DATA.CSV"));
  }
}

void loop() {
  static unsigned long lastMillis = 0;
  unsigned long now = millis();

  // Every 1000 ms, append a new line
  if (now - lastMillis < 1000) return;
  lastMillis = now;

  // 1) Open the file in “open existing, write-only” mode.
  //    If it doesn’t exist, print an error (since the header should already be there).
  FIL    csvFile;
  FRESULT rc;
  UINT   bw;

  rc = f_open(&csvFile, filename, FA_OPEN_EXISTING | FA_WRITE);
  if (rc == FR_NO_FILE) {
    Serial.println(F("ERROR: DATA.CSV does not exist (did setup() create it?)"));
    return;
  }
  if (rc) {
    Serial.print(F("ERROR: f_open(append) failed, rc="));
    Serial.println((uint32_t)rc);
    return;
  }

  // 2) Seek to end-of-file before writing, using f_lseek() + f_size()
  DWORD fileSize = f_size(&csvFile);    // current length of file
  rc = f_lseek(&csvFile, fileSize);     // move file pointer to exactly fileSize
  if (rc) {
    Serial.print(F("ERROR: f_lseek to end failed, rc="));
    Serial.println((uint32_t)rc);
    f_close(&csvFile);
    return;
  }

  // 3) Build one CSV line: "<seconds>,<A0>,<A1>\r\n"
  char         lineBuf[64];
  unsigned long sec  = now / 1000;       // seconds since Arduino start
  int           valA0 = analogRead(A0);
  int           valA1 = analogRead(A1);
  int           len   = snprintf(lineBuf, sizeof(lineBuf),
                                 "%lu,%d,%d\r\n", sec, valA0, valA1);

  rc = f_write(&csvFile, lineBuf, len, &bw);
  if (rc || bw < (UINT)len) {
    Serial.print(F("ERROR: f_write failed, rc="));
    Serial.println((uint32_t)rc);
    f_close(&csvFile);
    return;
  }

  // 4) Close so that everything actually gets flushed to the stick
  rc = f_close(&csvFile);
  if (rc) {
    Serial.print(F("ERROR: f_close failed, rc="));
    Serial.println((uint32_t)rc);
    return;
  }

  // 5) Debug print to Serial
  Serial.print(F("Appended: "));
  Serial.print(lineBuf);
}

