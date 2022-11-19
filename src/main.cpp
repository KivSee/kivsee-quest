#include <SPI.h>
#include <MFRC522.h>
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <FastLED.h>

// logic
#define MY_LEVEL 1

// RFID
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN); // Instance of the class
#include "MFRC522_func.h"

MFRC522::MIFARE_Key key; 
// Init array that will store new NUID 
byte nuidPICC[4];
byte blockAddr      = 4;
byte buffer[18];
byte size = sizeof(buffer);
byte trailerBlock   = 7;
bool read_success, write_success, auth_success;

// sound
SoftwareSerial mySoftwareSerial(2, 3); // RX, TX
DFRobotDFPlayerMini myDFPlayer;
#define VOLUME 15

// LEDs
#define DATA_PIN 7
#define NUM_LEDS 3
#define BRIGHTNESS 50

// Define the array of leds
CRGB leds[NUM_LEDS];


void setup() { 

  Serial.begin(115200);

  // RFID
  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  //mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  Serial.print("Station level is: "); Serial.println(MY_LEVEL);

  // Sound
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
  
  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin DFPlayer Mini:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    Serial.println(F("Continuing with no sound..."));
  } else {
    Serial.println(F("DFPlayer Mini online."));
    myDFPlayer.volume(VOLUME);  //Set volume value. From 0 to 30
  }

  // LEDs
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
}
 
void loop() {

  // clearing LEDs if the sound is not playing
  if (myDFPlayer.readState() != Busy) {
    FastLED.clear();
    FastLED.show();
  };

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;

  // perform authentication to open communication
  auth_success = authenticate(trailerBlock, key);
  if (!auth_success) {
    return;
  }

  // RFID reads get the size and change it.
  // we have to restore it before every call
  size = sizeof(buffer);
  // read the tag to get coded information to buffer
  read_success = read_block(blockAddr, buffer, size);
  if (!read_success) {
    // Serial.println(F("Initial read failed, closing connection"));
    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
    return;
  }

  byte level = *(buffer + 1);
  if(level == 0xff) {
    level = 0;
  }
  Serial.print("Current chip level: "); Serial.println(level);

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
      // play - could not write. try again
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
  mfrc522.PICC_HaltA();

  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();

  // visual indication for a successful operation
  fill_solid(leds, NUM_LEDS, CHSV(0, 255, 255));
  FastLED.show();
      
  // hold everything in place for some time
  delay(200);
}

