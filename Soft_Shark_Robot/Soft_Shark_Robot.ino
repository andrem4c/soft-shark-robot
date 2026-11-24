// Bioinspired Soft Shark Robot
// By: RoboDIYer

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// NTC pins
const int pinNTC1 = A8;  
const int pinNTC2 = A9;  

// Setup MOSFETS
const int pwmPin1 = 1;
const int pwmPin2 = 2;
const int pwmChannel1 = 0;
const int pwmChannel2 = 1;
const int pwmFreq = 1000;
const int pwmResolution = 8;

// Default start values
int dutyCycle = 165;            // Default PWM value (65%)
int activationTime = 1000;      // Default activation time
int restTime = 700;             // Default rest time

// Default calibration values
int calibrationActiveTime = 2000;
int calibrationRestTime = 4000;
int calibrationDutyCycle = 165;

// BLE config.
BLEServer* pServer = nullptr;
BLECharacteristic* pNotifyCharacteristic = nullptr;
BLECharacteristic* pWriteCharacteristic = nullptr;

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHAR_NOTIFY_UUID    "abcd1234-ab12-cd34-ef56-abcdef123456"
#define CHAR_WRITE_UUID     "fff1fff1-1111-2222-3333-444455556666"

bool deviceConnected = false;
String lastCommand = "";

// Robot states
enum MovementState { CALIBRATING, STOPPED, FORWARD, LEFT, RIGHT, COOLING };
MovementState currentState = STOPPED;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Disconnected");
    BLEDevice::startAdvertising();
    currentState = STOPPED;        
  }
};

// Auto cooling values
float temp1 = 0.0;              // Left SMA springs temperature
float temp2 = 0.0;              // Right SMA springs temperature

const float coolingThreshold = 60.0;
const float coolingResumeThreshold = 30.0;

int coolingRestTime = 0;
int coolingSpringsTime = 4000;

MovementState resumeState = STOPPED;

class WriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      lastCommand = String(rxValue.c_str());
      Serial.print("📩 Command received: ");
      Serial.println(lastCommand);
    }
  }
};

// Read temperature from NTC's function
float readNTCTemperature(int pin) {
  float tempSum = 0;
  for (int i = 0; i < 10; i++) {
    int adc = analogRead(pin);
    float voltage = adc * 3.3 / 4095.0;
    float resistance = (3.3 * 100000.0 / voltage) - 100000.0;
    float tempK = 1.0 / (1.0 / 298.15 + (1.0 / 3435.0) * log(resistance / 100000.0));
    tempSum += (tempK - 273.15);
  }
  return tempSum / 10;
}

// Get temperature status
String getTempStatus(float tempC) {
  if (tempC < 30.0) {
    return "✅ Normal";
  } else if (tempC >= 30.0) {
    return "🔥 Overheated";
  } 
}

// FSM variables
int movementStep = 0;
unsigned long previousMillis = 0;

// Setup function
void setup() {

  Serial.begin(115200);
  ledcSetup(pwmChannel1, pwmFreq, pwmResolution);
  ledcAttachPin(pwmPin1, pwmChannel1);
  ledcSetup(pwmChannel2, pwmFreq, pwmResolution);
  ledcAttachPin(pwmPin2, pwmChannel2);

  BLEDevice::init("Soft Robot Shark");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pNotifyCharacteristic = pService->createCharacteristic(
    CHAR_NOTIFY_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  pNotifyCharacteristic->setValue("Awaiting commands...");

  pWriteCharacteristic = pService->createCharacteristic(
    CHAR_WRITE_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pWriteCharacteristic->setCallbacks(new WriteCallbacks());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();

  Serial.println("Establishing connection…");
}

// Loop function
void loop() {

  if (deviceConnected) {

    if (lastCommand == "START") {

      currentState = CALIBRATING;
      movementStep = 0;
      previousMillis = millis()-calibrationRestTime;    

      pNotifyCharacteristic->setValue("⚙️ Calibrating, please wait...");
      pNotifyCharacteristic->notify();
      Serial.println("⚙️ Calibrating");
      lastCommand = "";

    } else if (lastCommand == "F") {

      currentState = FORWARD;
      movementStep = 0;
      previousMillis = millis()-restTime;

      pNotifyCharacteristic->setValue("⬆️ Moving forward");
      pNotifyCharacteristic->notify();
      Serial.println("⬆️ Movment: Forward");
      lastCommand = "";

    } else if (lastCommand == "L") {

      currentState = LEFT;
      movementStep = 0;
      previousMillis = millis()-restTime;

      pNotifyCharacteristic->setValue("⬅️ Turning left");
      pNotifyCharacteristic->notify();
      Serial.println("⬅️ Movment: Left");
      lastCommand = "";

    } else if (lastCommand == "R") {

      currentState = RIGHT;
      movementStep = 0;
      previousMillis = millis()-restTime;
      
      pNotifyCharacteristic->setValue("➡️ Turning right");
      pNotifyCharacteristic->notify();
      Serial.println("➡️ Movment: Right");
      lastCommand = "";

    } else if (lastCommand == "STOP") {

      if (currentState == CALIBRATING) {

      } else {
        currentState = STOPPED;
        stopMotion();
        movementStep = 0;
        previousMillis = millis();

        pNotifyCharacteristic->setValue("⛔️ Stopped");
        pNotifyCharacteristic->notify();   
        Serial.println("Stopped");
      }
      
      lastCommand = "";

    } else if (lastCommand.startsWith("D:")) {

      int newDuty = lastCommand.substring(2).toInt();
      dutyCycle = constrain(newDuty, 0, 255); 

      Serial.print("🎚️ New Duty Cycle: ");
      Serial.println(dutyCycle);
      lastCommand = "";

    } else if (lastCommand.startsWith("T:")) {

      int newTime = lastCommand.substring(2).toInt();
      activationTime = constrain(newTime, 500, 6000);
      
      Serial.print("⏱️ New activation time: ");
      Serial.println(activationTime);
      lastCommand = "";

    } else if (lastCommand.startsWith("R:")) {

      int newRest = lastCommand.substring(2).toInt();
      restTime = constrain(newRest, 0, 5000);  

      Serial.print("⏲️ New rest time: ");
      Serial.println(restTime);
      lastCommand = "";

    }

  }
  
  if (currentState == CALIBRATING) {
    heatSprings();
  } else if (currentState == FORWARD) {
    moveForward();
  } else if (currentState == LEFT) {
    moveLeft();
  } else if (currentState == RIGHT) {
    moveRight();
  } else if (currentState == COOLING) {
    cooling();
  }

// Send temperature status to XCode
  static unsigned long lastTempSent = 0;
  if (millis() - lastTempSent > 100) {

    temp1 = readNTCTemperature(pinNTC1);
    temp2 = readNTCTemperature(pinNTC2);

    if ((currentState == FORWARD || currentState == LEFT || currentState == RIGHT) && (temp1 >= coolingThreshold || temp2 >= coolingThreshold)) {
      resumeState = currentState;
      currentState = COOLING;
      movementStep = 0;
      previousMillis = millis();
      stopMotion();
      pNotifyCharacteristic->setValue("❄️ Cooling springs... please wait");
      pNotifyCharacteristic->notify();

    }

    String status1 = getTempStatus(temp1);
    String status2 = getTempStatus(temp2);

    String data = "T1:" + String(temp1, 1) + "," + status1 + ";T2:" + String(temp2, 1) + "," + status2;
    if (deviceConnected) {
      pNotifyCharacteristic->setValue(data.c_str());
      pNotifyCharacteristic->notify();
    }
    lastTempSent = millis();
  }

}

// Calibration function
void heatSprings() {

  unsigned long currentMillis = millis();

  if (movementStep == 0 && currentMillis - previousMillis >= calibrationRestTime) {
    ledcWrite(pwmChannel1, calibrationDutyCycle);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 1;
  } else if (movementStep == 1 && currentMillis - previousMillis >= calibrationActiveTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 2;
  } else if (movementStep == 2 && currentMillis - previousMillis >= calibrationRestTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, calibrationDutyCycle);

    previousMillis = currentMillis;
    movementStep = 3;
  } else if (movementStep == 3 && currentMillis - previousMillis >= calibrationActiveTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);
    pNotifyCharacteristic->setValue("✅ Calibration complete!");
    pNotifyCharacteristic->notify();

    previousMillis = currentMillis;
    movementStep = 4;
  }

}

// Move robot forward function
void moveForward() {

  unsigned long currentMillis = millis();

  if (movementStep == 0 && currentMillis - previousMillis >= restTime) {
    ledcWrite(pwmChannel1, dutyCycle);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 1;
  } else if (movementStep == 1 && currentMillis - previousMillis >= activationTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 2;
  } else if (movementStep == 2 && currentMillis - previousMillis >= restTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, dutyCycle);

    previousMillis = currentMillis;
    movementStep = 3;
  } else if (movementStep == 3 && currentMillis - previousMillis >= activationTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 0;
  }

}

// Turn robot to left function
void moveLeft() {

  unsigned long currentMillis = millis();

  if (movementStep == 0 && currentMillis - previousMillis >= restTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, dutyCycle);

    previousMillis = currentMillis;
    movementStep = 1;
  } else if (movementStep == 1 && currentMillis - previousMillis >= activationTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 2;
  } else if (movementStep == 2 && currentMillis - previousMillis >= restTime) {
    ledcWrite(pwmChannel1, dutyCycle);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 3;
  } else if (movementStep == 3 && currentMillis - previousMillis >= activationTime/2) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 4;
  } else if (movementStep == 4 && currentMillis - previousMillis >= restTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, dutyCycle);

    previousMillis = currentMillis;
    movementStep = 5;
  } else if (movementStep == 5 && currentMillis - previousMillis >= activationTime/2) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 2;
  } 

}

// Turn robot to right function
void moveRight() {

  unsigned long currentMillis = millis();

  if (movementStep == 0 && currentMillis - previousMillis >= restTime) {
    ledcWrite(pwmChannel1, dutyCycle);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 1;
  } else if (movementStep == 1 && currentMillis - previousMillis >= activationTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 2;
  } else if (movementStep == 2 && currentMillis - previousMillis >= restTime) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, dutyCycle);

    previousMillis = currentMillis;
    movementStep = 3;
  } else if (movementStep == 3 && currentMillis - previousMillis >= activationTime/2) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 4;
  } else if (movementStep == 4 && currentMillis - previousMillis >= restTime) {
    ledcWrite(pwmChannel1, dutyCycle);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 5;
  } else if (movementStep == 5 && currentMillis - previousMillis >= activationTime/2) {
    ledcWrite(pwmChannel1, 0);
    ledcWrite(pwmChannel2, 0);

    previousMillis = currentMillis;
    movementStep = 2;
  } 

}

// Stop robot function
void stopMotion() {

  ledcWrite(pwmChannel1, 0);
  ledcWrite(pwmChannel2, 0);
  
}

// Auto cooling springs function
void cooling() {

  unsigned long currentMillis = millis();

  if (movementStep == 0 && currentMillis - previousMillis >= coolingRestTime) {
    stopMotion();

    previousMillis = currentMillis;
    movementStep = 1;
  } else if (movementStep == 1 && currentMillis - previousMillis >= coolingSpringsTime) {

    if (temp1 <= coolingResumeThreshold && temp2 <= coolingResumeThreshold) {

      currentState = resumeState;
      movementStep = 0;
      previousMillis = millis()-restTime;

      pNotifyCharacteristic->setValue("✅ Cooling complete. Resuming movement...");
      pNotifyCharacteristic->notify();
      delay(1500);

      if (currentState == FORWARD) {
        pNotifyCharacteristic->setValue("⬆️ Moving forward");
      } else if (currentState == LEFT) {
        pNotifyCharacteristic->setValue("⬅️ Turning left");
      } else if (currentState == RIGHT) {
        pNotifyCharacteristic->setValue("➡️ Turning right");
      }
      pNotifyCharacteristic->notify();

    } else {

      // Still hot, continue cooling
      previousMillis = currentMillis;
      movementStep = 0;

    }

  }
  
}
