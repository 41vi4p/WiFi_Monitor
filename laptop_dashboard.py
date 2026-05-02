#!/usr/bin/env python3
"""
WiFi Sniffer Analysis Dashboard
Receives data from ESP32 and displays comprehensive WiFi network analytics

Features:
- Real-time device tracking
- MAC address and signal strength visualization
- WiFi attack detection
- Device location estimation from RSSI
- Network statistics
"""

import serial
import threading
import time
from datetime import datetime
from collections import defaultdict, deque
import math
import json
from pathlib import Path

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import numpy as np

class WiFiDevice:
    """Represents a WiFi device with tracking data"""
    def __init__(self, mac):
        self.mac = mac
        self.rssi_history = deque(maxlen=60)
        self.first_seen = time.time()
        self.last_seen = time.time()
        self.packet_count = 0
        self.channel = 0
        self.probe_reqs = 0
        self.max_rssi = -100
        self.current_rssi = -100
        self.locations = deque(maxlen=10)  # Track multiple position estimates
        
    def update(self, rssi, channel, packets, probe_reqs):
        self.current_rssi = rssi
        self.max_rssi = max(self.max_rssi, rssi)
        self.rssi_history.append(rssi)
        self.channel = channel
        self.packet_count = packets
        self.probe_reqs = probe_reqs
        self.last_seen = time.time()
        
    def get_signal_strength(self):
        """Classify signal strength"""
        if self.current_rssi >= -50:
            return "Excellent"
        elif self.current_rssi >= -60:
            return "Good"
        elif self.current_rssi >= -70:
            return "Fair"
        elif self.current_rssi >= -80:
            return "Weak"
        else:
            return "Very Weak"
    
    def estimate_distance(self):
        """Estimate distance from RSSI using Friis Free Space Equation
        Distance(m) = 10^((TxPower - RSSI) / (10*N))
        Assuming TxPower = -40 dBm and N = 2 (free space)
        """
        tx_power = -40  # Typical WiFi transmit power
        n = 2.0  # Path loss exponent (2.0 for free space)
        
        if self.current_rssi == -100:
            return None
        
        distance = 10 ** ((tx_power - self.current_rssi) / (10 * n))
        return max(0.5, min(distance, 100))  # Clamp between 0.5m and 100m

class WiFiNetworkAnalyzer:
    """Main analysis engine for WiFi network data"""
    
    def __init__(self):
        self.devices = {}
        self.stats = {
            'total_packets': 0,
            'active_devices': 0,
            'unique_devices': 0,
            'beacons': 0,
            'probe_requests': 0,
            'probe_responses': 0,
            'deauth_attacks': 0,
            'disassoc_attacks': 0,
            'current_channel': 1,
            'attack_detected': False,
            'attack_type': 'None'
        }
        self.attack_history = deque(maxlen=60)
        self.device_count_history = deque(maxlen=60)
        self.suspicious_devices = set()
        
    def update_from_esp_data(self, data_dict):
        """Process data received from ESP32"""
        self.stats['active_devices'] = data_dict.get('ACT', 0)
        self.stats['unique_devices'] = data_dict.get('DEV', 0)
        self.stats['total_packets'] = data_dict.get('PKT', 0)
        self.stats['attack_detected'] = data_dict.get('ATTACK', 0) == 1
        self.stats['attack_type'] = data_dict.get('TYPE', 'None')
        self.stats['beacons'] = data_dict.get('BEA', 0)
        self.stats['probe_requests'] = data_dict.get('PRQ', 0)
        self.stats['probe_responses'] = data_dict.get('PRR', 0)
        self.stats['deauth_attacks'] = data_dict.get('DEA', 0)
        
        self.device_count_history.append(self.stats['active_devices'])
        self.attack_history.append(1 if self.stats['attack_detected'] else 0)
        
        # Parse device data
        if 'devices_list' in data_dict:
            self.update_devices(data_dict['devices_list'])
    
    def update_devices(self, device_list):
        """Update tracked devices from device list"""
        for dev_data in device_list:
            mac, rssi, packets, channel, probe_reqs = dev_data
            
            if mac not in self.devices:
                self.devices[mac] = WiFiDevice(mac)
            
            rssi_int = int(rssi)
            self.devices[mac].update(rssi_int, int(channel), int(packets), int(probe_reqs))
            
            # Detect suspicious devices
            if int(probe_reqs) > 50:
                self.suspicious_devices.add(mac)
    
    def get_top_devices(self, n=10):
        """Get top N devices by signal strength"""
        sorted_devices = sorted(
            self.devices.items(),
            key=lambda x: x[1].max_rssi,
            reverse=True
        )
        return sorted_devices[:n]
    
    def get_closest_devices(self, n=5):
        """Get closest devices by signal strength"""
        sorted_devices = sorted(
            self.devices.items(),
            key=lambda x: x[1].current_rssi,
            reverse=True
        )
        return sorted_devices[:n]
    
    def analyze_attack_pattern(self):
        """Analyze recent attack patterns"""
        if len(self.attack_history) < 5:
            return None
        
        recent_attacks = sum(list(self.attack_history)[-5:])
        if recent_attacks >= 3:
            return {
                'severity': 'HIGH' if recent_attacks >= 4 else 'MEDIUM',
                'recent_count': recent_attacks,
                'type': self.stats['attack_type']
            }
        return None

class SerialDataReceiver(threading.Thread):
    """Background thread to receive serial data from ESP32"""
    
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        super().__init__(daemon=True)
        self.port = port
        self.baudrate = baudrate
        self.running = False
        self.analyzer = WiFiNetworkAnalyzer()
        self.data_callbacks = []
        self.error_callbacks = []
        
    def add_callback(self, callback):
        """Register callback for new data"""
        self.data_callbacks.append(callback)
    
    def add_error_callback(self, callback):
        """Register callback for errors"""
        self.error_callbacks.append(callback)
    
    def run(self):
        """Main loop - receive and parse serial data"""
        try:
            ser = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"Connected to {self.port} at {self.baudrate} baud")
            self.running = True
            
            while self.running:
                try:
                    if ser.in_waiting:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        
                        # Parse ESP32 data format: $DATA|...|END
                        if line.startswith('$DATA|'):
                            parsed_data = self.parse_esp_data(line)
                            if parsed_data:
                                self.analyzer.update_from_esp_data(parsed_data)
                                
                                # Call all registered callbacks
                                for callback in self.data_callbacks:
                                    callback(self.analyzer)
                        else:
                            # Regular debug output
                            print(f"[ESP]: {line}")
                    
                    time.sleep(0.01)
                    
                except Exception as e:
                    print(f"Error processing data: {e}")
                    for callback in self.error_callbacks:
                        callback(str(e))
                    time.sleep(0.5)
            
            ser.close()
            
        except serial.SerialException as e:
            print(f"Serial connection error: {e}")
            for callback in self.error_callbacks:
                callback(f"Serial error: {e}")
    
    def parse_esp_data(self, line):
        """Parse data from ESP32 format"""
        try:
            # Remove header and footer
            data_str = line.replace('$DATA|', '').replace('|END', '')
            
            parsed = {}
            devices_list = []
            
            parts = data_str.split('|')
            for part in parts:
                if part.startswith('DEVICES:'):
                    # Parse device data
                    dev_data = part.replace('DEVICES:', '')
                    if dev_data:
                        for dev_entry in dev_data.split(';'):
                            if dev_entry:
                                fields = dev_entry.split('|')
                                if len(fields) >= 5:
                                    devices_list.append(fields)
                    parsed['devices_list'] = devices_list
                elif ':' in part:
                    key, value = part.split(':', 1)
                    try:
                        parsed[key] = int(value) if value.isdigit() else value
                    except:
                        parsed[key] = value
            
            return parsed if parsed else None
            
        except Exception as e:
            print(f"Parse error: {e}")
            return None
    
    def stop(self):
        """Stop the receiver thread"""
        self.running = False

class WiFiDashboard:
    """Main GUI dashboard"""
    
    def __init__(self, root):
        self.root = root
        self.root.title("WiFi Network Sniffer Analysis Dashboard")
        self.root.geometry("1400x900")
        
        self.receiver = SerialDataReceiver()
        self.receiver.add_callback(self.on_data_update)
        self.receiver.add_error_callback(self.on_error)
        
        self.setup_ui()
        self.update_loop()
        
    def setup_ui(self):
        """Create UI elements"""
        # Main container
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Left panel - Controls and Info
        left_panel = ttk.Frame(main_frame, width=350)
        left_panel.pack(side=tk.LEFT, fill=tk.BOTH, padx=5)
        
        # Connection status
        conn_frame = ttk.LabelFrame(left_panel, text="Connection", padding=10)
        conn_frame.pack(fill=tk.X, pady=5)
        
        ttk.Label(conn_frame, text="Serial Port:").grid(row=0, column=0, sticky=tk.W)
        self.port_var = tk.StringVar(value="/dev/ttyUSB0")
        ttk.Entry(conn_frame, textvariable=self.port_var, width=15).grid(row=0, column=1, sticky=tk.W)
        
        self.connect_btn = ttk.Button(conn_frame, text="Connect", command=self.start_connection)
        self.connect_btn.grid(row=1, column=0, columnspan=2, pady=5, sticky=tk.EW)
        
        self.status_label = ttk.Label(conn_frame, text="Disconnected", foreground="red")
        self.status_label.grid(row=2, column=0, columnspan=2)
        
        # Statistics
        stats_frame = ttk.LabelFrame(left_panel, text="Network Statistics", padding=10)
        stats_frame.pack(fill=tk.X, pady=5)
        
        self.stats_widgets = {}
        stats_data = [
            ('Active Devices', 'active_devices'),
            ('Unique Devices', 'unique_devices'),
            ('Total Packets', 'total_packets'),
            ('Beacons', 'beacons'),
            ('Probe Requests', 'probe_requests'),
            ('Deauth Attacks', 'deauth_attacks'),
            ('Current Channel', 'current_channel'),
        ]
        
        for i, (label, key) in enumerate(stats_data):
            ttk.Label(stats_frame, text=f"{label}:").grid(row=i, column=0, sticky=tk.W)
            value_label = ttk.Label(stats_frame, text="0", font=("Arial", 10, "bold"))
            value_label.grid(row=i, column=1, sticky=tk.E)
            self.stats_widgets[key] = value_label
        
        # Attack status
        attack_frame = ttk.LabelFrame(left_panel, text="Attack Status", padding=10)
        attack_frame.pack(fill=tk.X, pady=5)
        
        self.attack_label = ttk.Label(attack_frame, text="No attacks detected", foreground="green", font=("Arial", 11))
        self.attack_label.pack(fill=tk.X)
        
        self.attack_type_label = ttk.Label(attack_frame, text="Type: None")
        self.attack_type_label.pack(fill=tk.X)
        
        # Top Devices
        devices_frame = ttk.LabelFrame(left_panel, text="Top Devices (by signal)", padding=10)
        devices_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        self.devices_text = scrolledtext.ScrolledText(devices_frame, height=15, width=40, font=("Courier", 8))
        self.devices_text.pack(fill=tk.BOTH, expand=True)
        
        # Right panel - Graphs
        right_panel = ttk.Frame(main_frame)
        right_panel.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=5)
        
        # Create figure for graphs
        self.fig = Figure(figsize=(10, 8), dpi=100)
        self.fig.patch.set_facecolor('#f0f0f0')
        
        # Create subplots
        self.ax1 = self.fig.add_subplot(2, 2, 1)  # Device count over time
        self.ax2 = self.fig.add_subplot(2, 2, 2)  # Attack timeline
        self.ax3 = self.fig.add_subplot(2, 2, 3)  # RSSI distribution
        self.ax4 = self.fig.add_subplot(2, 2, 4)  # Signal strength of top devices
        
        self.canvas = FigureCanvasTkAgg(self.fig, master=right_panel)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
    def start_connection(self):
        """Start serial connection"""
        if not self.receiver.running:
            self.receiver.port = self.port_var.get()
            self.receiver.start()
            self.connect_btn.config(state=tk.DISABLED)
    
    def on_data_update(self, analyzer):
        """Called when new data is received"""
        self.update_statistics(analyzer)
        self.update_device_list(analyzer)
        self.update_graphs(analyzer)
    
    def on_error(self, error_msg):
        """Called when an error occurs"""
        self.status_label.config(text=f"Error: {error_msg}", foreground="red")
    
    def update_statistics(self, analyzer):
        """Update statistics display"""
        if analyzer.stats['attack_detected']:
            self.status_label.config(text="CONNECTED - ATTACK DETECTED!", foreground="red")
            self.attack_label.config(text="⚠️ ATTACK DETECTED!", foreground="red")
            self.attack_type_label.config(text=f"Type: {analyzer.stats['attack_type']}")
        else:
            self.status_label.config(text="Connected", foreground="green")
            self.attack_label.config(text="✓ No attacks detected", foreground="green")
            self.attack_type_label.config(text="Type: None")
        
        # Update all stat widgets
        for key, widget in self.stats_widgets.items():
            value = analyzer.stats.get(key, 0)
            widget.config(text=str(value))
    
    def update_device_list(self, analyzer):
        """Update top devices list"""
        self.devices_text.config(state=tk.NORMAL)
        self.devices_text.delete(1.0, tk.END)
        
        self.devices_text.insert(tk.END, "MAC Address      | RSSI | Sig Strength | Dist\n")
        self.devices_text.insert(tk.END, "-" * 50 + "\n")
        
        for mac, device in analyzer.get_closest_devices(15):
            signal = device.get_signal_strength()
            distance = device.estimate_distance()
            dist_str = f"{distance:.1f}m" if distance else "N/A"
            
            self.devices_text.insert(tk.END, 
                f"{mac:18s} | {device.current_rssi:4d} | {signal:12s} | {dist_str:5s}\n")
        
        self.devices_text.config(state=tk.DISABLED)
    
    def update_graphs(self, analyzer):
        """Update all graphs"""
        self.ax1.clear()
        self.ax2.clear()
        self.ax3.clear()
        self.ax4.clear()
        
        # Graph 1: Device count over time
        if analyzer.device_count_history:
            self.ax1.plot(list(analyzer.device_count_history), color='#0066CC', linewidth=2)
            self.ax1.fill_between(range(len(analyzer.device_count_history)), 
                                 analyzer.device_count_history, alpha=0.3, color='#0066CC')
            self.ax1.set_title("Active Devices Over Time")
            self.ax1.set_ylabel("Device Count")
            self.ax1.grid(True, alpha=0.3)
        
        # Graph 2: Attack timeline
        if analyzer.attack_history:
            colors = ['red' if x == 1 else 'green' for x in analyzer.attack_history]
            self.ax2.bar(range(len(analyzer.attack_history)), 
                        [int(x) for x in analyzer.attack_history],
                        color=colors, alpha=0.7)
            self.ax2.set_title("Attack Detection Timeline")
            self.ax2.set_ylabel("Attack (1=Yes, 0=No)")
            self.ax2.set_ylim(0, 1.2)
        
        # Graph 3: RSSI distribution
        if analyzer.devices:
            rssi_values = [dev.current_rssi for dev in analyzer.devices.values() if dev.current_rssi > -100]
            if rssi_values:
                self.ax3.hist(rssi_values, bins=15, color='#00AA00', alpha=0.7, edgecolor='black')
                self.ax3.set_title("RSSI Distribution")
                self.ax3.set_xlabel("Signal Strength (dBm)")
                self.ax3.set_ylabel("Device Count")
                self.ax3.grid(True, alpha=0.3, axis='y')
        
        # Graph 4: Top devices signal strength
        top_devices = analyzer.get_closest_devices(8)
        if top_devices:
            macs = [mac[:15] for mac, _ in top_devices]
            rssis = [dev.current_rssi for _, dev in top_devices]
            
            colors_rssi = ['#FF6B6B' if r < -70 else '#FFA500' if r < -60 else '#4CAF50' for r in rssis]
            self.ax4.barh(macs, rssis, color=colors_rssi, alpha=0.8)
            self.ax4.set_title("Closest Devices (Signal Strength)")
            self.ax4.set_xlabel("Signal Strength (dBm)")
            self.ax4.set_xlim(-100, -30)
            self.ax4.grid(True, alpha=0.3, axis='x')
        
        self.fig.tight_layout()
        self.canvas.draw_idle()
    
    def update_loop(self):
        """Periodic update loop"""
        self.root.after(500, self.update_loop)

def main():
    """Start the dashboard"""
    root = tk.Tk()
    app = WiFiDashboard(root)
    root.mainloop()
    
    if app.receiver.running:
        app.receiver.stop()

if __name__ == "__main__":
    main()
