#!/usr/bin/env python3
"""
ESP32 Tank Robot Bluetooth Controller
Tty-friendly, no GUI required
"""

import sys
import bluetooth
import select
import argparse
import time

class TankBTController:
    def __init__(self, mac_address=None, device_name="ESP32TankRobot"):
        self.mac_address = mac_address
        self.device_name = device_name
        self.sock = None
        self.connected = False

    def discover_device(self):
        """Discover ESP32 by name if MAC not provided"""
        print(f"Scanning for device '{self.device_name}'...")
        try:
            nearby_devices = bluetooth.discover_devices(duration=5, lookup_names=True)
            for addr, name in nearby_devices:
                if self.device_name in name:
                    print(f"Found: {name} ({addr})")
                    return addr
        except Exception as e:
            print(f"Discovery error: {e}")
        return None

    def connect(self):
        """Connect to ESP32 via Bluetooth"""
        if self.mac_address is None:
            self.mac_address = self.discover_device()
            if self.mac_address is None:
                print("ERROR: Could not find ESP32 device")
                return False

        try:
            print(f"Connecting to {self.mac_address}...")
            self.sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
            self.sock.connect((self.mac_address, 1))
            self.sock.settimeout(0.5)  # Non-blocking with timeout
            self.connected = True
            print("✓ Connected!")
            return True
        except bluetooth.BluetoothError as e:
            print(f"Connection failed: {e}")
            return False

    def disconnect(self):
        """Disconnect from ESP32"""
        if self.sock:
            try:
                self.sock.close()
            except:
                pass
        self.connected = False
        print("Disconnected")

    def send_command(self, cmd):
        """Send command to ESP32"""
        if not self.connected:
            print("ERROR: Not connected")
            return False

        try:
            self.sock.send((cmd + "\n").encode())
            return True
        except Exception as e:
            print(f"Send failed: {e}")
            self.connected = False
            return False

    def listen(self):
        """Listen for incoming messages from ESP32"""
        if not self.connected:
            return ""

        try:
            data = self.sock.recv(1024)
            if data:
                return data.decode('utf-8', errors='ignore')
        except bluetooth.BluetoothError:
            # Timeout or no data
            pass
        except Exception as e:
            print(f"Receive error: {e}")
            self.connected = False
        return ""

    def run_interactive(self):
        """Main interactive loop"""
        print("\n" + "="*50)
        print("TANK ROBOT BLUETOOTH CONTROLLER")
        print("="*50)
        print("Commands:")
        print("  Tank: tf[spd], tb[spd], tl[spd], tr[spd], ttl[spd], ttr[spd]")
        print("        ts, tms<L><R> (e.g., tms150-100)")
        print("  Claw/Reach: m0f,m0b,m0s,m0x (claw grab)")
        print("             m1f,m1b (claw rotate)")
        print("             m2f,m2b (middle reach)")
        print("             m3f,m3b (base reach)")
        print("  System: d (demo), status, stopall, h (help)")
        print("  Ctrl+C to exit")
        print("="*50 + "\n")

        # Flush any pending data
        time.sleep(0.5)
        while True:
            try:
                # Check for Bluetooth input
                bt_data = self.listen()
                if bt_data:
                    sys.stdout.write(bt_data)
                    sys.stdout.flush()

                # Check for user input (non-blocking)
                if select.select([sys.stdin], [], [], 0.1) == ([sys.stdin], [], []):
                    cmd = sys.stdin.readline().strip()
                    if cmd.lower() in ['exit', 'quit', 'q']:
                        print("Exiting...")
                        break
                    if cmd:
                        self.send_command(cmd)

            except KeyboardInterrupt:
                print("\nExiting...")
                break
            except Exception as e:
                print(f"Error: {e}")
                break

def main():
    parser = argparse.ArgumentParser(description='Tank Robot Bluetooth Controller')
    parser.add_argument('--mac', help='ESP32 Bluetooth MAC address (e.g., 24:A1:62:XX:XX:XX)')
    parser.add_argument('--name', default='ESP32TankRobot', help='ESP32 Bluetooth device name')
    args = parser.parse_args()

    controller = TankBTController(mac_address=args.mac, device_name=args.name)

    if not controller.connect():
        print("\nTroubleshooting:")
        print("1. Make sure ESP32 is powered and running")
        print("2. ESP32 should show: 'Bluetooth started...'")
        print("3. Pair ESP32 in system Bluetooth settings first")
        print("4. Use --mac option with MAC address")
        print("   Find MAC with: python3 -m bluetooth <device_name>")
        sys.exit(1)

    try:
        controller.run_interactive()
    finally:
        controller.disconnect()

if __name__ == "__main__":
    main()
