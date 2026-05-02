#!/usr/bin/env python3
"""
Simplified WiFi Sniffer Dashboard - Debug Version
"""

import streamlit as st
import serial
import threading
import time
from datetime import datetime
from collections import deque
import pandas as pd
import plotly.graph_objects as go
import plotly.express as px

st.set_page_config(page_title="WiFi Sniffer", page_icon="📡", layout="wide")

# ===== Data Classes =====
class WiFiDevice:
    def __init__(self, mac):
        self.mac = mac
        self.rssi_history = deque(maxlen=60)
        self.packet_count = 0
        self.channel = 0
        self.probe_reqs = 0
        self.current_rssi = -100
        self.max_rssi = -100
        
    def update(self, rssi, channel, packets, probe_reqs):
        self.current_rssi = int(rssi)
        self.max_rssi = max(self.max_rssi, self.current_rssi)
        self.rssi_history.append(self.current_rssi)
        self.channel = int(channel)
        self.packet_count = int(packets)
        self.probe_reqs = int(probe_reqs)
    
    def get_distance(self):
        if self.current_rssi == -100:
            return None
        tx_power = -40
        n = 2.0
        distance = 10 ** ((tx_power - self.current_rssi) / (10 * n))
        return max(0.5, min(distance, 100))
    
    def get_strength(self):
        if self.current_rssi >= -50:
            return "Excellent"
        elif self.current_rssi >= -60:
            return "Good"
        elif self.current_rssi >= -70:
            return "Fair"
        else:
            return "Weak"

class Analyzer:
    def __init__(self):
        self.devices = {}
        self.stats = {
            'ACT': 0, 'DEV': 0, 'PKT': 0, 'ATTACK': 0, 'TYPE': '',
            'BEA': 0, 'PRQ': 0, 'PRR': 0, 'DEA': 0
        }
        self.history = deque(maxlen=60)
        
    def parse_line(self, line):
        try:
            if not line.startswith('$DATA|'):
                return False
            
            data_str = line.replace('$DATA|', '').replace('|END', '')
            parts = data_str.split('|')
            
            # Parse stats
            for part in parts:
                if part.startswith('DEVICES:'):
                    dev_data = part.replace('DEVICES:', '').strip()
                    if dev_data:
                        for entry in dev_data.split(';'):
                            if entry.strip():
                                fields = entry.split(',')
                                if len(fields) >= 5:
                                    mac = fields[0].strip()
                                    if mac:
                                        if mac not in self.devices:
                                            self.devices[mac] = WiFiDevice(mac)
                                        self.devices[mac].update(fields[1], fields[3], fields[2], fields[4])
                elif ':' in part:
                    key, val = part.split(':', 1)
                    key = key.strip()
                    val = val.strip()
                    try:
                        self.stats[key] = int(val) if val.isdigit() else val
                    except:
                        self.stats[key] = val
            
            self.history.append(self.stats['ACT'])
            return True
        except Exception as e:
            print(f"Parse error: {e}")
            return False

class SerialReader(threading.Thread):
    def __init__(self, port, baudrate=115200):
        super().__init__(daemon=True)
        self.port = port
        self.baudrate = baudrate
        self.analyzer = Analyzer()
        self.running = False
        self.lines_read = 0
        
    def run(self):
        try:
            ser = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"Connected to {self.port}")
            self.running = True
            
            while self.running:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.analyzer.parse_line(line)
                        self.lines_read += 1
                time.sleep(0.01)
            
            ser.close()
        except Exception as e:
            print(f"Serial error: {e}")
    
    def stop(self):
        self.running = False

# ===== Session State =====
if 'reader' not in st.session_state:
    st.session_state.reader = None
if 'connected' not in st.session_state:
    st.session_state.connected = False

# ===== Sidebar =====
with st.sidebar:
    st.title("⚙️ Configuration")
    port = st.text_input("Serial Port", value="/dev/ttyACM1")
    
    col1, col2 = st.columns(2)
    with col1:
        if st.button("🔗 Connect", width='stretch'):
            if st.session_state.reader is None:
                st.session_state.reader = SerialReader(port)
                st.session_state.reader.start()
                st.session_state.connected = True
                time.sleep(0.5)
                st.success("✅ Connected!")
    
    with col2:
        if st.button("❌ Disconnect", width='stretch'):
            if st.session_state.reader:
                st.session_state.reader.stop()
                st.session_state.reader = None
                st.session_state.connected = False
                st.info("Disconnected")
    
    st.markdown("---")
    status = "🟢 Connected" if st.session_state.connected else "🔴 Disconnected"
    st.info(status)

# ===== Main Dashboard =====
st.title("📡 WiFi Sniffer Dashboard")

if st.session_state.connected and st.session_state.reader:
    analyzer = st.session_state.reader.analyzer
    
    # Key metrics
    col1, col2, col3, col4, col5 = st.columns(5)
    with col1:
        st.metric("Active Devices", analyzer.stats['ACT'])
    with col2:
        st.metric("Unique Devices", analyzer.stats['DEV'])
    with col3:
        st.metric("Packets", analyzer.stats['PKT'])
    with col4:
        st.metric("Beacons", analyzer.stats['BEA'])
    with col5:
        attack_status = "⚠️ ATTACK!" if analyzer.stats['ATTACK'] else "✅ Safe"
        st.metric("Status", attack_status)
    
    st.markdown("---")
    
    # Graphs
    if len(analyzer.history) > 1:
        col1, col2 = st.columns(2)
        
        with col1:
            st.subheader("📈 Active Devices Timeline")
            df = pd.DataFrame({'Active': list(analyzer.history)})
            fig = px.line(df, y='Active', markers=True, title="Devices Over Time")
            fig.update_layout(height=300, showlegend=False)
            st.plotly_chart(fig, width='stretch')
        
        with col2:
            st.subheader("📊 Frame Types")
            frame_data = {
                'Type': ['Beacons', 'Probe Req', 'Probe Resp', 'Deauth'],
                'Count': [analyzer.stats['BEA'], analyzer.stats['PRQ'], analyzer.stats['PRR'], analyzer.stats['DEA']]
            }
            fig = px.bar(frame_data, x='Type', y='Count', title="Frame Distribution")
            fig.update_layout(height=300, showlegend=False)
            st.plotly_chart(fig, width='stretch')
    
    st.markdown("---")
    
    # Device table
    st.subheader("📡 All Detected Devices")
    
    if analyzer.devices:
        devices_data = []
        for mac, dev in sorted(analyzer.devices.items(), key=lambda x: x[1].current_rssi, reverse=True):
            dist = dev.get_distance()
            devices_data.append({
                'MAC': mac,
                'Signal (dBm)': dev.current_rssi,
                'Packets': dev.packet_count,
                'Channel': dev.channel,
                'Distance (m)': f"{dist:.1f}" if dist else "N/A",
                'Strength': dev.get_strength()
            })
        
        df = pd.DataFrame(devices_data)
        st.dataframe(df, width='stretch', height=400)
        
        # MAC list for copying
        with st.expander("📋 Copy MAC List"):
            macs = "\n".join([d['MAC'] for d in devices_data])
            st.text_area("MACs:", value=macs, height=200, disabled=True)
    else:
        st.warning("Waiting for devices...")
    
    st.markdown("---")
    
    # Debug info
    with st.expander("🐛 Debug"):
        st.write(f"Lines read: {st.session_state.reader.lines_read}")
        st.write(f"Devices tracked: {len(analyzer.devices)}")
        st.write(f"History size: {len(analyzer.history)}")
        st.write(f"Last stats: {analyzer.stats}")
    
    # Auto refresh
    import time
    time.sleep(1)
    st.rerun()

else:
    st.warning("⚠️ Connect using the sidebar controls")
