/*
   * ESP32 TANK + 4 MOTORS WITH DUAL BLUETOOTH (Classic + BLE)
   * Using 3 L298N modules with independent H-bridges
   * Bluetooth Classic: ESP32 acts as slave (SPP profile)
   * Bluetooth Low Energy: ESP32 acts as BLE UART peripheral
   *
   * Pin Configuration:
   * TANK:   ENA=12, IN1=13, IN2=14, ENB=25, IN3=26, IN4=27
   * CLAW:   IN1=16, IN2=17, IN3=18, IN4=19  (ENA/ENB hardwired to 5V)
   * REACH:  IN1=36, IN2=39, IN3=32, IN4=33  (ENA/ENB hardwired to 5V)
   *
   * Total pins: 14 (all used)
   *
   * ⚠️  IMPORTANT: Remove jumpers from CLAW/REACH L298N ENA/ENB headers!
   *                Hardwire ENA→5V and ENB→5V on those modules!
   */

#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// Check if Bluetooth is enabled
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth is not enabled! Please run 'menuconfig' and enable Bluetooth"
#endif

BluetoothSerial btSerial;

// ===== TANK MOTORS (Module 1) - WITH PWM =====
const int TANK_LEFT_ENA = 27;   // PWM ✅ User confirmed works!
const int TANK_LEFT_IN1 = 26;
const int TANK_LEFT_IN2 = 25;
const int TANK_RIGHT_ENB = 12;  // PWM ✅
const int TANK_RIGHT_IN3 = 13;
const int TANK_RIGHT_IN4 = 14;

// ===== CLAW MOTORS (Module 2) - ENA/ENB HARDWIRED TO 5V =====
const int CLAW_GRAB_IN1 = 16;
const int CLAW_GRAB_IN2 = 17;
const int CLAW_ROTATE_IN3 = 18;
const int CLAW_ROTATE_IN4 = 19;

// ===== REACH MOTORS (Module 3) - ENA/ENB HARDWIRED TO 5V =====
const int REACH_MIDDLE_IN1 = 2;
const int REACH_MIDDLE_IN2 = 4;
const int REACH_BASE_IN3 = 32;
const int REACH_BASE_IN4 = 33;

// ===== PWM CONFIG =====
const int PWM_FREQ = 3000;
const int PWM_RES = 8;
const int PWM_CH_LEFT = 0;
const int PWM_CH_RIGHT = 1;

// ===== PARAMETERS =====
const int MAX_SPEED = 255;      // Max PWM (0-255)
const int MIN_SPEED = 0;        // Min speed to overcome stall
const int DEFAULT_SPEED = 150;

const int BUZZER_PIN = 23;

// Buzzer frequencies for different events (in Hz)
const int BUZZER_FREQ_STARTUP = 1000;    // Startup beep
const int BUZZER_FREQ_CONNECT = 1500;    // Connection confirmation
const int BUZZER_FREQ_DISCONNECT = 500;  // Disconnect warning
const int BUZZER_FREQ_ALARM = 300;       // Failsafe alarm (low pitch)
const int BUZZER_FREQ_ERROR = 2000;      // Error/unknown command

// ===== BLUETOOTH SETTINGS =====
const char* BT_DEVICE_NAME = "ESP32TankRobot";
bool btConnected = false;

// ===== BLE DEFINITIONS =====
#define BLE_SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_TX_CHAR_UUID        "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // Write (web → ESP32)
#define BLE_RX_CHAR_UUID        "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // Notify (ESP32 → web)

BLEServer* pServer = NULL;
BLEService* pService = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
BLECharacteristic* pRxCharacteristic = NULL;
bool bleConnected = false;
String bleCommandBuffer = "";

// ===== BUZZER PWM SETUP =====
const int BUZZER_LEDC_CH = 2;  // Use LEDC channel 2 for buzzer
const int BUZZER_DUTY = 128;   // 50% duty cycle (0-255)

// ===== FUNCTION FORWARD DECLARATIONS =====
void buzzerTone(int frequency, int durationMs);
void sendBLE(String msg);
void log(String msg);

// ===== BLE CALLBACK CLASSES =====
class MyBLEServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleConnected = true;
        Serial.println("[BLE] Client connected");
        buzzerTone(BUZZER_FREQ_CONNECT, 150);  // Short high beep
    }
    void onDisconnect(BLEServer* pServer) {
        bleConnected = false;
        Serial.println("[BLE] Client disconnected");
        // Only beep if Classic BT is still connected (so we don't double-beep during failsafe)
        if (btSerial.connected()) {
            buzzerTone(BUZZER_FREQ_DISCONNECT, 100);  // Short low beep
        }
    }
};

class MyBLECharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            for (int i = 0; i < rxValue.length(); i++) {
                char c = rxValue[i];
                bleCommandBuffer += c;
            }
        }
    }
};

// ===== BUZZER FUNCTION =====
void buzzerTone(int frequency, int durationMs) {
    // Change frequency on LEDC channel 2
    ledcChangeFrequency(BUZZER_LEDC_CH, frequency, 8);
    // Start tone (50% duty cycle)
    ledcWrite(BUZZER_LEDC_CH, BUZZER_DUTY);
    delay(durationMs);
    // Stop tone
    ledcWrite(BUZZER_LEDC_CH, 0);
}

// ===== BLE FUNCTION =====
void sendBLE(String msg) {
    if (bleConnected && pRxCharacteristic) {
        pRxCharacteristic->setValue((msg + "\n").c_str());
        pRxCharacteristic->notify();
    }
}

// ===== MOTOR CONTROL FUNCTIONS =====
void setTankLeftMotor(int speed) {
    speed = constrain(speed, -MAX_SPEED, MAX_SPEED);
    if (speed > 0) {
        digitalWrite(TANK_LEFT_IN1, HIGH);
        digitalWrite(TANK_LEFT_IN2, LOW);
        ledcWrite(PWM_CH_LEFT, speed);
    } else if (speed < 0) {
        digitalWrite(TANK_LEFT_IN1, LOW);
        digitalWrite(TANK_LEFT_IN2, HIGH);
        ledcWrite(PWM_CH_LEFT, -speed);
    } else {
        digitalWrite(TANK_LEFT_IN1, LOW);
        digitalWrite(TANK_LEFT_IN2, LOW);
        ledcWrite(PWM_CH_LEFT, 0);
    }
}

void setTankRightMotor(int speed) {
    speed = constrain(speed, -MAX_SPEED, MAX_SPEED);
    if (speed > 0) {
        digitalWrite(TANK_RIGHT_IN3, HIGH);
        digitalWrite(TANK_RIGHT_IN4, LOW);
        ledcWrite(PWM_CH_RIGHT, speed);
    } else if (speed < 0) {
        digitalWrite(TANK_RIGHT_IN3, LOW);
        digitalWrite(TANK_RIGHT_IN4, HIGH);
        ledcWrite(PWM_CH_RIGHT, -speed);
    } else {
        digitalWrite(TANK_RIGHT_IN3, LOW);
        digitalWrite(TANK_RIGHT_IN4, LOW);
        ledcWrite(PWM_CH_RIGHT, 0);
    }
}

void forwardTank(int speed) {
    speed = constrain(speed, MIN_SPEED, MAX_SPEED);
    setTankLeftMotor(speed);
    setTankRightMotor(speed);
    log("TANK: Forward " + String(speed));
}

void backwardTank(int speed) {
    speed = constrain(speed, MIN_SPEED, MAX_SPEED);
    setTankLeftMotor(-speed);
    setTankRightMotor(-speed);
    log("TANK: Backward " + String(speed));
}

void pivotLeftTank(int speed) {
    speed = constrain(speed, MIN_SPEED, MAX_SPEED);
    setTankLeftMotor(-speed);
    setTankRightMotor(speed);
    log("TANK: Pivot Left ⭐ " + String(speed));
}

void pivotRightTank(int speed) {
    speed = constrain(speed, MIN_SPEED, MAX_SPEED);
    setTankLeftMotor(speed);
    setTankRightMotor(-speed);
    log("TANK: Pivot Right ⭐ " + String(speed));
}

void tankTurnLeft(int speed) {
    speed = constrain(speed, MIN_SPEED, MAX_SPEED);
    setTankLeftMotor(speed * 0.6);
    setTankRightMotor(speed);
    log("TANK: Turn Left (L=" + String(speed*0.6) + " R=" + String(speed) + ")");
}

void tankTurnRight(int speed) {
    speed = constrain(speed, MIN_SPEED, MAX_SPEED);
    setTankLeftMotor(speed);
    setTankRightMotor(speed * 0.6);
    log("TANK: Turn Right (L=" + String(speed) + " R=" + String(speed*0.6) + ")");
}

void stopTank() {
    setTankLeftMotor(0);
    setTankRightMotor(0);
    log("TANK: Stop");
}

void parseTankMotorSpeeds(String speeds) {
    speeds.trim();

    // Find separator that's NOT at the beginning (for negative first value)
    int sepIndex = -1;
    for (int i = 1; i < speeds.length(); i++) {  // Start from index 1 to skip leading '-'
        if (speeds[i] == ' ' || speeds[i] == ',' || speeds[i] == '-') {
            sepIndex = i;
            break;
        }
    }

    if (sepIndex == -1) {
        // Single value - set both motors to same speed
        int speed = speeds.toInt();
        setTankLeftMotor(speed);
        setTankRightMotor(speed);
        log("Tank: Both set to " + String(speed));
        return;
    }

    String leftStr = speeds.substring(0, sepIndex);
    String rightStr = speeds.substring(sepIndex + 1);

    int leftSpeed = leftStr.toInt();
    int rightSpeed = rightStr.toInt();

    setTankLeftMotor(leftSpeed);
    setTankRightMotor(rightSpeed);

    log("Tank: L=" + String(leftSpeed) + " R=" + String(rightSpeed));
}

// ===== NON-PWM MOTORS (Claw + Reach) =====
void setNonPWMMotor(int motor, char action) {
    int pin1 = -1, pin2 = -1;
    String motorName = "";

    switch (motor) {
        case 0: // Claw Grab
            pin1 = CLAW_GRAB_IN1;
            pin2 = CLAW_GRAB_IN2;
            motorName = "CLAW_GRAB";
            break;
        case 1: // Claw Rotate
            pin1 = CLAW_ROTATE_IN3;
            pin2 = CLAW_ROTATE_IN4;
            motorName = "CLAW_ROTATE";
            break;
        case 2: // Middle Reach
            pin1 = REACH_MIDDLE_IN1;
            pin2 = REACH_MIDDLE_IN2;
            motorName = "MIDDLE_REACH";
            break;
        case 3: // Base Reach
            pin1 = REACH_BASE_IN3;
            pin2 = REACH_BASE_IN4;
            motorName = "BASE_REACH";
            break;
        default:
            log("Invalid motor # (0-3)");
            return;
    }

    String msg;
    if (action == 'f') { // Forward
        digitalWrite(pin1, HIGH);
        digitalWrite(pin2, LOW);
        msg = motorName + ": Forward";
    }
    else if (action == 'b') { // Backward
        digitalWrite(pin1, LOW);
        digitalWrite(pin2, HIGH);
        msg = motorName + ": Backward";
    }
    else if (action == 's') { // Stop (coast)
        digitalWrite(pin1, LOW);
        digitalWrite(pin2, LOW);
        msg = motorName + ": Stop";
    }
    else if (action == 'x') { // Brake
        digitalWrite(pin1, HIGH);
        digitalWrite(pin2, HIGH);
        msg = motorName + ": Brake";
    }
    else {
        msg = "Invalid action: f=forward, b=backward, s=stop, x=brake";
    }

    log(msg);
}

void stopAllClawReach() {
    digitalWrite(CLAW_GRAB_IN1, LOW);
    digitalWrite(CLAW_GRAB_IN2, LOW);
    digitalWrite(CLAW_ROTATE_IN3, LOW);
    digitalWrite(CLAW_ROTATE_IN4, LOW);
    digitalWrite(REACH_MIDDLE_IN1, LOW);
    digitalWrite(REACH_MIDDLE_IN2, LOW);
    digitalWrite(REACH_BASE_IN3, LOW);
    digitalWrite(REACH_BASE_IN4, LOW);
    log("ALL CLAW/REACH STOPPED");
}

void stopAllMotors() {
    stopTank();
    stopAllClawReach();
}

// ===== HELP & DEMO =====
void printHelp() {
    String help =
        "\n========== TANK ROBOT COMMANDS ==========\n"
        "TANK (with PWM speed control -200 to +200):\n"
        "  tf[speed]   - Forward (e.g., tf150 or tf)\n"
        "  tb[speed]   - Backward\n"
        "  tl[speed]   - Pivot Left ⭐ (rotate in place)\n"
        "  tr[speed]   - Pivot Right ⭐\n"
        "  ttl[speed]  - Tank Turn Left (gentle)\n"
        "  ttr[speed]  - Tank Turn Right (gentle)\n"
        "  ts          - Stop tank\n"
        "  tms<L><R>   - Set tank motors separately (e.g., tms150-100, tms-100,50)\n"
        "\nCLAW & REACH (always full speed):\n"
        "  m0f - Claw Grab forward\n"
        "  m0b - Claw Grab backward\n"
        "  m0s - Claw Grab stop (coast)\n"
        "  m0x - Claw Grab brake\n"
        "  m1f - Claw Rotate forward\n"
        "  m1b - Claw Rotate backward\n"
        "  m1s - Claw Rotate stop\n"
        "  m1x - Claw Rotate brake\n"
        "  m2f - Middle Reach forward\n"
        "  m2b - Middle Reach backward\n"
        "  m2s - Middle Reach stop\n"
        "  m2x - Middle Reach brake\n"
        "  m3f - Base Reach forward\n"
        "  m3b - Base Reach backward\n"
        "  m3s - Base Reach stop\n"
        "  m3x - Base Reach brake\n"
        "\nSYSTEM:\n"
        "  d    - Run demo sequence\n"
        "  status - Print current status\n"
        "  s - Emergency stop all motors\n"
        "  h    - This help\n"
        "Bluetooth Classic: " + String(BT_DEVICE_NAME) + "\n"
        "BLE:              ESP32TankRobot\n"
        "==========================================";

    log(help);
}

void printStatus() {
    String status =
        "\n===== STATUS =====\n"
        "Bluetooth Classic: " + String(btConnected ? "CONNECTED" : "DISCONNECTED") + "\n"
        "BLE: " + String(bleConnected ? "CONNECTED" : "DISCONNECTED") + "\n"
        "Tank left PWM: " + String(analogRead(TANK_LEFT_ENA)) + "/255\n"
        "Tank right PWM: " + String(analogRead(TANK_RIGHT_ENB)) + "/255\n"
        "==================";

    log(status);
}

void runDemo() {
    log("\n>>> FULL SYSTEM DEMO <<<");

    delay(1000);
    log("1. TANK: Forward");
    forwardTank(150);
    delay(500);

    log("2. TANK: Pivot Left ⭐");
    pivotLeftTank(180);
    delay(500);

    log("3. TANK: Pivot Right ⭐");
    pivotRightTank(180);
    delay(500);

    log("4. CLAW: Grab forward");
    setNonPWMMotor(0, 'f');
    delay(1000);

    log("5. CLAW: Grab backward");
    setNonPWMMotor(0, 'b');
    delay(1000);

    log("5.5. CLAW: Stop");
    stopAllClawReach();
    delay(500);

    log("6. CLAW: Rotate forward");
    setNonPWMMotor(1, 'f');
    delay(1000);

    log("6. CLAW: Rotate backward");
    setNonPWMMotor(1, 'b');
    delay(1000);

    log("7. CLAW: Stop");
    stopAllClawReach();
    delay(500);

    log("8. REACH: Middle extend");
    setNonPWMMotor(2, 'f');
    delay(500);

    log("8. REACH: Middle retract");
    setNonPWMMotor(2, 'b');
    delay(500);

    log("8.5. REACH: Stop");
    stopAllClawReach();
    delay(500);

    log("9. REACH: Base rotate");
    setNonPWMMotor(3, 'f');
    delay(500);
    setNonPWMMotor(3, 'b');
    delay(500);

    log("10. All stop");
    stopTank();
    stopAllClawReach();

    log(">>> DEMO COMPLETE <<<\n");
}

// ===== UTILITY =====
int extractSpeed(String cmd, int startPos) {
    String speedStr = "";
    for (int i = startPos; i < cmd.length(); i++) {
        if (isDigit(cmd[i])) {
            speedStr += cmd[i];
        }
    }
    if (speedStr.length() == 0) return DEFAULT_SPEED;
    int speed = speedStr.toInt();
    return constrain(speed, MIN_SPEED, MAX_SPEED);
}

void log(String msg) {
    Serial.println(msg);
    if (btConnected) btSerial.println(msg);
    sendBLE(msg);
}

// ===== COMMAND PROCESSING =====
void processCommand(String cmd) {
    cmd.toLowerCase();

    // Echo command to all outputs
    String echo = "[CMD] " + cmd;
    Serial.println(echo);
    if (btConnected) btSerial.println(echo);
    sendBLE(echo);

    // === TANK COMMANDS (with speed) ===
    if (cmd == "tf") forwardTank(DEFAULT_SPEED);
    else if (cmd == "tb") backwardTank(DEFAULT_SPEED);
    else if (cmd == "tl") pivotLeftTank(DEFAULT_SPEED);
    else if (cmd == "tr") pivotRightTank(DEFAULT_SPEED);
    else if (cmd == "ttl") tankTurnLeft(DEFAULT_SPEED);
    else if (cmd == "ttr") tankTurnRight(DEFAULT_SPEED);
    else if (cmd == "ts") stopTank();
    else if (cmd.startsWith("tf")) forwardTank(extractSpeed(cmd, 2));
    else if (cmd.startsWith("tb")) backwardTank(extractSpeed(cmd, 2));
    else if (cmd.startsWith("tl")) pivotLeftTank(extractSpeed(cmd, 2));
    else if (cmd.startsWith("tr")) pivotRightTank(extractSpeed(cmd, 2));
    else if (cmd.startsWith("ttl")) tankTurnLeft(extractSpeed(cmd, 3));
    else if (cmd.startsWith("ttr")) tankTurnRight(extractSpeed(cmd, 3));
    else if (cmd.startsWith("tms")) parseTankMotorSpeeds(cmd.substring(3));

    // === CLAW & REACH COMMANDS ===
    // Format: m<number><action>
    // Motors: 0=claw grab, 1=claw rotate, 2=middle reach, 3=base reach
    else if (cmd.startsWith("m") && cmd.length()>=3) {
        int motor = cmd[1] - '0';
        char action = cmd[2];
        setNonPWMMotor(motor, action);
    }

    // === SYSTEM COMMANDS ===
    else if (cmd == "h" || cmd == "help") printHelp();
    else if (cmd == "d" || cmd == "demo") runDemo();
    else if (cmd == "status") printStatus();
    else if (cmd == "s") {
        stopTank();
        stopAllClawReach();
        String msg = "ALL STOP";
        Serial.println(msg);
        if (btConnected) btSerial.println(msg);
        sendBLE(msg);
        buzzerTone(BUZZER_FREQ_ERROR, 200);  // Error beep for emergency stop
    }
    else {
        String msg = "Unknown: " + cmd + " (type 'h' for help)";
        Serial.println(msg);
        if (btConnected) btSerial.println(msg);
        sendBLE(msg);
        buzzerTone(BUZZER_FREQ_ERROR, 100);  // Error beep for unknown command
    }
}

// ===== SETUP & LOOP =====
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("==========================================");
    Serial.println("ESP32 ROBOT - DUAL BLUETOOTH CONTROL");
    Serial.println("Classic: " + String(BT_DEVICE_NAME));
    Serial.println("BLE:     ESP32TankRobot");
    Serial.println("==========================================");

    // Initialize ALL motor control pins FIRST
    pinMode(TANK_LEFT_ENA, OUTPUT);
    pinMode(TANK_LEFT_IN1, OUTPUT);
    pinMode(TANK_LEFT_IN2, OUTPUT);
    pinMode(TANK_RIGHT_ENB, OUTPUT);
    pinMode(TANK_RIGHT_IN3, OUTPUT);
    pinMode(TANK_RIGHT_IN4, OUTPUT);

    pinMode(CLAW_GRAB_IN1, OUTPUT);
    pinMode(CLAW_GRAB_IN2, OUTPUT);
    pinMode(CLAW_ROTATE_IN3, OUTPUT);
    pinMode(CLAW_ROTATE_IN4, OUTPUT);

    pinMode(REACH_MIDDLE_IN1, OUTPUT);
    pinMode(REACH_MIDDLE_IN2, OUTPUT);
    pinMode(REACH_BASE_IN3, OUTPUT);
    pinMode(REACH_BASE_IN4, OUTPUT);

    pinMode(BUZZER_PIN, OUTPUT);

    // Setup PWM for tank motors
    ledcSetup(PWM_CH_LEFT, PWM_FREQ, PWM_RES);
    ledcAttachPin(TANK_LEFT_ENA, PWM_CH_LEFT);
    ledcSetup(PWM_CH_RIGHT, PWM_FREQ, PWM_RES);
    ledcAttachPin(TANK_RIGHT_ENB, PWM_CH_RIGHT);

    // Setup PWM for buzzer (passive buzzer on pin 23)
    ledcSetup(BUZZER_LEDC_CH, BUZZER_FREQ_STARTUP, 8);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CH);

    // Stop everything immediately
    stopAllMotors();

    // ==== INIT BLUETOOTH CLASSIC ====
    btSerial.begin(BT_DEVICE_NAME);
    delay(500);
    Serial.println("[Classic] Bluetooth started. Device name: " + String(BT_DEVICE_NAME));
    Serial.println("[Classic] Pair with your computer's Bluetooth...");

    // ==== INIT BLE ====
    BLEDevice::init("ESP32TankRobot");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyBLEServerCallbacks());

    pService = pServer->createService(BLE_SERVICE_UUID);

    // TX: Web app writes to this (ESP32 reads)
    pTxCharacteristic = pService->createCharacteristic(
        BLE_TX_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pTxCharacteristic->setCallbacks(new MyBLECharacteristicCallbacks());

    // RX: ESP32 writes to this (Web app reads via notify)
    pRxCharacteristic = pService->createCharacteristic(
        BLE_RX_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pRxCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("[BLE] Started advertising as ESP32TankRobot");
    Serial.println("[BLE] Use Web Bluetooth to connect (Chrome/Edge)");

    // Send welcome messages
    btSerial.println("\n==========================================");
    btSerial.println("ESP32 TANK ROBOT - DUAL BLUETOOTH READY");
    btSerial.println("Classic: " + String(BT_DEVICE_NAME));
    btSerial.println("BLE:     ESP32TankRobot");
    btSerial.println("Then connect and send 'h' for help");
    btSerial.println("==========================================\n");

    sendBLE("\n==========================================\n");
    sendBLE("ESP32 TANK ROBOT - BLE READY\n");
    sendBLE("Use Web Bluetooth to connect\n");
    sendBLE("Type 'h' for help after connecting...\n");
    sendBLE("==========================================\n");

    Serial.println("\n==========================================");
    Serial.println("READY! Pair with '" + String(BT_DEVICE_NAME) + "' (Classic)");
    Serial.println("Or connect via BLE: ESP32TankRobot");
    Serial.println("==========================================\n");

    delay(200);
    // Startup sequence - 3 quick beeps
    for (int i = 0; i < 3; i++) {
        buzzerTone(BUZZER_FREQ_STARTUP, 100);
        delay(100);
    }
}

void loop() {
    static bool lastBtState = false;
    static bool lastBleState = false;
    static bool bothDisconnectedFlag = false;

    // ==== BLUETOOTH CLASSIC CONNECTION HANDLING ====
    bool currentBtConnected = btSerial.connected();
    bool currentBleConnected = bleConnected;

    // Classic Bluetooth connection change detection
    if (currentBtConnected && !lastBtState) {
        // Just connected
        Serial.println("[Classic] Client connected");
        btSerial.println("\n[Classic] Connected! Type 'h' for commands");
        // Connection tone - 2 quick beeps
        for (int i = 0; i < 2; i++) {
            buzzerTone(BUZZER_FREQ_CONNECT, 100);
            delay(100);
        }
        lastBtState = true;
    }
    else if (!currentBtConnected && lastBtState) {
        // Just disconnected
        Serial.println("[Classic] Client disconnected");
        // Only beep if BLE is still connected (so we don't double-beep during failsafe)
        if (currentBleConnected) {
            buzzerTone(BUZZER_FREQ_DISCONNECT, 100);
        }
        lastBtState = false;
    }

    // ==== BLE CONNECTION HANDLING (callbacks already set bleConnected) ====

    // ==== FAILSAFE: Check if BOTH connections are lost ====
    if (!currentBtConnected && !currentBleConnected) {
        if (!bothDisconnectedFlag) {
            bothDisconnectedFlag = true;
            Serial.println("[FAILSAFE] All Bluetooth connections lost!");
            log("FAILSAFE: Emergency stop - Bluetooth lost!");

            // Stop all motors immediately
            stopAllMotors();

            // Sound alarm - 3 long beeps (low alarm frequency)
            for (int i = 0; i < 3; i++) {
                buzzerTone(BUZZER_FREQ_ALARM, 400);
                delay(300);
            }
        }
    } else {
        bothDisconnectedFlag = false;
    }

    // ==== PROCESS BLE COMMANDS FROM BUFFER ====
    if (bleCommandBuffer.length() > 0) {
        int newlineIndex;
        while ((newlineIndex = bleCommandBuffer.indexOf('\n')) != -1) {
            String cmd = bleCommandBuffer.substring(0, newlineIndex);
            bleCommandBuffer = bleCommandBuffer.substring(newlineIndex + 1);
            cmd.trim();
            if (cmd.length() > 0) {
                processCommand(cmd);
            }
        }
    }

    // ==== CHECK BLUETOOTH CLASSIC FOR COMMANDS ====
    if (btSerial.available()) {
        String cmd = btSerial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
            processCommand(cmd);
        }
    }

    // ==== CHECK USB SERIAL FOR DEBUGGING COMMANDS ====
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
            // Only process if command starts with '!' to avoid confusion
            if (cmd.startsWith("!")) {
                cmd = cmd.substring(1);
                Serial.print("[USB CMD] ");
                Serial.println(cmd);
                processCommand(cmd);
            }
        }
    }

    delay(10); // Small delay to prevent busy loop
}
