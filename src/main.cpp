#include <SPI.h>
#include <MFRC522.h>
#include "MFRC522_func.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <FastLED.h>

// logic
#define MY_LEVEL 1
#define EVENT_ID_BIT 3 // burnerot 2022 is bit 3, count starts at 0

// RFID
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN); // Instance of the class

MFRC522::MIFARE_Key key; 
// Init array that will store new NUID 
byte nuidPICC[4];
byte blockAddr      = 4;
byte buffer[18];
byte size = sizeof(buffer);
byte trailerBlock   = 7;

// sound
#define RX_PIN 2
#define TX_PIN 3
#define BUSY_PIN 5
#define VOLUME 25
#define DFPLAYER_BUSY LOW
SoftwareSerial mySoftwareSerial(RX_PIN, TX_PIN); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

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
  Serial.print(F("Station level is: ")); Serial.println(MY_LEVEL);

  // Sound
  pinMode(BUSY_PIN, INPUT);
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
  if (digitalRead(BUSY_PIN) != DFPLAYER_BUSY) {
    FastLED.clear();
    FastLED.show();
  };

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been read
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;

  // perform authentication to open communication
  bool auth_success = authenticate(trailerBlock, &key, mfrc522);
  if (!auth_success) {
    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
    return;
  }

  // read the tag to get coded information to buffer
  bool read_success = read_block(blockAddr, buffer, size, mfrc522);
  if (!read_success) {
    // Serial.println(F("Initial read failed, closing connection"));
    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
    return;
  }

  byte color = *(buffer + 0); // byte 0 for color encoding
  if(color == 0xff) {
    color = 0x4; // set uninitialized color to 0x4
  }
  byte level = *(buffer + 1); // byte 1 for level encoding
  if(level == 0xff) {
    level = 0;
  }
  byte eventTrack = *(buffer + 15); // byte 15 for event track encoding bit[0] = burnerot2018, bit[1] = contra2019, bit[2] = midburn2022, bit[3] = burnerot2022
  if(eventTrack == 0xff) {
    eventTrack = 0x8; // set uninitialized eventTrack to 0x8, bit[3] = burnerot2022
  }
  Serial.print(F("Current chip color: ")); Serial.println(color);
  Serial.print(F("Current chip level: ")); Serial.println(level);
  Serial.print(F("Current chip eventTrack: ")); Serial.println(eventTrack);

  if(level == MY_LEVEL - 1) {
    // happy path
    eventTrack |= (1 << EVENT_ID_BIT);
    buffer[1] = MY_LEVEL;
    byte dataBlock[] = {
      color, MY_LEVEL, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, eventTrack  // byte 15 for event track 
    };

    bool write_success = write_and_verify(blockAddr, dataBlock, buffer, size, mfrc522);
    if(!write_success) {
      // play - could not write. try again
      myDFPlayer.play(4);
      Serial.println(F("write failed"));
      return;
    } else {
      // play audio good
      myDFPlayer.play(1);  //Play the first mp3
      Serial.println(F("playing sound - good"));
    }
  } else {
    if(level < (MY_LEVEL - 1)) {
      // play sound - go back
      myDFPlayer.play(2);
      Serial.println(F("playing sound - bad, you missed a station"));
    } else {
      myDFPlayer.play(3);
      Serial.println(F("playing sound - bad, already visited"));
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
      
  // hold everything in place for some time so we don't accidentally read the same chip again
  delay(200);
}

