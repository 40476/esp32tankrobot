/*
  * ESP32 TANK + 4 MOTORS WITH BLUETOOTH CLASSIC CONTROL
  * Using 3 L298N modules with independent H-bridges
  * Bluetooth: ESP32 acts as slave (SPP profile)
  *
  * Pin Configuration:
  * TANK:   ENA=12, IN1=13, IN2=14, ENB=25, IN3=26, IN4=27
  * CLAW:   IN1=16, IN2=17, IN3=18, IN4=19  (ENA/ENB hardwired to 5V)
  * REACH:  IN1=21, IN2=22, IN3=32, IN4=33  (ENA/ENB hardwired to 5V)
  *
  * Total pins: 14 (all used)
  *
  * ⚠️  IMPORTANT: Remove jumpers from CLAW/REACH L298N ENA/ENB headers!
  *                Hardwire ENA→5V and ENB→5V on those modules!
  */

 #include <BluetoothSerial.h>

 // Check if Bluetooth is enabled
 #if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
 #error "Bluetooth is not enabled! Please run 'menuconfig' and enable Bluetooth"
 #endif

 BluetoothSerial btSerial;

 // ===== TANK MOTORS (Module 1) - WITH PWM =====
 // Left Track
 const int TANK_LEFT_ENA = 12;   // PWM ✅ User confirmed works!
 const int TANK_LEFT_IN1 = 13;
 const int TANK_LEFT_IN2 = 14;

 // Right Track
 const int TANK_RIGHT_ENB = 27;  // PWM ✅
 const int TANK_RIGHT_IN3 = 26;
 const int TANK_RIGHT_IN4 = 25;

 // ===== CLAW MOTORS (Module 2) - ENA/ENB HARDWIRED TO 5V =====
 // Claw Grab (Motor A on Module 2)
 const int CLAW_GRAB_IN1 = 16;
 const int CLAW_GRAB_IN2 = 17;

 // Claw Rotate (Motor B on Module 2)
 const int CLAW_ROTATE_IN3 = 18;
 const int CLAW_ROTATE_IN4 = 19;

 // ===== REACH MOTORS (Module 3) - ENA/ENB HARDWIRED TO 5V =====
 // Middle Reach (Motor A on Module 3)
 const int REACH_MIDDLE_IN1 = 21;  // ✅ Safe pin
 const int REACH_MIDDLE_IN2 = 22;  // ✅ Safe pin

 // Base Reach (Motor B on Module 3)
 const int REACH_BASE_IN3 = 32;
 const int REACH_BASE_IN4 = 33;

 // ===== PWM CONFIG =====
 const int PWM_FREQ = 3000;
 const int PWM_RES = 8;
 const int PWM_CH_LEFT = 0;
 const int PWM_CH_RIGHT = 1;

 // ===== PARAMETERS =====
 const int MAX_SPEED = 255;      // Max PWM (0-255)
 const int MIN_SPEED = 0;       // Min speed to overcome stall
 const int DEFAULT_SPEED = 150;

 const int BUZZER_PIN = 23;
 // ===== BLUETOOTH SETTINGS =====
 const char* BT_DEVICE_NAME = "ESP32TankRobot";
 bool btConnected = false;

 void setup() {
   // Initialize USB serial for debugging
   Serial.begin(115200);
   delay(1000);

   Serial.println("==========================================");
   Serial.println("ESP32 ROBOT - BLUETOOTH CLASSIC CONTROL");
   Serial.println("==========================================");

   // Initialize Bluetooth
   btSerial.begin(BT_DEVICE_NAME);
   Serial.println("Bluetooth started. Device name: " + String(BT_DEVICE_NAME));
   Serial.println("Pair with your computer's Bluetooth...");

   // Initialize all motor control pins
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

   // Stop everything
   stopTank();
   stopAllClawReach();

   Serial.println("\n==========================================");
   Serial.println("READY! Pair with '" + String(BT_DEVICE_NAME) + "'");
   Serial.println("Then run: python3 tank_bt_control.py");
   Serial.println("==========================================\n");

   // Send welcome message to Bluetooth when connected
   // (will be buffered until connection)
   btSerial.println("\n==========================================");
   btSerial.println("ESP32 TANK ROBOT - BLUETOOTH READY");
   btSerial.println("Pair with: " + String(BT_DEVICE_NAME));
   btSerial.println("Then run: python3 tank_bt_control.py");
   btSerial.println("==========================================\n");
   btSerial.println("Type 'h' for help once connected...");
   digitalWrite(BUZZER_PIN, HIGH);
   delay(50);
   digitalWrite(BUZZER_PIN, LOW);
   delay(50);
   digitalWrite(BUZZER_PIN, HIGH);
   delay(50);
   digitalWrite(BUZZER_PIN, LOW);
   delay(100);
   digitalWrite(BUZZER_PIN, HIGH);
   delay(50);
   digitalWrite(BUZZER_PIN, LOW);
   delay(50);
 }

 void loop() {
   // Update connection status
   if (btSerial.connected() && !btConnected) {
     btConnected = true;
     Serial.println("[BT] Client connected");
     btSerial.println("\n[BT] Connected! Type 'h' for commands");
     digitalWrite(BUZZER_PIN, HIGH);
     delay(100);
     digitalWrite(BUZZER_PIN, LOW);
     delay(100);
     digitalWrite(BUZZER_PIN, HIGH);
     delay(100);
     digitalWrite(BUZZER_PIN, LOW);

   } else if (!btSerial.connected() && btConnected) {
     btConnected = false;
     Serial.println("[BT] Client disconnected");
     digitalWrite(BUZZER_PIN, HIGH);
     delay(100);
     digitalWrite(BUZZER_PIN, LOW);

   }

   // Check for commands from Bluetooth
   if (btSerial.available()) {
     String cmd = btSerial.readStringUntil('\n');
     cmd.trim();
     if (cmd.length() > 0) {
       processCommand(cmd);
     }
   }

   // Optional: Also check USB serial for debugging (if connected)
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

 // ===== COMMAND PROCESSING =====
 void processCommand(String cmd) {
   cmd.toLowerCase();

   // Echo command to both outputs
   String echo = "[CMD] " + cmd;
   Serial.println(echo);
   if (btConnected) btSerial.println(echo);

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
   // Actions: f=forward, b=backward, s=stop, x=brake
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
     Serial.println("ALL STOP");
     if (btConnected) btSerial.println("ALL STOP");
   }
   else {
     String msg = "Unknown: " + cmd + " (type 'h' for help)";
     Serial.println(msg);
     if (btConnected) btSerial.println(msg);
   }
 }

 // ===== TANK CONTROL (with PWM) =====
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
   int sepIndex = -1;
   for (int i = 0; i < speeds.length(); i++) {
     if (speeds[i] == ' ' || speeds[i] == '-' || speeds[i] == ',') {
       sepIndex = i;
       break;
     }
   }

   if (sepIndex == -1) {
     log("Format: tms<left><sep><right> (e.g., tms150-100)");
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
 // Motors 0-3: 0=claw grab, 1=claw rotate, 2=middle reach, 3=base reach
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

   if (action == 'f') { // Forward
     digitalWrite(pin1, HIGH);
     digitalWrite(pin2, LOW);
     log(motorName + ": Forward");
   }
   else if (action == 'b') { // Backward
     digitalWrite(pin1, LOW);
     digitalWrite(pin2, HIGH);
     log(motorName + ": Backward");
   }
   else if (action == 's') { // Stop (coast)
     digitalWrite(pin1, LOW);
     digitalWrite(pin2, LOW);
     log(motorName + ": Stop");
   }
   else if (action == 'x') { // Brake
     digitalWrite(pin1, HIGH);
     digitalWrite(pin2, HIGH);
     log(motorName + ": Brake");
   }
   else {
     log("Invalid action: f=forward, b=backward, s=stop, x=brake");
   }
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
 }

 // ===== HELP & DEMO =====
 void printHelp() {
   String help =
     "\n========== TANK ROBOT COMMANDS ==========\n"
     "TANK (with PWM speed control 0-200):\n"
     "  tf[speed]   - Forward (e.g., tf150 or tf)\n"
     "  tb[speed]   - Backward\n"
     "  tl[speed]   - Pivot Left ⭐ (rotate in place)\n"
     "  tr[speed]   - Pivot Right ⭐\n"
     "  ttl[speed]  - Tank Turn Left (gentle)\n"
     "  ttr[speed]  - Tank Turn Right (gentle)\n"
     "  ts          - Stop tank\n"
     "  tms<L><R>   - Set tank motors separately (e.g., tms150-100)\n"
     "\nCLAW & REACH (always full speed):\n"
     "  m0f - Claw Grab forward\n"
     "  m0b - Claw Grab backward\n"
     "  m0s - Claw Grab stop (coast)\n"
     "  m0x - Claw Grab brake\n"
     "  m1f - Claw Rotate forward\n"
     "  m1b - Claw Rotate backward\n"
     "  m2f - Middle Reach forward\n"
     "  m2b - Middle Reach backward\n"
     "  m3f - Base Reach forward\n"
     "  m3b - Base Reach backward\n"
     "\nSYSTEM:\n"
     "  d    - Run demo sequence\n"
     "  status - Print current status\n"
     "  s - Emergency stop all motors\n"
     "  h    - This help\n"
     "==========================================\n"
     "Note: Speeds 0-200. Negative values = reverse.\n"
     "Bluetooth: " + String(BT_DEVICE_NAME) + "\n";

   Serial.println(help);
   if (btConnected) btSerial.println(help);
 }

 void printStatus() {
   String status =
     "\n===== STATUS =====\n"
     "Bluetooth: " + String(btConnected ? "CONNECTED" : "DISCONNECTED") + "\n"
     "Tank left: " + String(analogRead(TANK_LEFT_ENA) * 100 / 255) + "%\n"
     "Tank right: " + String(analogRead(TANK_RIGHT_ENB) * 100 / 255) + "%\n"
     "==================\n";

   Serial.println(status);
   if (btConnected) btSerial.println(status);
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
   if (btConnected) {
     btSerial.println(msg);
   }
 }
