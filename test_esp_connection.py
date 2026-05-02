#!/usr/bin/env python3
"""Test script to diagnose ESP32 connection"""

import serial
import time

def test_port(port, baudrate=115200):
    print(f"\n🔍 Testing {port} at {baudrate} baud...")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
        print(f"✅ Port {port} opened successfully")
        
        # Clear any buffered data
        ser.reset_input_buffer()
        time.sleep(0.5)
        
        # Try to read data
        print("📍 Waiting for data (5 seconds)...\n")
        data_received = False
        
        for i in range(50):  # 5 seconds in 100ms intervals
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore')
                if line.strip():
                    print(f"[{i*100}ms] {line.strip()}")
                    data_received = True
            time.sleep(0.1)
        
        if not data_received:
            print("❌ No data received from ESP32")
            print("   Possible issues:")
            print("   - Firmware not running on ESP32")
            print("   - Firmware crashed or hung up")
            print("   - Wrong baud rate")
        else:
            print("\n✅ Data received! ESP32 is transmitting.")
        
        ser.close()
        return data_received
        
    except serial.SerialException as e:
        print(f"❌ Error opening port: {e}")
        return False
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        return False

if __name__ == "__main__":
    ports = ['/dev/ttyACM0', '/dev/ttyUSB0', '/dev/ttyUSB1']
    
    print("=" * 60)
    print("ESP32 CONNECTION TEST")
    print("=" * 60)
    
    for port in ports:
        try:
            import os
            if os.path.exists(port):
                if test_port(port):
                    break
            else:
                print(f"\n⏭️  {port} not found, skipping...")
        except Exception as e:
            print(f"Error testing {port}: {e}")
    
    print("\n" + "=" * 60)
    print("✅ TEST COMPLETE")
    print("=" * 60)
