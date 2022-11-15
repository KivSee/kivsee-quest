#include <SPI.h>
#include <MFRC522.h>
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

// logic
#define MY_LEVEL 1

// RFID
#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key; 
// Init array that will store new NUID 
byte nuidPICC[4];
byte blockAddr      = 4;
byte buffer[18];
byte size = sizeof(buffer);
byte trailerBlock   = 7;

// sound
SoftwareSerial mySoftwareSerial(2, 3); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

bool write_and_verify(byte blockAddr, byte dataBlock[], byte buffer[], byte size) {
    MFRC522::StatusCode status;

    // Write data to the block
    Serial.print(F("Writing data into block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    // dump_byte_array(dataBlock, 16); Serial.println();
    status = (MFRC522::StatusCode) rfid.MIFARE_Write(blockAddr, dataBlock, 16);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Write() failed: "));
        Serial.println(rfid.GetStatusCodeName(status));
        return false;
    }
    Serial.println();

    // Read data from the block (again, should now be what we have written)
    Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    status = (MFRC522::StatusCode) rfid.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Read() failed: "));
        Serial.println(rfid.GetStatusCodeName(status));
        return false;
    }
    // Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
    // dump_byte_array(buffer, 16); Serial.println();

    // Check that data in block is what we have written
    // by counting the number of bytes that are equal
    Serial.println(F("Checking result..."));
    byte count = 0;
    for (byte i = 0; i < 16; i++) {
        // Compare buffer (= what we've read) with dataBlock (= what we've written)
        if (buffer[i] == dataBlock[i])
            count++;
    }
    Serial.print(F("Number of bytes that match = ")); Serial.println(count);
    if (count == 16) {
        Serial.println(F("Success :-)"));
        return true;
    } else {
        Serial.println(F("Failure, no match :-("));
        Serial.println(F("  perhaps the write didn't work properly..."));
        Serial.println();
        return false;
    }
}

void setup() { 

  Serial.begin(115200);

  // RFID
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // Sound
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
  
  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true){
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }
  Serial.println(F("DFPlayer Mini online."));
  
  myDFPlayer.volume(10);  //Set volume value. From 0 to 30
}
 
void loop() {

  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  // authenticate!
  MFRC522::StatusCode auth_status = (MFRC522::StatusCode) rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(rfid.uid));
  if (auth_status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(rfid.GetStatusCodeName(auth_status));
    return;
  }

  // MIFARE_Read get the size and changes it.
  // we have to restore it before every call
  size = sizeof(buffer);
  MFRC522::StatusCode read_status = (MFRC522::StatusCode) rfid.MIFARE_Read(blockAddr, buffer, &size);
  if (read_status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Read() failed: "));
      Serial.println(rfid.GetStatusCodeName(read_status));
      return;
  }

  byte level = *(buffer + 1);
  if(level == 0xff) {
    level = 0;
  }
  Serial.print("Current level: "); Serial.println(level);

  if(level == MY_LEVEL - 1) {
    // happy path
    
    buffer[1] = MY_LEVEL;
    byte dataBlock[] = {
      0x04, MY_LEVEL, 0x00, 0x00, //  byte 1 for color encoding
      0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x02  // byte 15 for event track bit[0] = burnerot2018, bit[1] = contra2019
    };

    bool write_success = write_and_verify(blockAddr, dataBlock, buffer, size);
    if(!write_success) {
      // play - cound not write. try again
      myDFPlayer.play(4);
      Serial.println(F("write failed"));
      return;
    } else {
      // play audio good
      myDFPlayer.play(1);  //Play the first mp3
      Serial.println("playing sound - good");
    }
  } else {

    if(level < (MY_LEVEL - 1)) {
      // play sound - go back
      myDFPlayer.play(2);
      Serial.println("playing sound - bad, you missed a station");
    } else {
      myDFPlayer.play(3);
      Serial.println("playing sound - bad, already visited");
      // play you were here before - no need to come back
    }
  }

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

