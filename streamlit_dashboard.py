#!/usr/bin/env python3
"""
WiFi Sniffer Analysis Dashboard - Streamlit Version
Receives data from ESP32 and displays comprehensive WiFi network analytics
"""

import streamlit as st
import serial
import threading
import time
from collections import deque
import pandas as pd
import plotly.express as px

st.set_page_config(
    page_title="WiFi Sniffer Dashboard",
    page_icon="📡",
    layout="wide",
    initial_sidebar_state="expanded"
)

# ===== WiFi Device Class =====
class WiFiDevice:
    def __init__(self, mac):
        self.mac = mac
        self.rssi_history = deque(maxlen=60)
        self.packet_count = 0
        self.channel = 0
        self.probe_reqs = 0
        self.current_rssi = -100
        self.max_rssi = -100
        self.last_seen = time.time()  # Track when device was last seen
        
    def update(self, rssi, channel, packets, probe_reqs):
        self.current_rssi = int(rssi)
        self.max_rssi = max(self.max_rssi, self.current_rssi)
        self.rssi_history.append(self.current_rssi)
        self.channel = int(channel)
        self.packet_count = int(packets)
        self.probe_reqs = int(probe_reqs)
        self.last_seen = time.time()  # Update timestamp when device is seen
    
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
        elif self.current_rssi >= -80:
            return "Weak"
        else:
            return "Very Weak"

# ===== Analyzer Class =====
class WiFiNetworkAnalyzer:
    def __init__(self):
        self.devices = {}
        self.stats = {
            'ACT': 0, 'DEV': 0, 'PKT': 0, 'ATTACK': 0, 'TYPE': '',
            'BEA': 0, 'PRQ': 0, 'PRR': 0, 'DEA': 0
        }
        self.history = deque(maxlen=60)
        self.device_timeout = 45  # seconds - remove devices not seen for 45s
        
    def parse_line(self, line):
        """Parse ESP32 data line"""
        try:
            if not line.startswith('$DATA|'):
                return False
            
            data_str = line.replace('$DATA|', '').replace('|END', '')
            parts = data_str.split('|')
            
            # Parse stats and devices
            for part in parts:
                if part.startswith('DEVICES:'):
                    # Format: MAC,RSSI,Packets,Channel,ProbeReqs;MAC,...
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
                        # Special handling for ATTACK to ensure it's an int
                        if key == 'ATTACK':
                            self.stats[key] = int(val) if val.isdigit() else (1 if val == 'True' else 0)
                        else:
                            self.stats[key] = int(val) if val.isdigit() else val
                    except:
                        self.stats[key] = val
            
            # Remove stale devices and update unique device count
            self.cleanup_stale_devices()
            self.stats['DEV'] = len(self.devices)
            
            self.history.append(self.stats['ACT'])
            return True
        except Exception as e:
            return False
    
    def cleanup_stale_devices(self):
        """Remove devices not seen within the timeout period"""
        current_time = time.time()
        stale_devices = []
        
        for mac, device in self.devices.items():
            if current_time - device.last_seen > self.device_timeout:
                stale_devices.append(mac)
        
        for mac in stale_devices:
            del self.devices[mac]

# ===== Serial Reader Class =====
class SerialReader(threading.Thread):
    def __init__(self, port, baudrate=115200):
        super().__init__(daemon=True)
        self.port = port
        self.baudrate = baudrate
        self.analyzer = WiFiNetworkAnalyzer()
        self.running = False
        self.lines_read = 0
        self.data_lines = 0
        
    def run(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"✅ Connected to {self.port}")
            self.running = True
            
            while self.running:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.lines_read += 1
                        if self.analyzer.parse_line(line):
                            self.data_lines += 1
                time.sleep(0.01)
            
            self.ser.close()
        except Exception as e:
            print(f"Serial error: {e}")
    
    def stop(self):
        self.running = False
        try:
            if hasattr(self, 'ser'):
                self.ser.close()
        except:
            pass

# ===== Session State =====
if 'reader' not in st.session_state:
    st.session_state.reader = None
if 'connected' not in st.session_state:
    st.session_state.connected = False

# ===== Sidebar Controls =====
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
st.title("📡 WiFi Network Sniffer Dashboard")

if st.session_state.connected and st.session_state.reader:
    analyzer = st.session_state.reader.analyzer
    
    # Key Metrics
    col1, col2, col3, col4, col5 = st.columns(5)
    with col1:
        st.metric("Active Devices", analyzer.stats['ACT'])
    with col2:
        st.metric("Unique Devices", analyzer.stats['DEV'])
    with col3:
        st.metric("Total Packets", analyzer.stats['PKT'])
    with col4:
        st.metric("Beacons", analyzer.stats['BEA'])
    with col5:
        attack_value = int(analyzer.stats['ATTACK']) if isinstance(analyzer.stats['ATTACK'], (int, str)) else 0
        if attack_value:
            st.metric("⚠️ Status", "ATTACK!", delta="DETECTED", delta_color="inverse")
        else:
            st.metric("✅ Status", "Safe", delta_color="normal")
    
    st.markdown("---")
    
    # Graphs
    if len(analyzer.history) > 1:
        col1, col2 = st.columns(2)
        
        with col1:
            st.subheader("📈 Active Devices Over Time")
            df = pd.DataFrame({'Active Devices': list(analyzer.history)})
            fig = px.line(
                df, 
                y='Active Devices', 
                markers=True,
                template='plotly_dark',
                title="Device Count Timeline"
            )
            fig.update_layout(height=300, showlegend=False, hovermode='x unified')
            st.plotly_chart(fig, width='stretch')
        
        with col2:
            st.subheader("📊 Frame Statistics")
            frame_data = pd.DataFrame({
                'Type': ['Beacons', 'Probe Req', 'Probe Resp', 'Deauth'],
                'Count': [analyzer.stats['BEA'], analyzer.stats['PRQ'], analyzer.stats['PRR'], analyzer.stats['DEA']]
            })
            fig = px.bar(
                frame_data, 
                x='Type', 
                y='Count',
                template='plotly_dark',
                title="Frame Types Distribution"
            )
            fig.update_layout(height=300, showlegend=False)
            st.plotly_chart(fig, width='stretch')
    
    st.markdown("---")
    
    # All Detected Devices
    st.subheader("📡 All Detected Devices")
    
    if analyzer.devices:
        devices_data = []
        for mac, dev in sorted(analyzer.devices.items(), key=lambda x: x[1].current_rssi, reverse=True):
            dist = dev.get_distance()
            devices_data.append({
                'MAC Address': mac,
                'Signal (dBm)': dev.current_rssi,
                'Packets': dev.packet_count,
                'Channel': dev.channel,
                'Probe Reqs': dev.probe_reqs,
                'Distance (m)': f"{dist:.1f}" if dist else "N/A",
                'Strength': dev.get_strength()
            })
        
        df = pd.DataFrame(devices_data)
        st.dataframe(df, width='stretch', height=400)
        
        # MAC Address List
        with st.expander("📋 Copy MAC Address List"):
            macs = "\n".join([d['MAC Address'] for d in devices_data])
            st.text_area("All MAC Addresses (one per line):", value=macs, height=200, disabled=True)
    else:
        st.warning("⏳ Waiting for WiFi devices... Stand by.")
    
    st.markdown("---")
    
    # Attack Statistics
    col1, col2, col3, col4 = st.columns(4)
    with col1:
        st.metric("Deauth Attacks", analyzer.stats['DEA'])
    with col2:
        st.metric("Probe Requests", analyzer.stats['PRQ'])
    with col3:
        st.metric("Probe Responses", analyzer.stats['PRR'])
    with col4:
        attack_value = int(analyzer.stats['ATTACK']) if isinstance(analyzer.stats['ATTACK'], (int, str)) else 0
        if attack_value:
            st.metric("🚨 Alert", analyzer.stats['TYPE'], delta="Active")
        else:
            st.metric("✅ Alert", "None", delta="Normal")
    
    st.markdown("---")
    
    # Debug Panel
    with st.expander("🐛 Debug Information"):
        col1, col2, col3, col4 = st.columns(4)
        with col1:
            st.metric("Total Lines", st.session_state.reader.lines_read)
        with col2:
            st.metric("Data Lines", st.session_state.reader.data_lines)
        with col3:
            st.metric("Devices", len(analyzer.devices))
        with col4:
            st.metric("History Size", len(analyzer.history))
        
        st.write("**Latest Stats:**")
        st.json(analyzer.stats)
    
    # Auto-refresh
    time.sleep(1)
    st.rerun()

else:
    st.warning("⚠️ Connect to ESP32 using the sidebar controls to begin monitoring", icon="⚠️")
    
    with st.expander("🔧 Troubleshooting Guide"):
        st.markdown("""
        ### Connection Issues
        
        **1. No data appearing?**
        - Verify ESP32 is connected: `ls /dev/tty*`
        - Update serial port in sidebar (usually `/dev/ttyACM0` or `/dev/ttyACM1`)
        - Check ESP32 firmware is uploaded correctly
        
        **2. Port not found?**
        - Unplug ESP32 and plug back in
        - Try different USB cable
        - Check device manager for unknown devices
        
        **3. Debug dashboard working?**
        - If `http://localhost:8502` works but this doesn't, there's a parsing issue
        - Make sure firmware sends data in format: `$DATA|DEV:X|ACT:Y|...|END`
        
        **4. Terminal shows data but dashboard doesn't?**
        - Run: `python3 test_streamlit_data.py`
        - This will show what data is being received
        - Compare with expected format
        """)
