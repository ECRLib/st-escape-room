// Start with the LED pins. We are using MOSFETs to ground the negative leg of the LEDs
#define LED_U 2
#define LED_S 3
#define LED_E 4
#define LED_P 6
#define LED_H 8
#define LED_O 10
#define LED_N 12

// Now the trigger remote switch pin.
#define SW_IN 5

// Now the string power toggle remote pin.
#define SW_OUT 7

// The internal LED
#define I_LED 13

// First word is USE
int firstWord[] = {LED_U, LED_S, LED_E};
// Second word is PHONE
int secondWord[] = {LED_P, LED_H, LED_O, LED_N, LED_E};

// This function will flash each of the LEDs in the array for 1 second
void flashLEDs(int ledArray[]);
// Turn off the string of LEDs
void stringToggle();

void setup() {
  // Init Serial
  Serial.begin(115200);
  // Set the pin modes for LED output, switch out, and switch in
  int outPins[] =
  {
    LED_U, LED_S, LED_E, LED_P, LED_H, LED_O, LED_N, SW_OUT, I_LED
  };

  pinMode(SW_IN, INPUT_PULLUP);

  for (int i = 0; i < 8; i++) {
    pinMode(outPins[i], OUTPUT);
  }
}

void loop() {
  // Get the current state for the remote relay
  int lastRead = digitalRead(SW_IN);
  Serial.print("Switch is on? : ");
  Serial.println(lastRead);
  // Do nothing until the remote trigger is activated
  do {
    digitalWrite(I_LED, lastRead);
    delay(50);
  } while (digitalRead(SW_IN) == lastRead);

  // Triggered! Let's play the LED sequence.
  Serial.println("Remote switch toggle detected! Playing LED sequence!");
  // First we turn off the LED string.
  stringToggle();
  // Now we play the first word sequence
  flashLEDs(firstWord, 3);
  Serial.println("USE should have been flashed.");
  // Wait a bit for the reader
  delay(1000);
  // Play the second word sequence
  flashLEDs(secondWord, 5);
  Serial.println("PHONE should have been flashed.");
  // Pause for dramatic effect
  delay(5000);
  // Turn the string back on
  stringToggle();
}

void flashLEDs(int ledArray[], int length) {
  for (int i = 0; i < length; i++) {
    Serial.print("Flashing LED on pin ");
    Serial.println(i);
    digitalWrite(ledArray[i], HIGH); // Turn the LED on
    delay(1000); // Leave it on for 1 second
    digitalWrite(ledArray[i], LOW); // Turn it off again
    delay(250); // Wait a little bit before starting with the next one
  }
}

void stringToggle() {
  digitalWrite(SW_OUT, HIGH); // Toggle the MOSFET across the switch to trigger
  delay(50); // Wait a bit
  digitalWrite(SW_OUT, LOW); // Disconnect the switch again
  delay(1000); // Pause for dramatic effect
}
