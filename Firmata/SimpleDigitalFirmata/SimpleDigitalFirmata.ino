/*
 * Firmata is a generic protocol for communicating with microcontrollers
 * from software on a host computer. It is intended to work with
 * any host computer software package.
 *
 * To download a host software package, please clink on the following link
 * to open the download page in your default browser.
 *
 * http://firmata.org/wiki/Download
 */

/* This sketch is compatible with vMix
 *  
 * Author: Ford Polia
 */
//#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <SPI.h>
#include <EthernetV2_0.h>
#include <BlynkSimpleEthernetV2_0.h>
#include <Firmata.h>
#include <BMDSDIControl.h>

#define W5200_CS  10
#define SDCARD_CS 4

char auth[] = "e1c914f18a6041bc816598a7623a3da8";

#define PIN_LIVE_CAMERA V0 //virtual pin for Blynk
#define PHYS_PIN_CAM_1_PREV 2
#define PHYS_PIN_CAM_1_LIVE 3
#define PHYS_PIN_CAM_2_PREV 5
#define PHYS_PIN_CAM_2_LIVE 6
#define PHYS_PIN_CAM_3_PREV 7
#define PHYS_PIN_CAM_3_LIVE 8
#define PHYS_PIN_CAM_4_PREV 9
#define PHYS_PIN_CAM_4_LIVE 12

WidgetLED previewIndicator(V5);
WidgetLED liveIndicator(V6);

int currentCamera;
int liveCamera;
int previewCamera;
bool autoIris[4];
bool autoFocus[4];
float audioLevels[2];

byte previousPIN[TOTAL_PORTS];  // PIN means PORT for input
byte previousPORT[TOTAL_PORTS];

// Blackmagic Design SDI control shield globals
const int                 shieldAddress = 0x6E;
BMD_SDITallyControl_I2C   sdiTallyControl(shieldAddress);
BMD_SDICameraControl_I2C  sdiCameraControl(shieldAddress);

void outputPort(byte portNumber, byte portValue)
{
  // only send the data when it changes, otherwise you get too many messages!
  if (previousPIN[portNumber] != portValue) {
    Firmata.sendDigitalPort(portNumber, portValue);
    previousPIN[portNumber] = portValue;
  }
}

void setPinModeCallback(byte pin, int mode) {
  if (IS_PIN_DIGITAL(pin)) {
    pinMode(PIN_TO_DIGITAL(pin), mode);
  }
}

void digitalWriteCallback(byte port, int value)
{
  byte i;
  byte currentPinValue, previousPinValue;

  if (port < TOTAL_PORTS && value != previousPORT[port]) {
    for (i = 0; i < 8; i++) {
      currentPinValue = (byte) value & (1 << i);
      previousPinValue = previousPORT[port] & (1 << i);
      if (currentPinValue != previousPinValue) {
        digitalWrite(i + (port * 8), currentPinValue);
      }
    }
    previousPORT[port] = value;
  }
}

void setup()
{
  //Serial.begin(57600);
  Firmata.setFirmwareVersion(FIRMATA_FIRMWARE_MAJOR_VERSION, FIRMATA_FIRMWARE_MINOR_VERSION);
  Firmata.attach(DIGITAL_MESSAGE, digitalWriteCallback);
  Firmata.attach(SET_PIN_MODE, setPinModeCallback);
  Firmata.begin(57600);

  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH); // Deselect the SD card

  Blynk.begin(auth);

  // Set up the BMD SDI control library
  sdiTallyControl.begin();
  sdiCameraControl.begin();

  // Enable both tally and control overrides
  sdiTallyControl.setOverride(true);
  sdiCameraControl.setOverride(true);
}

// This function tells Arduino what to do if there is a Widget
// which is requesting data for Virtual Pin (0)
BLYNK_READ(PIN_LIVE_CAMERA)
{
  // This command writes Arduino's uptime in seconds to Virtual Pin (5)
  Blynk.virtualWrite(PIN_LIVE_CAMERA, liveCamera);
  if (currentCamera == liveCamera){
    liveIndicator.on();
    previewIndicator.off();
  } else if (currentCamera == previewCamera){
    liveIndicator.off();
    previewIndicator.on();
  } else {
    liveIndicator.off();
    previewIndicator.off();
  }
}

BLYNK_WRITE(V1) //user selected camera 1
{
  if (param.asInt() == 1){
    currentCamera = 1;
    Blynk.virtualWrite(V2, 0);
    Blynk.virtualWrite(V3, 0);
    Blynk.virtualWrite(V4, 0);
  }
}

BLYNK_WRITE(V2) //User selected camera 2
{
  if (param.asInt() == 1){
    currentCamera = 2;
    Blynk.virtualWrite(V1, 0);
    Blynk.virtualWrite(V3, 0);
    Blynk.virtualWrite(V4, 0);
  }
}

BLYNK_WRITE(V3) //User selected camera 3
{
  if (param.asInt() == 1){
    currentCamera = 3;
    Blynk.virtualWrite(V1, 0);
    Blynk.virtualWrite(V2, 0);
    Blynk.virtualWrite(V4, 0);
  }
}

BLYNK_WRITE(V4) //User selected camera 4
{
  if (param.asInt() == 1){
    currentCamera = 4;
    Blynk.virtualWrite(V1, 0);
    Blynk.virtualWrite(V2, 0);
    Blynk.virtualWrite(V3, 0);
  }
}

BLYNK_WRITE(V7) //User adjusted focus
{
  sdiCameraControl.writeCommandFixed16(
      currentCamera,          // Destination Camera
      0,                      // Category:       Lens
      0,                      // Param:          Focus
      0,                      // Operation:      Assign Value
      (param.asFloat()/100.0) // Value
    );
}

BLYNK_WRITE(V8) {
  switch (param.asInt())
  {
    case 1: { // User selected autofocus off
      autoFocus[currentCamera] = false;
      break;
    }
    case 2: { // User selected autofocus on
      autoFocus[currentCamera] = true;
      break;
    }
    default: {
      autoFocus[currentCamera] = false;
    }
  }
}

BLYNK_WRITE(V9) //User adjusted aperture
{
  sdiCameraControl.writeCommandFixed16(
      currentCamera,          // Destination Camera
      0,                      // Category:       Lens
      3,                      // Param:          Aperture (Normalised)
      0,                      // Operation:      Assign Value
      (param.asFloat()/100.0) // Value
    );
}

BLYNK_WRITE(V10) {
  switch (param.asInt())
  {
    case 1: { // User selected auto iris off
      autoIris[currentCamera-1] = false;
      break;
    }
    case 2: { // User selected auto iris on
      autoIris[currentCamera-1] = true;
      break;
    }
    default: {
      autoIris[currentCamera-1] = false;
    }
  }
}

BLYNK_WRITE(V11) //User adjusted zoom
{
  sdiCameraControl.writeCommandFixed16(
      currentCamera,          // Destination Camera
      0,                      // Category:       Lens
      8,                      // Param:          Zoom (Normalised)
      0,                      // Operation:      Assign Value
      (param.asFloat()/100.0) // Value
    );
}

BLYNK_WRITE(V12) //User adjusted exposure
{
  sdiCameraControl.writeCommandFixed16(
      currentCamera,          // Destination Camera
      1,                      // Category:       Video
      5,                      // Param:          Exposure (Normalised)
      0,                      // Operation:      Assign Value
      (param.asFloat()/1000.0)// Value
    );
}

BLYNK_WRITE(V13) //User adjusted audio ch 0
{
  audioLevels[0] = param.asFloat()/100.0;
  sdiCameraControl.writeCommandFixed16(
      currentCamera,          // Destination Camera
      2,                      // Category:       Audio
      8,                      // Param:          Input Levels
      0,                      // Operation:      Assign Value
      audioLevels             // Values
    );
}

BLYNK_WRITE(V14) //User adjusted audio ch 1
{
  audioLevels[1] = param.asFloat()/100.0;
  sdiCameraControl.writeCommandFixed16(
      currentCamera,          // Destination Camera
      2,                      // Category:       Audio
      8,                      // Param:          Input Levels
      0,                      // Operation:      Assign Value
      audioLevels             // Values
    );
}

void loop()
{
  byte i;

  for (i = 0; i < TOTAL_PORTS; i++) {
    outputPort(i, readPort(i, 0xff));
  }

  Blynk.run();

  while (Firmata.available()) {
    Blynk.run();
    Firmata.processInput();

    if (digitalRead(PHYS_PIN_CAM_1_LIVE) == HIGH){
      sdiTallyControl.setCameraTally(    
        1,                                                     // Camera Number
        true,                                                  // Program Tally
        false                                                  // Preview Tally
      );
      liveCamera = 1;
    } else if (digitalRead(PHYS_PIN_CAM_1_PREV) == HIGH){
      sdiTallyControl.setCameraTally(    
        1, 
        false, 
        true   
      );
      previewCamera = 1;
      if (autoFocus[0] == true){
        sdiCameraControl.writeCommandVoid(
          1,      // Destination:    Camera 1
          0,      // Category:       Lens
          1       // Param:          Auto Focus
        );
      }
      if (autoIris[0] == true){
        sdiCameraControl.writeCommandVoid(
          1,      // Destination:    Camera 1
          0,      // Category:       Lens
          5       // Param:          Auto Iris
        );
      }
    } else {
      sdiTallyControl.setCameraTally(    
        1, 
        false, 
        false
      );
    }
    if (digitalRead(PHYS_PIN_CAM_2_LIVE) == HIGH){
      sdiTallyControl.setCameraTally(    
        2,                                                     // Camera Number
        true,                                                  // Program Tally
        false                                                  // Preview Tally
      );
      liveCamera = 2;
    } else if (digitalRead(PHYS_PIN_CAM_2_PREV) == HIGH){
      sdiTallyControl.setCameraTally(    
        2, 
        false, 
        true   
      );
      previewCamera = 2;
      if (autoFocus[1] == true){
        sdiCameraControl.writeCommandVoid(
          2,      // Destination:    Camera 2
          0,      // Category:       Lens
          1       // Param:          Auto Focus
        );
      }
      if (autoIris[1] == true){
        sdiCameraControl.writeCommandVoid(
          2,      // Destination:    Camera 2
          0,      // Category:       Lens
          5       // Param:          Auto Iris
        );
      }
    } else {
      sdiTallyControl.setCameraTally(    
        2, 
        false, 
        false
      );
    }
    if (digitalRead(PHYS_PIN_CAM_3_LIVE) == HIGH){
      sdiTallyControl.setCameraTally(    
        3,                                                     // Camera Number
        true,                                                  // Program Tally
        false                                                  // Preview Tally
      );
      liveCamera = 3;
    } else if (digitalRead(PHYS_PIN_CAM_3_PREV) == HIGH){
      sdiTallyControl.setCameraTally(    
        3, 
        false, 
        true   
      );
      previewCamera = 3;
      if (autoFocus[2] == true){
        sdiCameraControl.writeCommandVoid(
          3,      // Destination:    Camera 3
          0,      // Category:       Lens
          1       // Param:          Auto Focus
        );
      }
      if (autoIris[2] == true){
        sdiCameraControl.writeCommandVoid(
          3,      // Destination:    Camera 3
          0,      // Category:       Lens
          5       // Param:          Auto Iris
        );
      }
    } else {
      sdiTallyControl.setCameraTally(    
        3, 
        false, 
        false
      );
    }
    if (digitalRead(PHYS_PIN_CAM_4_LIVE) == HIGH){
      sdiTallyControl.setCameraTally(    
        4,                                                     // Camera Number
        true,                                                  // Program Tally
        false                                                  // Preview Tally
      );
      liveCamera = 4;
    } else if (digitalRead(PHYS_PIN_CAM_4_PREV) == HIGH){
      sdiTallyControl.setCameraTally(    
        4, 
        false, 
        true   
      );
      previewCamera = 4;
      if (autoFocus[3] == true){
        sdiCameraControl.writeCommandVoid(
          4,      // Destination:    Camera 4
          0,      // Category:       Lens
          1       // Param:          Auto Focus
        );
      }
      if (autoIris[3] == true){
        sdiCameraControl.writeCommandVoid(
          4,      // Destination:    Camera 4
          0,      // Category:       Lens
          5       // Param:          Auto Iris
        );
      }
    } else {
      sdiTallyControl.setCameraTally(    
        4, 
        false, 
        false
      );
    }
  }
}
