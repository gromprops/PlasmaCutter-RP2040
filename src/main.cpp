#include <Arduino.h>
#include <Servo.h>
#include <FastLED.h>
#include <DFPlayerMini_Fast.h>

unsigned long currMillis = 0;

/*
  INPUT
*/

// Definitions
const uint8_t trigger1Pin = 0;              // Inner trigger
const uint8_t trigger2Pin = 1;              // Outer trigger
const uint8_t selectorPin = 2;              // Rotation selector

const uint8_t calModePin = 5;               // Switches servo position tuning mode
const uint8_t calPotPin = A2;               // Calibration potentiometer pin

// Setup Function

void initializeInputs() {
  Serial.println(F("Initializing Inputs"));

  pinMode(trigger1Pin, INPUT_PULLUP);
  pinMode(trigger2Pin, INPUT_PULLUP);
  pinMode(selectorPin, INPUT_PULLUP);

  pinMode(calModePin, INPUT_PULLUP);
}

/*
  SOUND
*/

// Definitions
DFPlayerMini_Fast player;

const uint8_t defaultVol = 30;

// Track assignments
const uint8_t startupSFX = 1;
const uint8_t finOpenSFX = 2;
const uint8_t finCloseSFX = 3;
const uint8_t rotateSFX = 4;
const uint8_t errorSFX = 5;
const uint8_t emptySFX = 6;
const uint8_t fire1SFX = 7;
const uint8_t fire2SFX = 8;

// Functions
void playSFX(int track) {
  player.play(track);
}

// Setup Function
void initializeDFPlayer() {
  Serial1.begin(9600);
  Serial.println(F("Initializing DFPlayer... (May take 3-5 seconds)"));
  player.begin(Serial1, true);
  Serial.println(F("DFPlayer Mini initialized."));

  player.volume(defaultVol);
}

/*
  SERVOS

  This prop uses 2 servo motors, 1 MG90S and 1 DS3218MG, to extend/retract the outer fins and to rotate the whole head respectively.
  The MG90S is driven by a digital PWM pin and powered off of a L7806ABV 6V regulator.
  The DS3218MG is driven by a digital PWM pin and powered from the 7.8V output from the buck converter.
*/

// Fin Servo
Servo finservo;                         // MG90S servo used to actuate retracting fins
const uint8_t finServoPin = 14;
uint8_t finExtendedPos = 160;
const uint8_t finFiringPos = finExtendedPos - 80;        // TODO Tune firing position
uint8_t finRetractedPos = 20;

const uint16_t finInterval = 250;               // Approx delay @ 5V (325)
const uint16_t finFiringInterval = 125;         // TODO Tune interval

uint8_t finPos = finRetractedPos;
unsigned long lastFinMillis = 0;
bool finMoving = false;
bool finExtended = false;

bool readyFinRetract = false;
bool readyFinExtend = false;

// Fin Servo Functions
void finExtend() {
  if (!finMoving && finPos != finExtendedPos) {
    playSFX(finOpenSFX);
    finservo.write(finExtendedPos);
    finPos = finExtendedPos;
    finMoving = true;
    lastFinMillis = currMillis;
    }
  if (finMoving) {
    if (currMillis >= lastFinMillis + finInterval) {
      finMoving = false;
      finExtended = true;
      Serial.println(F("finExtend complete"));
    }
  }
}

void finRetract() {
  if (!finMoving && finPos != finRetractedPos) {
    playSFX(finCloseSFX);
    finservo.write(finRetractedPos);
    finPos = finRetractedPos;
    finMoving = true;
    lastFinMillis = currMillis;
    }
  if (finMoving) {
    if (currMillis >= lastFinMillis + finInterval) {
      finMoving = false;
      finExtended = false;
      Serial.println(F("finHeadRetract complete"));
    }
  }
}

// Head Servo
Servo headservo;                          // DS3218MG servo used to rotate head
const uint8_t headServoPin = 15;
const uint16_t headServoMinPulse = 500;
const uint16_t headServoMaxPulse = 2500;
uint8_t headServoMinPos = 53;
uint8_t headServoMaxPos = 107;

const uint16_t headInterval = 350;                // TODO Verify head delay @ working voltage (~7V)

bool initHeadRotate = false;
uint8_t headPos = headServoMinPos;
unsigned long lastHeadMillis = 0;
bool headMoving = false;

void rotateHead() {
  if (!finExtended) {
    finExtend();                              // Extends fins to allow for head rotation
  }

  // // playSFX(rotateSFX);
  if (finExtended && (finPos == finExtendedPos)) {
    if (!headMoving) {
      switch(headPos) {
        case headServoMaxPos:
          headservo.write(headServoMinPos);
          lastHeadMillis = currMillis;
          headPos = headServoMinPos;
          headMoving = true;
          break;
        case headServoMinPos:
          headservo.write(headServoMaxPos);
          lastHeadMillis = currMillis;
          headPos = headServoMaxPos;
          headMoving = true;
          break;
      }
    }
    if (headMoving) {
      if (currMillis >= lastHeadMillis + headInterval) {
        headMoving = false;
        readyFinRetract = true;
      }
    }
  }

  if (readyFinRetract) {
    finRetract();
    delay(500);
    if (!finExtended) {
      readyFinRetract = false;
      initHeadRotate = false;
    }
  }
}

// Servo Calibration
uint8_t newHeadHomePos = 0;
uint8_t newHeadRotatedPos = 0;
uint8_t newFinExtendedPos = 0;
uint8_t newFinRetractedPos = 0;

/*
Mode setting:
The MODE buttom push will cycle through the following modes:
0: Normal operation
1: Head home position calibration
2: Head rotated position calibration
3: Extended fin position calibration
4: Retracted fin postion calibration
*/
uint8_t currMode = 0;

void savePosCalibration() {
  headServoMinPos = newHeadHomePos;
  headServoMaxPos = newHeadRotatedPos;
  finExtendedPos = newFinExtendedPos;
  finRetractedPos = newFinRetractedPos;
}

void syncHeadHomePos() {
  uint8_t headHomePotVal = analogRead(calPotPin);
  newHeadHomePos = floor(270 * (headHomePotVal/4095));
  digitalWrite(headServoPin, newHeadHomePos);
}

void syncHeadRotatedPos() {
  uint8_t headRotatedPotVal = analogRead(calPotPin);
  newHeadRotatedPos = floor(270 * (headRotatedPotVal/4095));
  digitalWrite(headServoPin, newHeadRotatedPos);
}

void syncFinExtendedPos() {
  uint8_t finExtendedPotVal = analogRead(calPotPin);
  newFinExtendedPos = floor(180 * (finExtendedPotVal/4095));
  digitalWrite(finServoPin, newFinExtendedPos);
}

void syncFinRetractedPos() {
  uint8_t finRetractedPotVal = analogRead(calPotPin);
  newFinRetractedPos = floor(180 * (finRetractedPotVal/4095));
  digitalWrite(finServoPin, newFinRetractedPos);
}

void setMode() {
  currMode = currMode + 1;
  if (currMode > 4) currMode = 0;

  // TODO: implement mode indicator LED
  switch (currMode) {
    case 0:
      savePosCalibration();
      break;
    case 1:
      syncHeadHomePos();
      break;
    case 2:
      syncHeadRotatedPos();
      break;
    case 3:
      syncFinExtendedPos();
      break;
    case 4:
      syncFinRetractedPos();
      break;
  }
}

// Setup Function
void initializeServos() {
  Serial.println(F("Initializing servos"));

  finservo.attach(finServoPin);
  headservo.attach(headServoPin, headServoMinPulse, headServoMaxPulse);

  finservo.write(finExtendedPos);           // Sets fins and head to home positions
  delay(finInterval);
  headservo.write(headServoMinPos);
  delay(headInterval);
  finservo.write(finRetractedPos);
  delay(finInterval);
}


/*
  LIGHTS
*/
// Declarations
const uint8_t laserTransistorPin = 8;

const uint8_t ledDataPin = 7;
const uint8_t ledNum = 3;

unsigned long ledRefreshInterval = 10000;

CRGB nozzleLeds[ledNum];

// Variables
bool ledStatus = false;
unsigned long lastLEDRefreshMillis = 0;

// Functions
void lasersOn() {
  digitalWrite(laserTransistorPin, HIGH);
}

void lasersOff() {
  digitalWrite(laserTransistorPin, LOW);
}

void ledsOn(CRGB newColor, bool isChange = false) {
  if (!ledStatus || isChange) {
    nozzleLeds[0] = newColor;
    FastLED.show();
    ledStatus = true;
  }
}

void ledsOff() {
  if (ledStatus) {
    nozzleLeds[0] = CRGB::Black;
    FastLED.show();
    ledStatus = false;
  }
}

// Setup Function
void initializeLights() {
  Serial.println(F("Initializing lasers and LEDs"));
  pinMode(laserTransistorPin, OUTPUT);

  FastLED.addLeds<NEOPIXEL, ledDataPin>(nozzleLeds, ledNum);
}

/*
  ACTIONS
*/

// State Definitions
bool triggerPrimed = false;
bool firing = false;

// Functions
void startup() {
  delay(2000);
  playSFX(startupSFX);
  delay(1500);

  playSFX(finOpenSFX);
  finservo.write(finExtendedPos);
  delay(finInterval + 500);
  playSFX(finCloseSFX);
  finservo.write(finRetractedPos);
  delay(finInterval + 500);

  playSFX(finOpenSFX);
  finservo.write(finExtendedPos);
  delay(finInterval + 500);

  playSFX(rotateSFX);
  headservo.write(headServoMinPos);
  delay(headInterval + 500);

  playSFX(finCloseSFX);
  finservo.write(finRetractedPos);
  delay(finInterval + 500);

  playSFX(finOpenSFX);
  finservo.write(finExtendedPos);
  delay(finInterval + 500);

  playSFX(rotateSFX);
  headservo.write(headServoMaxPos);
  delay(headInterval + 500);

  playSFX(finCloseSFX);
  finservo.write(finRetractedPos);
  delay(finInterval + 500);

  playSFX(finOpenSFX);
  finservo.write(finExtendedPos);
  delay(finInterval + 500);

  playSFX(rotateSFX);
  headservo.write(headServoMinPos);
  delay(headInterval + 500);

  playSFX(finCloseSFX);
  finservo.write(finRetractedPos);
  delay(finInterval + 500);
}

void primeTrigger() {
  finExtend();
  ledsOn(CRGB::Cyan);
  lasersOn();
  triggerPrimed = true;                   // Enables main trigger action
}

void unprimeTrigger() {
  lasersOff();
  ledsOff();
  finRetract();
  triggerPrimed = false;                  // Locks main trigger action
}



bool finFireRetractReady = false;
bool finFireExtendReady = false;

void fireTrigger() {                     // Initiates firing action, requires trigger priming
  if (!finFireRetractReady) {
    if (!finMoving && finPos != finFiringPos) {
      finservo.write(finFiringPos);
      finPos = finFiringPos;
      finMoving = true;
      lastFinMillis = currMillis;
    }
    if (finMoving) {
      if (currMillis >= lastFinMillis + finFiringInterval) {
        finMoving = false;
        finFireRetractReady = true;
      }
    }
  }

  if (finFireRetractReady && (finPos == finFiringPos)) {
    playSFX(fire1SFX);
    ledsOff();
    ledsOn(CRGB::Orange, true);
    lasersOff();
    finFireExtendReady = true;
  }

  if (finFireExtendReady) {
    if (!finMoving && finPos != finExtendedPos) {
      finservo.write(finExtendedPos);
      finPos = finExtendedPos;
      finMoving = true;
      lastFinMillis = currMillis;
    }
    if (finMoving) {
      if (currMillis >= lastFinMillis + finFiringInterval) {
        finMoving = false;
        finFireRetractReady = true;
      }
    }
    if (finFireRetractReady) {
      delay(500);               // TODO implement LED fadeout
      ledsOff();
      delay(200);               // TODO clean up firing rearm implementation
      finFireRetractReady = false;
      finFireExtendReady = false;
      firing = false;
    }
  }
}

/*
  MAIN
*/
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  initializeInputs();
  initializeDFPlayer();
  initializeServos();
  initializeLights();

  startup();
}

void loop() {
  currMillis = millis();

  if (firing) {
    digitalWrite(LED_BUILTIN, HIGH);
  }
  if (!firing) {
    digitalWrite(LED_BUILTIN, LOW);
  }

  byte trigger1State = digitalRead(trigger1Pin);
  byte trigger2State = digitalRead(trigger2Pin);
  byte selectorState = digitalRead(selectorPin);
  if (trigger1State == LOW && !firing) {
    primeTrigger();
  }
  if (trigger1State == HIGH && !firing) {
    if (!initHeadRotate) {
      unprimeTrigger();
    }
  }

  if (trigger2State == LOW) {
    if (triggerPrimed) {
      firing = true;
    } else {
      playSFX(errorSFX);
    }
  }

  if (selectorState == LOW && !firing) {
    initHeadRotate = true;
  }

  if (initHeadRotate) {
    rotateHead();
  }
  if (firing) {
    fireTrigger();
  }
}