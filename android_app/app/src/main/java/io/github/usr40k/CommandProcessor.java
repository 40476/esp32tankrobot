 package io.github.usr40k.esp32tankrobot;
 
 public class CommandProcessor {
     // Tank commands
     public static String tankStop() { return "ts"; }

     public static String tankSetMotors(int leftSpeed, int rightSpeed) {
         // Clamp values to -200 to 200 range
         leftSpeed = clampSpeed(leftSpeed);
         rightSpeed = clampSpeed(rightSpeed);
         return "tms" + leftSpeed + rightSpeed;
     }

     // Arm commands
     public static String motorCommand(int motorId, String action) {
         // motorId: 0-3, action: f(forward), b(backward), s(stop)
         return "m" + motorId + action;
     }

     public static String clawGrab(boolean forward) {
         return motorCommand(0, forward ? "f" : "b");
     }

     public static String clawRotate(boolean forward) {
         return motorCommand(1, forward ? "f" : "b");
     }

     public static String middleReach(boolean forward) {
         return motorCommand(2, forward ? "f" : "b");
     }

     public static String baseReach(boolean forward) {
         return motorCommand(3, forward ? "f" : "b");
     }

     public static String motorStop(int motorId) {
         return motorCommand(motorId, "s");
     }

     public static String stopAll() {
         return "s";
     }

     public static String status() {
         return "status";
     }

     private static int clampSpeed(int speed) {
         return Math.max(-200, Math.min(200, speed));
     }
 }
