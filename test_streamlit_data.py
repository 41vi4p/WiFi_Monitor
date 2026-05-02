#!/usr/bin/env python3
"""Debug script to see what data Streamlit is receiving from ESP32"""

import serial
import time

def test_streamlit_parsing():
    print("\n🔍 Testing Streamlit Data Parsing")
    print("=" * 60)
    
    port = '/dev/ttyACM1'
    baudrate = 115200
    
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
        print(f"✅ Connected to {port} at {baudrate} baud\n")
        
        # Clear buffer
        ser.reset_input_buffer()
        time.sleep(0.5)
        
        # Read some data lines
        print("📍 Reading data from ESP32 (10 seconds)...\n")
        lines_received = []
        data_lines_count = 0
        
        start_time = time.time()
        while time.time() - start_time < 10:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    lines_received.append(line)
                    
                    # Count data vs debug lines
                    if line.startswith('$DATA|'):
                        data_lines_count += 1
                        print(f"✅ DATA: {line[:100]}...")  # Print first 100 chars
                    else:
                        print(f"📝 DEBUG: {line}")
            
            time.sleep(0.01)
        
        print(f"\n{'=' * 60}")
        print(f"📊 Summary:")
        print(f"   Total lines received: {len(lines_received)}")
        print(f"   DATA lines: {data_lines_count}")
        print(f"   DEBUG lines: {len(lines_received) - data_lines_count}")
        
        if data_lines_count == 0:
            print("\n⚠️  NO DATA LINES RECEIVED!")
            print("   ESP32 is sending debug output but not the $DATA| format")
            print("   Check if sendDataToLaptop() is being called in esp_sniffer.ino")
        else:
            print("\n✅ DATA is being transmitted!")
            
            # Try to parse one data line
            if data_lines_count > 0:
                sample_line = [l for l in lines_received if l.startswith('$DATA|')][0]
                print(f"\n📋 Sample data line:")
                print(f"   {sample_line}\n")
                
                # Try parsing
                try:
                    data_str = sample_line.replace('$DATA|', '').replace('|END', '')
                    parts = data_str.split('|')
                    
                    print(f"✅ Parsed {len(parts)} fields:")
                    for i, part in enumerate(parts[:10]):  # Show first 10
                        print(f"   [{i}] {part}")
                    
                    # Check for device data
                    device_parts = [p for p in parts if 'DEVICES:' in p]
                    if device_parts:
                        print(f"\n✅ Found DEVICES field!")
                        dev_data = device_parts[0].replace('DEVICES:', '')
                        devices = dev_data.split(';')
                        print(f"   Devices in this packet: {len([d for d in devices if d])}")
                    
                except Exception as e:
                    print(f"❌ Parse error: {e}")
        
        ser.close()
        
    except serial.SerialException as e:
        print(f"❌ Serial error: {e}")
    except Exception as e:
        print(f"❌ Error: {e}")
    
    print("=" * 60 + "\n")

if __name__ == "__main__":
    test_streamlit_parsing()
