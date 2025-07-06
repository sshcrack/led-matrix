import socket
import time
import struct
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# UDP server configuration
UDP_IP = "127.0.0.1"
UDP_PORT = 5005

# Packet structure based on network.rs
# Header: 5 bytes
# - Magic number (2 bytes): 0xAD (Audio Data)
# - Version (1 byte): 0x01
# - Number of bands (1 byte): up to 255 bands
# - Flags (1 byte): bit flags for additional info
# Data: variable length
# - Timestamp (4 bytes): Unix timestamp in seconds as u32
# - Bands data (num_bands bytes): Each band as u8 (0-255)

class AudioPacket:
    def __init__(self, data):
        if len(data) < 9:
            raise ValueError("Packet too small")
            
        # Parse header
        self.magic = data[0:2]
        if self.magic != bytearray([0xAD, 0x01]):
            raise ValueError(f"Invalid magic number: {self.magic.hex()}")
            
        self.version = data[2]
        self.num_bands = data[3]
        self.flags = data[4]
        self.timestamp = struct.unpack("<I", data[5:9])[0]
        
        # Check if we have correct data length
        expected_length = 9 + self.num_bands
        if len(data) != expected_length:
            raise ValueError(f"Expected {expected_length} bytes, got {len(data)}")
            
        # Parse band data
        self.bands = np.array(data[9:], dtype=np.uint8)
        
        # Convert to float (0.0-1.0)
        self.bands_float = self.bands.astype(np.float32) / 255.0
        
        # Extract flags
        self.interpolated_log = bool(self.flags & 0x01)
    
    def __str__(self):
        return f"AudioPacket: {self.num_bands} bands, flags={self.flags}, timestamp={self.timestamp}"

# Setup socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.settimeout(0.1)  # Non-blocking with timeout

print(f"Listening for UDP messages on {UDP_IP}:{UDP_PORT}")

# Variables for FPS calculation
packet_count = 0
start_time = time.time()
last_display_time = start_time

# Setup visualization
plt.style.use('dark_background')
fig, ax = plt.subplots(figsize=(12, 6))
bars = ax.bar([], [], color='cyan')
ax.set_ylim(0, 1)
ax.set_title('Audio Spectrum Analyzer')
ax.set_xlabel('Frequency Band')
ax.set_ylabel('Amplitude')

# Latest packet data
latest_packet = None

def update_plot(frame):
    global packet_count, start_time, last_display_time, latest_packet
    
    try:
        # Try to receive data, but don't block indefinitely
        data, addr = sock.recvfrom(1024)
        packet_count += 1
        
        try:
            # Parse the packet
            latest_packet = AudioPacket(data)
            
            # Update the bars
            if len(bars) != latest_packet.num_bands:
                ax.clear()
                x = np.arange(latest_packet.num_bands)
                new_bars = ax.bar(x, np.zeros(latest_packet.num_bands), color='cyan')
                for bar in bars.patches:
                    bar.remove()
                bars.patches = new_bars.patches
                ax.set_ylim(0, 1)
                ax.set_title('Audio Spectrum Analyzer')
                ax.set_xlabel('Frequency Band')
                ax.set_ylabel('Amplitude')
            
            # Update bar heights
            for i, bar in enumerate(bars):
                bar.set_height(latest_packet.bands_float[i])
            
            # Add flag indicator
            flags_text = f"Interpolated & Log: {'Yes' if latest_packet.interpolated_log else 'No'}"
            ax.set_title(f'Audio Spectrum Analyzer - {flags_text}')
            
        except Exception as e:
            print(f"Error parsing packet: {e}")
            
        # Calculate and display FPS every second
        current_time = time.time()
        elapsed = current_time - last_display_time
        
        if elapsed >= 1.0:  # Update display every second
            fps = packet_count / (current_time - start_time)
            print(f"Received {packet_count} packets. FPS: {fps:.2f}")
            if latest_packet:
                print(f"Latest packet: {latest_packet}")
            last_display_time = current_time
            
    except socket.timeout:
        # No data received, just update the plot
        pass
        
    return bars

# Make sure we're using an interactive backend
import matplotlib
matplotlib.use('TkAgg')  # Use TkAgg backend which is interactive

# Start the animation with explicit save_count to avoid warning
ani = FuncAnimation(fig, update_plot, interval=16, blit=True, cache_frame_data=False)
plt.show()