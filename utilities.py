#!/usr/bin/env python3
"""
WiFi Sniffer Utilities
Helper functions for data export, analysis, and testing
"""

import serial
import serial.tools.list_ports
import json
import csv
from datetime import datetime
from pathlib import Path

def find_esp32_port():
    """Auto-detect ESP32 serial port"""
    ports = serial.tools.list_ports.comports()
    
    esp32_ports = []
    for port in ports:
        # Look for common ESP32 USB adapters
        if 'CH340' in port.description or 'CH341' in port.description:
            esp32_ports.append(port.device)
        elif 'CP210x' in port.description:
            esp32_ports.append(port.device)
        elif 'Silicon Labs' in port.description:
            esp32_ports.append(port.device)
        elif 'USB Serial' in port.description or 'USB-SERIAL' in port.description:
            esp32_ports.append(port.device)
    
    return esp32_ports

def list_serial_ports():
    """List all available serial ports"""
    ports = serial.tools.list_ports.comports()
    
    print("\n" + "="*60)
    print("Available Serial Ports:")
    print("="*60)
    
    if not ports:
        print("No serial ports found!")
        return
    
    esp32_found = False
    for i, port in enumerate(ports, 1):
        print(f"\n{i}. Port: {port.device}")
        print(f"   Description: {port.description}")
        print(f"   Manufacturer: {port.manufacturer}")
        
        if 'CH340' in port.description or 'CP210x' in port.description or 'Silicon Labs' in port.description:
            print("   ✓ Likely ESP32 device")
            esp32_found = True
    
    if not esp32_found:
        print("\n⚠️  No obvious ESP32 device detected!")
        print("   Try the first USB port or check Arduino IDE for the port")
    
    print("\n" + "="*60 + "\n")

def test_esp32_connection(port="/dev/ttyUSB0", baudrate=115200):
    """Test connection to ESP32"""
    try:
        print(f"Connecting to {port} @ {baudrate} baud...")
        ser = serial.Serial(port, baudrate, timeout=2)
        
        print("✓ Connected! Listening for 10 seconds...")
        start_time = datetime.now()
        
        while (datetime.now() - start_time).seconds < 10:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"  ← {line}")
        
        ser.close()
        print("\n✓ Test successful!")
        return True
        
    except serial.SerialException as e:
        print(f"✗ Connection failed: {e}")
        return False

def export_to_csv(data_dict, filename=None):
    """Export captured device data to CSV"""
    if filename is None:
        filename = f"wifi_devices_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    
    with open(filename, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['MAC Address', 'RSSI', 'Packets', 'Channel', 'Probe Requests', 'Timestamp'])
        
        # Write data rows
        timestamp = datetime.now().isoformat()
        for mac, rssi, packets, channel, probe_reqs in data_dict.get('devices_list', []):
            writer.writerow([mac, rssi, packets, channel, probe_reqs, timestamp])
    
    print(f"✓ Data exported to {filename}")
    return filename

def export_to_json(analyzer_state, filename=None):
    """Export full analyzer state to JSON"""
    if filename is None:
        filename = f"wifi_analysis_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
    
    export_data = {
        'timestamp': datetime.now().isoformat(),
        'statistics': {
            'total_packets': analyzer_state.get('total_packets', 0),
            'active_devices': analyzer_state.get('active_devices', 0),
            'unique_devices': analyzer_state.get('unique_devices', 0),
            'beacons': analyzer_state.get('beacons', 0),
            'probe_requests': analyzer_state.get('probe_requests', 0),
            'deauth_attacks': analyzer_state.get('deauth_attacks', 0),
        },
        'devices': analyzer_state.get('devices_list', [])
    }
    
    with open(filename, 'w') as f:
        json.dump(export_data, f, indent=2)
    
    print(f"✓ Analysis exported to {filename}")
    return filename

def print_statistics(stats):
    """Pretty print statistics"""
    print("\n" + "="*60)
    print("Network Statistics")
    print("="*60)
    print(f"Active Devices:      {stats.get('ACT', 0)}")
    print(f"Unique Devices:      {stats.get('DEV', 0)}")
    print(f"Total Packets:       {stats.get('PKT', 0)}")
    print(f"Beacon Frames:       {stats.get('BEA', 0)}")
    print(f"Probe Requests:      {stats.get('PRQ', 0)}")
    print(f"Probe Responses:     {stats.get('PRR', 0)}")
    print(f"Deauth Attacks:      {stats.get('DEA', 0)}")
    print(f"Attack Detected:     {'Yes' if stats.get('ATTACK') else 'No'}")
    print(f"Attack Type:         {stats.get('TYPE', 'None')}")
    print("="*60 + "\n")

def analyze_rssi_values(rssi_list):
    """Analyze RSSI statistics"""
    if not rssi_list:
        return None
    
    rssi_values = [int(r) for r in rssi_list]
    
    return {
        'min': min(rssi_values),
        'max': max(rssi_values),
        'avg': sum(rssi_values) / len(rssi_values),
        'count': len(rssi_values)
    }

def main():
    """Command-line utility interface"""
    import sys
    
    print("\n" + "="*60)
    print("WiFi Sniffer System Utilities")
    print("="*60)
    
    if len(sys.argv) < 2:
        print("\nUsage: python3 utilities.py [command]")
        print("\nAvailable commands:")
        print("  ports          - List all serial ports")
        print("  find-esp32     - Find ESP32 device port")
        print("  test [port]    - Test connection to ESP32 (default: /dev/ttyUSB0)")
        print("\nExamples:")
        print("  python3 utilities.py find-esp32")
        print("  python3 utilities.py test /dev/ttyUSB0")
        return
    
    command = sys.argv[1].lower()
    
    if command == "ports":
        list_serial_ports()
    
    elif command == "find-esp32":
        ports = find_esp32_port()
        if ports:
            print(f"\n✓ Found ESP32 at: {ports[0]}")
            print(f"  Other matches: {', '.join(ports[1:])}")
        else:
            print("\n✗ No ESP32 device found")
            print("  Use 'ports' command to see available ports")
            list_serial_ports()
    
    elif command == "test":
        port = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyUSB0"
        test_esp32_connection(port)
    
    else:
        print(f"Unknown command: {command}")

if __name__ == "__main__":
    main()
