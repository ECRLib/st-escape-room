#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

// Internal LED is on Digital 12
#define LED 12
// We are using pins 2 and 3 to interface with the DFPlayer
#define rxPin 2
#define txPin 3
// We are using pin 4 for the rotary dial pulse
#define dialPulse 4
// We are using pin 5 for the rotary dial active
#define dialActive 5
// We are using pin 6 for the hook switch, when off hook it will be low
#define hookSwitch 6

// Define MP3 track names. MP3 names on SD card must be numeric
#define dialTone 0001
#define offHookTone 0002
#define notAvailable 0003
#define voiceMail 0004

// Debounce the pulses - should be longer than 5ms
#define debounceDelay 5
// Wait time before Off Hook tone plays
#define offHookDelay 10000

// The number needed to play the voice mail message
int targetDigits[] = {6,1,8,6,2,5,8,3,1,3}; // note zero would be 10 for pulse dialing
int dialedDigits[] = {0,0,0,0,0,0,0,0,0,0}; // start with nothing, 0 is invalid

SoftwareSerial DFPlayerSerial(rxPin, txPin); // RX, TX
DFRobotDFPlayerMini DFPlayer;

void(* resetFunc) (void) = 0; // Arduino reset function if DFPlayer cannot init

bool isOffHook(); // Check if hook switch is low
bool isDialing(); // Check if dial active is low
int pulses(); // Count pulses and return the count
void resetDialer(); // Reset the dialed digits to zeros

void setup() {
  // Initialize serial connection to PC if present
  Serial.begin(115200);

  // Initialize DFPlayer serial connection for control
  DFPlayerSerial.begin(9600);

  // Set pin modes to input pull up since all will go low when active
  int inPins[] = {hookSwitch, dialPulse, dialActive};
  for (int i = 0; i < 3; i++) {
    pinMode(inPins[i], INPUT_PULLUP);
  }
  Serial.println("Input pins are set up.");
  
  // Set LED as output
  pinMode(LED, OUTPUT);
  Serial.println("Internal LED configured.");

  Serial.println("Welcome to Stranger Things Phone!");
  Serial.println("Initializing DFPlayer ... (May take 3-5 seconds)");

  if (!DFPlayer.begin(DFPlayerSerial)) {  // Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    // Wait 5 seconds, then reset Arduino to try again
    delay(5000);
    resetFunc();
  }
  Serial.println(F("DFPlayer Mini online."));
  DFPlayer.volume(30);  // Set volume value. From 0 to 30, we want MAX

}

void loop() {
  // Wait until the receiver is taken off the hook
  while (!isOffHook()) {
    delay(50); // Do nothing for 50ms
  }

  // We can assume the receiver is off the hook at this point
  Serial.println("Receiver off hook! Play dial tone.");
  // so we should play the dial tone.
  DFPlayer.playMp3Folder(dialTone);
  // Record the current time to time off hook
  unsigned long hookTimer = millis();
  // Wait until the user starts dialing
  do {
    // If the user waits too long, play the off hook tone until they hang up
    if (millis() - hookTimer > offHookDelay) {
      Serial.println("Waited too long, play off hook tone and wait until hang up.");
      DFPlayer.playMp3Folder(offHookTone);
      while (isOffHook()) {
        delay(50); // Do nothing for 50ms
      }
    }
  } while (!isDialing() && isOffHook());

  // We can assume the user either hung up or has started dialing
  // so we should count the pulses if the receiver is off the hook
  if (isOffHook()) { // do nothing if the receiver is on the hook
    Serial.println("User is dialing!");
    // Stop playing the dial tone when the user is dialing
    DFPlayer.stop();
    for (int i = 0; i < 10 && isOffHook(); i++) { // Stop counting digits after 10 or when hung up
      int currentDigit = pulses();
      if ( currentDigit == 1 && i == 0) { // if the user dials a 1 for long distance, ignore it
        Serial.println("User dialed a 1 for long distance. Ignore and reset.");
        i = -1; // will increment to zero when the loop repeats
      } else {
        dialedDigits[i] = currentDigit;
        Serial.print("User dialed ");
        Serial.println(currentDigit);
      }
      // Record the current time to time off hook
      unsigned long hookTimer = millis();
      do {
        // If the user waits too long, play the off hook tone until they hang up
        if (millis() - hookTimer > offHookDelay) {
          DFPlayer.playMp3Folder(offHookTone);
          while (isOffHook()) {
            delay(50); // Do nothing for 50ms
          }
        }
      } while (!isDialing() && isOffHook() && i != 9);
    } // User either hung up or has dialed all 10 digits. Time to compare!
    if (isOffHook()) {
      bool isCorrect = true;
      for (int i = 0; i < 10 && isCorrect; i++) {
        isCorrect = dialedDigits[i] == targetDigits[i];
      }
      if (isCorrect) {
        Serial.println("User dialed the right number, play voice mail!");
        DFPlayer.playMp3Folder(voiceMail);
        DFPlayer.enableLoop();
      } else {
        Serial.println("User dialed the wrong number, play not available.");
        DFPlayer.playMp3Folder(notAvailable);
        DFPlayer.enableLoop();
      }
      // Wait until either one has finished playing or they hang up
      while (DFPlayer.readType() != DFPlayerPlayFinished && isOffHook()) {
        delay(50); // Do nothing for 50 ms
      }
      // Record the current time to time off hook
      unsigned long hookTimer = millis();
      // Wait until the user hangs up
      do {
        // If the user waits too long, play the off hook tone until they hang up
        if (millis() - hookTimer > 500000) {
          Serial.println("Waited too long, play off hook tone and wait until hang up.");
          DFPlayer.playMp3Folder(offHookTone);
          while (isOffHook()) {
            delay(50); // Do nothing for 50ms
          }
        }
      } while (isOffHook());
    }
  }
  // Receiver should be on the hook at this point, stop all playback and reset
  DFPlayer.stop();
  resetDialer();
  Serial.println("User hung up. Reset and go again!");
}

void resetDialer() {
  for (int i = 0; i < 10; i++) {
    dialedDigits[i] = 0;
  }
}

int pulses() {
  int digit = 0;
  int lastRead = LOW;
  unsigned long lastChangeTime = millis();
  // While dialing is active count pulses
  digitalWrite(LED, HIGH);
  while (isDialing()) {
    int currentRead = digitalRead(dialPulse);
    if (lastRead != currentRead && ((millis() - lastChangeTime) > debounceDelay)) {
      digitalWrite(LED, !lastRead);
      // Only increment at the leading edge of the pulse
      if (currentRead == HIGH) {
        digit++;
      }
      lastChangeTime = millis();
      lastRead = currentRead;
    }
  }
  digitalWrite(LED, LOW);
  // Dialing has finished, return counted pulses as the digit
  // Note that zero is 10 pulses
  return digit;
}

bool isOffHook() {
  return !digitalRead(hookSwitch);
}

bool isDialing() {
  return !digitalRead(dialActive);
}
